"""Real accepted modules executed on ordinary host memory, never camera firmware.

Each invocation validates and freezes one shot configuration. Native bindings
remain closed. C host callback fixtures are labelled and kept under host/.
"""
import ctypes as C
import hashlib
import importlib.util
import json
import os
from pathlib import Path
from fractions import Fraction
import shutil
import struct
import subprocess
import sys
from .config import ROOT, STYLE_FILMS, canonical, selected_modules, validate

class MissingInput(ValueError): pass
GAMMA_SHA='2c8f21661161934be5b419e613dca60b9c2b9ba7eafec747c0afdbaf3077ae0f'
ASC_SHA='0ed4a05cc5b74c07108c0d7e6ab531a7a86a798420a246e2dafa8a1da1efcaf3'
SOURCES={
 'touch':['touch_repair.c'],
 'framing':['lvf.c'],
 'controller':['style_framing.c','film_closed.c','../asc/m10_asc.c','../framing/lvf.c','../framing/m10_gui_backend.c'],
 'asc':['m10_asc.c','closed_imports.c'],
 'wb':['host/wb_metadata.c']}

def compiler():
    name=os.environ.get('CC','cc'); found=shutil.which(name)
    if not found: raise MissingInput('C11 host compiler required (set CC; clang/gcc). No target execution is used.')
    return found

def library(module, build_dir):
    directory=ROOT/'modules'/module
    if not directory.is_dir(): raise MissingInput('module not present in selected source bundle: '+module)
    output=Path(build_dir); output.mkdir(parents=True,exist_ok=True)
    suffix='.dll' if sys.platform=='win32' else '.dylib' if sys.platform=='darwin' else '.so'
    sources=[directory/s for s in SOURCES[module]]
    members=sources+[p for d in {s.parent for s in sources} for p in d.rglob('*.h')]+[ROOT/'host'/module/'reference.c']
    fingerprint=hashlib.sha256(b''.join(p.read_bytes() for p in sorted(members))).hexdigest()[:16]
    target=output/('reference_'+module+'_'+fingerprint+suffix)
    if target.is_file(): return C.CDLL(str(target.resolve()))
    args=[compiler(),'-std=c11','-O1','-shared']
    if sys.platform!='win32': args+=['-fPIC']
    args+=['-I'+str(directory),'-I'+str(directory/'host'),'-I'+str(ROOT/'modules/asc'),'-I'+str(ROOT/'modules/framing')]
    args += [str(directory/s) for s in SOURCES[module]]+[str(ROOT/'host'/module/'reference.c'),'-o',str(target)]
    run=subprocess.run(args,capture_output=True,text=True)
    if run.returncode: raise MissingInput('host compile failed: '+run.stderr)
    return C.CDLL(str(target.resolve()))

def compile_sources(build_dir):
    output=Path(build_dir); output.mkdir(parents=True,exist_ok=True); built=[];arm_objects=[]
    for source in sorted((ROOT/'modules').rglob('*.c')):
        target=output/('_'.join(source.relative_to(ROOT/'modules').parts)+'.o')
        args=[compiler(),'-std=c11','-O1','-c','-I'+str(source.parent),str(source),'-o',str(target)]
        if source.name=='m10_native.c':
            clang=shutil.which('clang')
            if not clang:raise MissingInput('m10_native.c requires pointer32: install an ARM-capable clang for compile-only checking; its ABI guard is never removed for host builds')
            args[0]=clang;args[1:1]=['--target=armv5te-none-eabi','-mthumb','-ffreestanding','-nostdlib']
            arm_objects.append(source.relative_to(ROOT).as_posix())
        r=subprocess.run(args,capture_output=True,text=True)
        if r.returncode: raise MissingInput('compile failed: '+source.name+'\n'+r.stderr)
        built.append(source.relative_to(ROOT).as_posix())
    return {'C_objects':len(built),'host_C_objects':len(built)-len(arm_objects),'compile_only_ARM_objects':arm_objects,'sources':built,'native_linked':False,'target_executed':False,
            'unresolved_native_imports':'Explicitly retained; compile-only objects do not qualify a loader or ABI.'}

def import_file(path,name):
    spec=importlib.util.spec_from_file_location(name,path); module=importlib.util.module_from_spec(spec)
    sys.modules[name]=module
    previous=sys.dont_write_bytecode
    # Frozen external references are read-only, including their cache directory.
    # Also works when the caller forgot -B; public code never changes that tree.
    sys.dont_write_bytecode=True
    try:spec.loader.exec_module(module)
    finally:sys.dont_write_bytecode=previous
    return module

def gamma_file(path):
    if path is None: raise MissingInput('GRKit recipes require --gamma: exact local 256-entry base_gamma; its redistribution license is unresolved, so no substitute is bundled.')
    data=json.loads(Path(path).read_text())
    if type(data) is dict: data=data.get('base_gamma')
    if type(data) is not list or len(data)!=256 or any(type(v) is not int or not 0<=v<=16384 for v in data): raise MissingInput('invalid local gamma array')
    if hashlib.sha256(struct.pack('>256H',*data)).hexdigest()!=GAMMA_SHA: raise MissingInput('exact local GRKit gamma hash mismatch')
    return data

