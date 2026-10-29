"""Portable stock-bound native recipes. Generates analysis IMG, never FW."""
from pathlib import Path
import hashlib
import json
import os
import stat
import struct
from math import gcd
from . import fine_semantics, legacy_semantics
from codec import independent_decoder as decoder
from . import resource as b2y

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SCHEMA = 'm10-native-composition/v2'
OLD_SCHEMA = 'm10-native-composition/v1'
FINE = 'fine-grid-1/256'
LEGACY = 'five-power-of-two'
STOCK_SIZE = 9898708
STOCK_SHA = '393d3c13a13e88ef43b58a07585dce06ce667f7c596da849f41af4653d9a0460'
FW = (127615696, 'd6ef5b77b70451d2b6a31fc4a090bf8f664c1e07a8bebade657aff7da692423c')
ASC_FW = (121740584, 'aacf21aac8aa1cd1d82c281c5fa1092c158e0bd491d23d9a0ae5197d08c32463')
ORDINARY_B2Y = (1037488, '419c7d6635d1905146ee372967dc01c918a41fcca1d123d7f85cea0c5792c8a3')
ASC_B2Y = (None, '88e93114be60854bfc84ba00901f39774ae962eeffd216a54dcf9ba6bbca1896')
HYBRID = (1678832, '0ed4a05cc5b74c07108c0d7e6ab531a7a86a798420a246e2dafa8a1da1efcaf3')
TOC_SHA = '21b3028826022605b49eba564c9015e16e3d28bc7e0ebb7af68e50f11873cb5f'
INDEX_SHA = 'e2faf45c03fe45fdf64968d1e602b32d300e167744328e94b4a5a85365e2f5e3'
RECIPE_SHA = 'f00b17e06544f076197f2a87c23c621b375e177891d05767b0f91a651cdd6151'
EXPANDED = 135899932
DATA_BASE = 36 + 277*584 + 4
sha = lambda data: hashlib.sha256(data).hexdigest()
md5 = lambda data: hashlib.md5(data).hexdigest()

def canonical(value):
    return (json.dumps(value, sort_keys=True, indent=2, ensure_ascii=True, allow_nan=False)+'\n').encode('ascii')

def exact(value, keys):
    if type(value) is not dict or any(type(k) is not str for k in value) or set(value)!=set(keys):
        raise ValueError('exact object keys required: '+','.join(keys))

def parse(raw, limit=2000000, config=False):
    if type(raw) is not bytes or len(raw)>limit or (config and b'\\' in raw): raise ValueError('JSON size/escaped config')
    def pairs(items):
        result={}
        for k,v in items:
            if k in result: raise ValueError('duplicate JSON key')
            result[k]=v
        return result
    try: return json.loads(raw.decode('ascii'),object_pairs_hook=pairs,parse_constant=lambda _: (_ for _ in ()).throw(ValueError('nonfinite JSON')))
    except (UnicodeError,json.JSONDecodeError) as e: raise ValueError('ASCII JSON required') from e

def safe_path(path):
    p=Path(path)
    if '..' in p.parts: raise ValueError('parent traversal rejected')
    p=p.absolute()
    if any(x.is_symlink() for x in (p,*p.parents)): raise ValueError('symlink path rejected')
    return p

def regular(path, limit=256*1024*1024):
    p=safe_path(path)
    return decoder.regular_bytes(p,limit)

def pinned(raw, spec, name):
    if type(raw) is not bytes or (spec[0] is not None and len(raw)!=spec[0]) or sha(raw)!=spec[1]:
        raise ValueError(name+' exact official content pin mismatch')
    return raw

def destination(path):
    p=safe_path(path)
    if p.exists() or not p.parent.is_dir(): raise ValueError('fresh output with existing parent required')
    if p==ROOT or ROOT in p.parents: raise ValueError('generated output must be outside source checkout')
    if any('.fw' in part.lower() for part in p.parts): raise ValueError('firmware-like output path rejected')
    return p

def write(path, files):
    out=destination(path)
    out.mkdir(mode=0o700)
    for name,raw in sorted(files.items()):
        q=Path(name)
        if q.is_absolute() or '..' in q.parts: raise ValueError('internal output name')
        target=out/q;target.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
        with target.open('xb') as f: f.write(raw)
        if regular(target)!=raw: raise ValueError('output readback mismatch')
    return out

def default_config():
    return {'schema':SCHEMA,'components':[]}