def film_rgb(look,rgb,gamma=None):
    if len(rgb)!=3 or any(type(v) is not int or not 0<=v<=255 for v in rgb): raise ValueError('RGB must contain three integers 0..255')
    if look=='800t':
        m=import_file(ROOT/'modules/film/frame800t.py','leica_frame800t')
        return {'rgb8':m.current_800t_rgb(tuple(rgb)), 'domain':'FRAME800T software recipe, RGB8 code input; excludes spatial/grain/halation stages', 'license':'GPL-2.0-only'}
    pins={r['path']:r['public_sha256'] for r in json.loads((ROOT/'provenance.json').read_text())['files']}
    def checked(relative):
        try:data=(ROOT/'modules/film'/relative).read_bytes()
        except OSError as e:raise MissingInput('missing recipe dependency: '+relative) from e
        if hashlib.sha256(data).hexdigest()!=pins.get('modules/film/'+relative):raise MissingInput('recipe dependency integrity mismatch: '+relative)
        return data
    catalog=json.loads(checked('recipe_catalog.json'))
    record=next((r for r in catalog if r['id']==look),None)
    if record is None:raise MissingInput('unknown actual GRKit recipe: '+look)
    for dependency in record['dependencies']:checked(dependency)
    m=import_file(ROOT/'modules/film/recipe_engine.py','leica_recipe_engine')
    raw=checked(record['recipe'])
    if hashlib.sha256(raw).hexdigest()!=record['recipe_sha256']:raise MissingInput('recipe integrity mismatch')
    effect=json.loads(raw)
    if effect['attribution']['license']!=record['licensing']['recipe_data']:raise MissingInput('recipe license mismatch')
    def srgb_decode(v):return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4
    linear=tuple(srgb_decode(v/255) for v in rgb)
    result=m.Model(effect,gamma_file(gamma)).render(linear)
    return {'display_rgb':result,'input_rgb8':rgb,'linear_input':linear,
            'domain':'sRGB-shaper decode -> scene-linear sRGB primaries -> exact GRKit source model -> display code',
            'code_license':'MIT','effect_license':record['licensing']['recipe_data'],'recipe_id':look,'monochrome':bool(effect['flags']),'gamma':'user local exact hash; not distributed'}

def run(config,build_dir,*,rgb=(128,128,128),gamma=None,asc_resource=None,touch_state=0,touch_event=0x113,touch_raw=0):
    shot=validate(config); f=shot['features'];style=f['styles']['default']
    if f['jpeg_crop'] or f['low_noise_shutter']: raise MissingInput('Selected JPEG crop/low-noise shutter has no executable implementation; selection can be exported for development but cannot run.')
    # Validate all required local data before executing selected components.
    if style in STYLE_FILMS and style!='cinestill800t': gamma_file(gamma)
    resource=None
    if style.startswith('asc_'):
        if asc_resource is None: raise MissingInput('ASC requires --asc-resource: locally prepared, exact combined B2Y resource. Use prepare-asc or extract-asc; no mock renderer is substituted.')
        resource=Path(asc_resource).read_bytes()
        if len(resource)!=1678832 or hashlib.sha256(resource).hexdigest()!=ASC_SHA: raise MissingInput('ASC combined resource hash/length mismatch')
    result={'scope':'HOST reference execution; not camera qualification or firmware', 'config_sha256':hashlib.sha256(canonical(shot)).hexdigest(),'shot_config':shot,'modules':{},'firmware_available':False}
    for module in selected_modules(shot):
        if module=='film':
            if style in STYLE_FILMS:result['modules']['film']=film_rgb(STYLE_FILMS[style],list(rgb),gamma)
            continue
        if module=='asc' and not style.startswith('asc_'):continue
        if module=='controller':continue  # invoked once below with the same frozen shot
        if module=='framing':continue
        lib=library(module,build_dir)
        if module=='touch':
            out=(C.c_uint32*7)(); lib.reference_touch(C.c_uint(touch_state),C.c_uint(touch_event),C.c_int(touch_raw),out)
            result['modules']['touch']=dict(zip(('outcome','submits','payload_frees','debug_calls','retained_envelope','retained_payload','prepares'),out))
        elif module=='wb':
            fractions=[Fraction(x) for x in f['wb']['gains']]; gains=(C.c_uint32*6)(*[v for q in fractions for v in (q.numerator,q.denominator)])
            original=struct.pack('<6I',1,2,1,1,3,2); out=(C.c_ubyte*24)()
            status=lib.reference_wb(gains,original,out)
            if status: raise ValueError('WB reference failed: '+str(status))
            result['modules']['wb']={'original_neutral_hex':original.hex(),'adjusted_neutral_hex':bytes(out).hex(),'pairs':list(struct.unpack('<6I',bytes(out))),'shot_config_frozen_before_future_reset':True,'full_dng_file_modified':False}
        elif module=='asc':
            out=(C.c_uint32*50)(); look=1 if style=='asc_classic' else 2
            status=lib.reference_asc(resource,C.c_uint32(len(resource)),C.c_uint(look),C.c_uint(2),out)
            if status: raise ValueError('ASC real-resource plan rejected: '+str(status))
            result['modules']['asc']={'resource_sha256':ASC_SHA,'gamma_count':out[0],'control_mask':out[1],'control_value':out[2],'cc1_fields':[list(out[3+i*3:6+i*3]) for i in range(9)],'gamma_full_and_consumed_bytes':[list(out[30+i*2:32+i*2]) for i in range(out[0])],'production_closed_status':lib.reference_asc_closed(C.c_uint(look)),'pixel_rendered':False}
    if 'controller' in selected_modules(shot):
        result['modules']['controller']=controller_run(shot,build_dir,resource)
        if f['frame_guides']['enabled']:
            result['modules']['framing']=result['modules']['controller']['framing']
    return result