def normalize(value):
    exact(value,('schema','components'))
    if type(value['schema']) is not str or value['schema'] not in (SCHEMA,OLD_SCHEMA): raise ValueError('explicit native schema required; source selection is not native')
    entries=value['components']
    if type(entries) is not list or len(entries)>3: raise ValueError('at most three native components')
    a={'component':'asc','enabled':False}
    w={'component':'wb','backend':LEGACY,'multipliers':None};seen=set()
    for item in entries:
        if type(item) is not dict or type(item.get('component')) is not str: raise ValueError('component name required')
        name=item['component']
        if name not in ('asc','wb','fine-wb'): raise ValueError('unsupported native feature: '+name)
        if name in seen or (name in ('wb','fine-wb') and seen.intersection(('wb','fine-wb'))): raise ValueError('duplicate or mutually exclusive WB components')
        seen.add(name)
        if name=='asc':
            exact(item,('component','enabled'))
            if type(item['enabled']) is not bool: raise ValueError('ASC enabled must be boolean')
            a=dict(item)
        else:
            backend=LEGACY if name=='wb' else FINE
            if value['schema']==OLD_SCHEMA:
                if name!='wb': raise ValueError('v1 cannot select fine-WB')
                exact(item,('component','multipliers'))
            else:
                exact(item,('component','backend','multipliers'))
                if type(item['backend']) is not str or item['backend']!=backend: raise ValueError('native WB backend mismatch')
            m=item['multipliers']
            if type(m) is str and m=='ORIGINAL': m=None
            normal,_=(fine_semantics if name=='fine-wb' else legacy_semantics).normalize({'schema_version':1,'multipliers':m})
            w={'component':name,'backend':backend,'multipliers':normal['multipliers']}
    return {'schema':SCHEMA,'components':[a,w]}

def config_file(path): return normalize(parse(regular(path,16384),16384,True))

def decode_official(path,spec):
    raw=pinned(regular(path),spec,'firmware')
    payload,_=decoder.decode_comp(raw)
    entries=decoder.inspect_toc(payload)
    return payload,entries

def extract(official_fw,output,asc_fw=None):
    destination(output)
    payload,index=decode_official(official_fw,FW)
    if len(payload)!=EXPANDED or len(index)!=277: raise ValueError('ordinary expanded/TOC profile')
    toc=bytes(payload[36:36+277*584]);pinned(toc,(277*584,TOC_SHA),'TOC')
    def body(idx,entries,data,path,pin):
        row=entries[idx]
        if row['path']!=path: raise ValueError('component path mismatch')
        return pinned(bytes(data[row['payload_offset']:row['payload_offset']+row['bytes']]),pin,path)
    stock=body(267,index,payload,'/system/system.bin',(STOCK_SIZE,STOCK_SHA))
    ordinary=body(247,index,payload,'/data/calib/B2Y.bin',ORDINARY_B2Y)
    index_raw=canonical(index)
    pinned(index_raw,(111383,INDEX_SHA),'official extracted index')
    files={'stock.IMG.bin':stock,'ordinary.B2Y.bin':ordinary,'TOC.records.bin':toc,'index.json':index_raw}
    if asc_fw is not None:
        asc_payload,asc_index=decode_official(asc_fw,ASC_FW)
        matches=[r for r in asc_index if r['path']=='/data/calib/B2Y.bin' and r['sha256']==ASC_B2Y[1]]
        if len(matches)!=1: raise ValueError('ASC unique B2Y profile')
        r=matches[0];asc=pinned(bytes(asc_payload[r['payload_offset']:r['payload_offset']+r['bytes']]),ASC_B2Y,'ASC B2Y')
        hybrid=pinned(b2y.encode(b2y.hybrid(b2y.parse(ordinary,42),b2y.parse(asc,47)),42),HYBRID,'hybrid B2Y')
        if len(b2y.parse(hybrid,42).entries)!=105: raise ValueError('hybrid complete catalog')
        files['hybrid.B2Y.bin']=hybrid
    receipt={'schema':'m10-portable-native-inputs/v1','source_fw_sha256':FW[1],'source_asc_fw_sha256':ASC_FW[1] if asc_fw is not None else None,'files':facts(files),'not_installable':True,'extracted_all_payloads_to_disk':False,'expanded_blob_retained':False}
    files['extraction.json']=canonical(receipt)
    write(output,files)
    return receipt

def facts(files): return {n:{'bytes':len(b),'sha256':sha(b)} for n,b in sorted(files.items())}