def controller_run(shot,build_dir,resource=None,fail_gui=False):
    f=validate(shot)['features'];style=f['styles']['default'];code={'normal':0,'asc_classic':1,'asc_contemporary':2}.get(style,3)
    lib=library('controller',build_dir);out=(C.c_int32*48)()
    status=lib.reference_controller(C.c_uint(code),C.c_uint(f['frame_guides']['enabled']),C.c_uint(f['frame_guides']['preset']),resource,C.c_uint32(len(resource) if resource else 0),C.c_uint(fail_gui),out)
    if status:raise ValueError('actual style/framing controller failed host setup: '+str(status))
    guides=f['frame_guides']['enabled'];draw_ok=(bool(out[4]&2) or bool(out[7]&2))
    return {'api':'sfc_freeze -> sfc_apply -> sfc_restore','frozen_style_id':style,'native_film_recipe_bound':False,'style_status':out[0],'native_style_status':out[1],'native_style_stage':out[2],
            'attempted':out[3],'completed':out[4],'needs_restore_after_apply':out[5],'independent_guides_status':out[6],
            'restore_status':out[32],'needs_restore_after_close':out[33],'open_after_close':bool(out[36]),'normal_after_restore':out[37]==0,
            'host_consumed_bytes_apply':out[38],'host_consumed_bytes_after_restore':out[39],
            'boundary':'actual accepted C cores/backend with explicit HOST resource/GUI callbacks; no native qualification','host_callbacks':True,'production_imports_closed':True,
            'framing':{'nominal_gate':list(out[8:12]),'strict_aperture':list(out[12:16]),'pixel_lines':[list(out[16+4*i:20+4*i]) for i in range(4)],'line_apply_calls':4 if draw_ok and guides else 0,'restored':bool(out[34]) if guides else not out[36],
                       'backend':'actual sfc/lvf/u16-M10 backend, synthetic HOST ordinary memory','jpeg_cropped':False}}

def source_eval(style='all',rgb=(128,128,128),gamma=None):
    if not (ROOT/'modules/film').is_dir():raise MissingInput('Film recipe module is absent from this source bundle')
    styles=list(STYLE_FILMS) if style=='all' else [style]
    if any(s not in STYLE_FILMS for s in styles):raise MissingInput('Source pixel evaluation supports film recipes only; NORMAL/ASC need resource plans')
    return {s:film_rgb(STYLE_FILMS[s],list(rgb),gamma) for s in styles}

def prepare_asc(ordinary,asc,output):
    a=Path(ordinary).read_bytes(); b=Path(asc).read_bytes()
    if hashlib.sha256(a).hexdigest()!='419c7d6635d1905146ee372967dc01c918a41fcca1d123d7f85cea0c5792c8a3': raise MissingInput('ordinary B2Y pin mismatch')
    if hashlib.sha256(b).hexdigest()!='88e93114be60854bfc84ba00901f39774ae962eeffd216a54dcf9ba6bbca1896': raise MissingInput('ASC B2Y pin mismatch')
    m=import_file(ROOT/'modules/asc/b2y.py','leica_b2y'); data=m.encode(m.hybrid(m.parse(a,42),m.parse(b,47)),42)
    if len(data)!=1678832 or hashlib.sha256(data).hexdigest()!=ASC_SHA: raise MissingInput('combined ASC resource pin mismatch')
    p=Path(output)
    if p.suffix.lower() not in ('.host-resource','.host-fixture'): raise MissingInput('local analysis output must end .host-resource or .host-fixture; not firmware')
    with p.open('xb') as stream: stream.write(data)
    return dict(bytes=len(data),sha256=ASC_SHA,scope='Local host resource only. Native memory/loader not bound; never an update file.')