def bundle(path,asc=False):
    directory=safe_path(path)
    if not directory.is_dir(): raise ValueError('local extracted input directory required')
    stock=pinned(regular(directory/'stock.IMG.bin'),(STOCK_SIZE,STOCK_SHA),'stock IMG')
    ordinary=pinned(regular(directory/'ordinary.B2Y.bin'),ORDINARY_B2Y,'ordinary B2Y')
    toc=pinned(regular(directory/'TOC.records.bin'),(277*584,TOC_SHA),'TOC records')
    index_raw=pinned(regular(directory/'index.json'),(None,INDEX_SHA),'official index')
    index=parse(index_raw)
    if canonical(index)!=index_raw or type(index) is not list or len(index)!=277: raise ValueError('canonical official index')
    hybrid=pinned(regular(directory/'hybrid.B2Y.bin'),HYBRID,'required ASC hybrid') if asc else None
    # No caller-written extraction receipt or source path grants trust.
    return stock,ordinary,toc,index,hybrid

def recipes():
    raw=pinned(regular(HERE/'recipes.json'),(None,RECIPE_SHA),'authored recipes')
    return parse(raw)

def run_recipe(stock,r):
    off=r['offset'];n=r['bytes'];pinned(stock[off:off+n],(n,r['preimage_sha256']),'native window')
    out=bytearray()
    for op in r['program']:
        if 'emit' in op: exact(op,('emit',));out+=bytes.fromhex(op['emit'])
        else:
            exact(op,('stock','bytes'));start=op['stock'];length=op['bytes']
            if type(start) is not int or type(length) is not int or not off<=start<start+length<=off+n: raise ValueError('recipe stock copy extent')
            out+=stock[start:start+length]
    return pinned(bytes(out),(n,r['replacement_sha256']),'authored fragment')

def hword(v): return struct.pack('<H',v)

def branch(a,t):
    d=t-(a+4)
    if d%2 or not -2048<=d<=2046: raise ValueError('Thumb B range')
    return hword(0xe000|((d>>1)&0x7ff))

def blx(a,t):
    d=t-((a+4)&~3)
    if a%2 or t%4 or d%4 or not -(1<<22)<=d<(1<<22): raise ValueError('Thumb BLX extent')
    return struct.pack('<HH',0xf000|((d>>12)&0x7ff),0xe800|((d>>1)&0x7fe))

def patches(stock,config):
    pinned(stock,(STOCK_SIZE,STOCK_SHA),'stock IMG')
    db=recipes();segs=[];a,i,w=config['components']
    def add(component,name,offset,after,pre_pin=None):
        before=stock[offset:offset+len(after)]
        if pre_pin: pinned(before,(len(after),pre_pin),'native preimage')
        segs.append({'component':component,'name':name,'offset':offset,'bytes':len(after),'preimage_sha256':sha(before),'replacement_sha256':sha(after)})
        return (offset,after)
    edits=[]
    for r in db['recipes']:
        if a['enabled'] and r['name'].startswith('asc/'):
            edits.append(add('asc',r['name'],r['offset'],run_recipe(stock,r),r['preimage_sha256']))
    semantics=fine_semantics if w['component']=='fine-wb' else legacy_semantics
    _,values=semantics.normalize({'schema_version':1,'multipliers':w['multipliers']})
    if values is not None:
        name='wb/fine' if w['component']=='fine-wb' else 'wb/legacy'
        r=next(x for x in db['recipes'] if x['name']==name);after=bytearray(run_recipe(stock,r))
        if name=='wb/fine':
            common=gcd(values[0],gcd(values[1],values[2]));struct.pack_into('<3I',after,246,*(k//common for k in values))
        else:
            key=(values[1]-values[0],values[1]-values[2]);row=next(x for x in db['legacy_classes'] if (x['delta_R'],x['delta_B'])==key)
            for off,word in zip((108,122),row['shift_words']): struct.pack_into('<H',after,off,int(word,16))
            pinned(bytes(after),(258,row['fragment_sha256']),'legacy encoding class')
        edits.append(add('wb',name,r['offset'],bytes(after),r['preimage_sha256']))
    ordered=sorted(edits)
    if any(a[0]+len(a[1])>b[0] for a,b in zip(ordered,ordered[1:])): raise ValueError('overlapping native patches')
    image=bytearray(stock)
    for off,after in ordered:image[off:off+len(after)]=after
    return bytes(image),sorted(segs,key=lambda s:s['offset'])


def record_values(record):
    return {'component_type':hex(decoder.u32(record,0)),'relative_offset':hex(decoder.u32(record,24)),'bytes':decoder.u32(record,28),'selector':hex(decoder.u32(record,32)),'mode':hex(decoder.u32(record,40)),'serial_constraint':decoder.u32(record,44),'md5':record[8:24].hex(),'path':decoder._cstring(record[72:])}

def metadata(toc,index,image,hybrid):
    replacements={267:image}
    if hybrid is not None: replacements[247]=hybrid
    expected={267:(0x01020001,'/system/system.bin'),247:(0x0605000f,'/data/calib/B2Y.bin')}
    for ident in replacements:
        attr=record_values(toc[ident*584:(ident+1)*584])
        if (int(attr['component_type'],16),attr['path'],attr['selector'],attr['mode'],attr['serial_constraint'])!=(*expected[ident],'0x141','0x120',0): raise ValueError('replacement selector/path contract')
    old_cursor=new_cursor=DATA_BASE;records=[];planned=[];moved=[]
    for i,row in enumerate(index):
        original=toc[i*584:(i+1)*584];before=record_values(original)
        gap=row['payload_offset']-old_cursor
        if gap<0: raise ValueError('original TOC ordering')
        new_cursor+=gap;record=bytearray(original);struct.pack_into('<I',record,24,new_cursor-DATA_BASE)
        if i in replacements:
            data=replacements[i];record[8:24]=hashlib.md5(data).digest();struct.pack_into('<I',record,28,len(data))
        record=bytes(record);attr=record_values(record)
        if new_cursor!=row['payload_offset']: moved.append(i)
        original_mask=bytearray(original);mask=bytearray(record);original_mask[24:28]=mask[24:28]=b'\0'*4
        if i not in replacements and mask!=original_mask: raise ValueError('nonreplacement attributes changed')
        planned.append({'id':i,'record_offset':hex(36+i*584),'original_payload_offset':hex(row['payload_offset']),'planned_payload_offset':hex(new_cursor),'planned_end_exclusive':hex(new_cursor+attr['bytes']),'gap_before_B':gap,'gap_bytes_verified_this_build':False,'attributes':attr,'original_record_sha256':sha(original),'planned_record_sha256':sha(record),'payload_sha256':sha(replacements[i]) if i in replacements else row['sha256'],'payload_action':'actual replacement' if i in replacements else 'preserve original official payload','payload_replacement_verified_this_build':i in replacements,'nonlayout_record_properties_unchanged':i not in replacements,'nonlayout_record_sha256':sha(mask)})
        records.append(record);old_cursor=row['payload_offset']+before['bytes'];new_cursor+=attr['bytes']
    trailing=EXPANDED-old_cursor
    if trailing<0: raise ValueError('trailing original extent')
    expanded=new_cursor+trailing;copyback=expanded+56;span=(23<<21)*3
    plan={'schema':'m10-portable-native-metadata-plan/v1','source_fw_sha256':FW[1],'source_directory_sha256':TOC_SHA,'toc_count':277,'toc_record_stride_B':584,'toc_record_offset':'0x24','data_base':hex(DATA_BASE),'original_expanded_B':EXPANDED,'planned_expanded_B':expanded,'planned_copyback_total_B':copyback,'planned_directory_md5':md5(b''.join(records)),'planned_directory_sha256':sha(b''.join(records)),'replacement_ids':sorted(replacements),'passively_offset_migrated_ids':moved,'nonreplacement_payloads_and_nonlayout_properties_preserved':277-len(replacements),'gap_total_B':sum(r['gap_before_B'] for r in planned),'trailing_gap_B':trailing,'gap_policy':'copy opaque gap bytes only from pinned official source in any future payload builder; this is a plan','gap_byte_values_verified':False,'new_expanded_blob_constructed':False,'new_expanded_digest':None,'new_compressed_fw_B':None,'new_compressed_fw_sha256':None,'actual_new_outer_header_generated':False,'new_fw_created':False,'codec_admission_modified':False,'installable':False,'deployment_ready':False,'conditional_logical_geometry':{'source_version':'3.22.23.38','encoded_block_B':23<<21,'reserved_blocks':3,'span_B':span,'required_B':copyback,'headroom_B':span-copyback,'runtime_allocation_and_live_backing':'UNKNOWN'},'records':planned}
    return plan,records

def source_facts():
    names=['leica_source/native/'+n for n in ('portable.py','recipes.json','fine_semantics.py','legacy_semantics.py','fine_template.S','legacy_template.S','native_selectors.S','loader_failure.S')]+['codec/independent_decoder.py','leica_source/native/resource.py','leica_source/__main__.py','leica_source/native/__init__.py']
    return facts({n:regular(ROOT/n) for n in names})

def artifacts(inputs,config):
    config=normalize(config);stock,ordinary,toc,index,hybrid=bundle(inputs,config['components'][0]['enabled'])
    image,segs=patches(stock,config);plan,records=metadata(toc,index,image,hybrid)
    files={'IMG-System.NOT-INSTALLABLE.bin':image,'config.normalized.json':canonical(config),'metadata/section267.record.NOT-INSTALLABLE.bin':records[267]}
    if hybrid is not None:
        files['resources/B2Y.NOT-INSTALLABLE.bin']=hybrid
        # The section247 resource contract keeps its original pre-migration offset.
        contract=bytearray(toc[247*584:248*584]);contract[8:24]=hashlib.md5(hybrid).digest();struct.pack_into('<I',contract,28,len(hybrid))
        files['resources/section247.record.NOT-INSTALLABLE.bin']=bytes(contract)
    manifest={'schema':'m10-portable-native-manifest/v1','source_fw_sha256':FW[1],'source_img_sha256':STOCK_SHA,'source_img_bytes':STOCK_SIZE,'source_directory_sha256':TOC_SHA,'source_files':source_facts(),'config':config,'segments':segs,'output_img_sha256':sha(image),'output_img_md5':md5(image),'owned_window_bytes':sum(s['bytes'] for s in segs),'changed_bytes':sum(a!=b for a,b in zip(stock,image)),'files':facts(files),'installable':False,'deployment_ready':False,'target_executed_or_emulated':False,'compressed_firmware_generated':False,'encoder_admission_changed':False,'coverage':{'asc':'10 Classic;01 Contemporary;00/11 Normal via original two booleans; eight native windows / mandatory 105-entry B2Y hybrid','wb':'C628 metadata only; mutually exclusive fine-grid or legacy five-power-of-two','fine_extra_transient_stack_B':52,'fine_maximum_supported_product':33553408,'fine_loop_upper_bound':3876,'whole_task_stack_and_time_budget':'UNKNOWN','hardware_boot_recovery':'UNQUALIFIED','unsupported':['touch','film19','frame','boot','recovery']}}
    files['patch-manifest.json']=canonical(manifest);plan['source_composition_manifest_sha256']=sha(files['patch-manifest.json'])
    files['full-layout-plan.json']=canonical(plan)
    return manifest,files,plan

def build(inputs,config,output):
    destination(output);manifest,files,plan=artifacts(inputs,config_file(config));write(output,files)
    return {'not_installable':True,'image_sha256':manifest['output_img_sha256'],'image_md5':manifest['output_img_md5'],'directory_md5':plan['planned_directory_md5'],'files':facts(files)}

def verify(inputs,output):
    out=safe_path(output)
    if not out.is_dir(): raise ValueError('generated output directory required')
    config=config_file(out/'config.normalized.json');manifest,files,plan=artifacts(inputs,config)
    actual={}
    for path in out.rglob('*'):
        if path.is_symlink(): raise ValueError('symlink artifact rejected')
        if path.is_file(): actual[path.relative_to(out).as_posix()]=regular(path)
        elif not path.is_dir(): raise ValueError('nonregular artifact')
    if set(actual)!=set(files): raise ValueError('artifact inventory mismatch')
    for name,raw in files.items():
        if actual[name]!=raw: raise ValueError('artifact differs from fully regenerated result: '+name)
    return {'verified':True,'image_sha256':manifest['output_img_sha256'],'directory_md5':plan['planned_directory_md5'],'files_verified':len(files),'installable':False}

def reverse(inputs,input_dir,output):
    destination(output);receipt=verify(inputs,input_dir)
    config=config_file(safe_path(input_dir)/'config.normalized.json');stock,_,_,_,_=bundle(inputs,config['components'][0]['enabled'])
    image=regular(safe_path(input_dir)/'IMG-System.NOT-INSTALLABLE.bin');expected,segs=patches(stock,config)
    if image!=expected: raise ValueError('reverse image mismatch')
    result=bytearray(image)
    for seg in segs:
        off=seg['offset'];n=seg['bytes'];result[off:off+n]=stock[off:off+n]
    pinned(bytes(result),(STOCK_SIZE,STOCK_SHA),'reversed stock')
    receipt={'schema':'m10-portable-native-reverse/v1','verified_output':receipt,'stock_sha256':sha(result),'stock_bytes':len(result),'entire_image_equal_to_stock':bytes(result)==stock,'installable':False}
    write(output,{'IMG-System.REVERSED-NOT-INSTALLABLE.bin':bytes(result),'reverse-receipt.json':canonical(receipt)})
    return receipt
