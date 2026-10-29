/** Offline COMP reference rebuilder. Host data transformations only. */
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
export const HERE=path.dirname(fileURLToPath(import.meta.url));
// Local official input is mandatory; no private workspace or bundled firmware.
let outputDirectory=HERE;
export const PIN='d6ef5b77b70451d2b6a31fc4a090bf8f664c1e07a8bebade657aff7da692423c';
export const IMG_PIN='393d3c13a13e88ef43b58a07585dce06ce667f7c596da849f41af4653d9a0460';
export const LIMIT=256*1024*1024;
export const digest=(b,a='sha256')=>crypto.createHash(a).update(b).digest('hex');
export function need(v,s){if(!v)throw Error(s);}
export function readRegular(p,limit=LIMIT){
  need(fs.lstatSync(p).isFile(),'regular non-symlink file required');
  const fd=fs.openSync(p,fs.constants.O_RDONLY|(fs.constants.O_NOFOLLOW??0)|(fs.constants.O_NONBLOCK??0));
  try{const a=fs.fstatSync(fd);need(a.isFile()&&a.size>0&&a.size<=limit,'regular bounded file required');
    const b=fs.readFileSync(fd);const z=fs.fstatSync(fd);
    need(b.length===a.size&&a.ino===z.ino&&a.dev===z.dev&&a.size===z.size&&a.mtimeMs===z.mtimeMs,'file changed during read');return b;
  }finally{fs.closeSync(fd);}
}
export function u32(b,o){need(Number.isSafeInteger(o)&&o>=0&&o+4<=b.length,'u32 bounds');return b.readUInt32LE(o);}
function text(b){const z=b.indexOf(0);need(z>=0,'TOC text terminator');return b.subarray(0,z).toString('ascii');}
export function outer(b){
  need(b.length>=56&&b.length<=LIMIT,'outer length');
  need(u32(b,0)===0x1e1caf3e&&u32(b,52)===0xe1e350c1,'outer magic');
  need(u32(b,36)>=4&&u32(b,40)===56,'outer header version/extent');
  need(u32(b,12)+56===b.length&&u32(b,16)>0&&u32(b,16)<=LIMIT,'outer dimensions');
  need(digest(b.subarray(56),'md5')===b.subarray(20,36).toString('hex'),'outer MD5');
  return {bytes:b.length,sha256:digest(b),version:[...b.subarray(4,8)],type:u32(b,8),hardware:u32(b,48),compression:u32(b,44),expanded_bytes:u32(b,16),header_hex:b.subarray(0,56).toString('hex')};
}
function tables(b){
  need(b.length>=408,'COMP table bounds');const perm=b.subarray(140,396);
  need(new Set(perm).size===256,'COMP permutation');
  const values=new Uint8Array(65536),lens=new Uint8Array(65536),spec=[];
  for(let c=0;c<16;c++){
    const n=b.readUInt16LE(108+2*c),code=b.readUInt16LE(76+2*c);
    if(n){need(c<=8&&n<=16&&n+c<=16,'COMP prefix bounds');spec.push([c,n,code]);}
  }
  for(let w=0;w<65536;w++){
    for(const [c,n,code] of spec){if((w>>>(16-n))!==(code>>>(16-n)))continue;
      let v=c?(w>>>(16-n-c))&((1<<c)-1):0;
      if(c&&v<(1<<(c-1)))v-=(1<<c)-1;
      values[w]=perm[(128+v)&255];lens[w]=n+c;break;
    }need(lens[w]>0,'COMP incomplete prefix code');
  }
  // Canonical signed amplitude -128..127; no stream bytes are copied here.
  const enc=new Array(256);
  for(let i=0;i<256;i++){
    const v=i-128,c=v===0?0:Math.floor(Math.log2(Math.abs(v)))+1;
    const n=b.readUInt16LE(108+2*c),base=b.readUInt16LE(76+2*c);
    need(n>0&&n+c<=16,'COMP byte lacks canonical code');
    const amp=v<0?v+(1<<c)-1:v;
    const code=(base>>>(16-n))*(1<<c)+amp,len=n+c,w=code<<(16-len);
    need(values[w]===perm[i]&&lens[w]===len,'COMP encoder/decoder code disagreement');enc[perm[i]]={code,len};
  }
  return {values,lens,enc};
}
export function decodeComp(b){
  const h=outer(b);need(h.compression===2&&b.subarray(56,60).equals(Buffer.from('COMP')),'COMP format required');
  const t=tables(b),count=u32(b,400),start=408+count*16;
  need(count>=1&&count<=64&&start<=b.length&&b.subarray(404,408).equals(Buffer.from('PMOC')),'COMP directory');
  const out=Buffer.alloc(h.expanded_bytes),blocks=[];let dst=0,prev=start-56;
  for(let i=0;i<count;i++){
    const p=408+i*16,r=u32(b,p),rel=u32(b,p+4),size=u32(b,p+8),want=u32(b,p+12);
    need(r===0&&rel===prev&&size>0&&want>0&&56+rel+size<=b.length&&dst+want<=out.length,'COMP block bounds/coverage');
    const src=b.subarray(56+rel,56+rel+size);let bit=0;
    for(let j=0;j<want;j++){
      const a=Math.floor(bit/8),s=bit&7;
      need(a<src.length,'COMP truncated block');
      const w=((((src[a]??0)<<16)|((src[a+1]??0)<<8)|(src[a+2]??0))>>>(8-s))&65535,n=t.lens[w];
      need(bit+n<=size*8,'COMP truncated symbol');out[dst+j]=t.values[w];bit+=n;
    }
    need(size*8-bit<=(i===count-1?256:31),'COMP trailing bits bound');
    blocks.push({index:i,offset:dst,expanded:want,relative:rel,packed:size,bits:bit,padding_bits:size*8-bit,source:src});
    dst+=want;prev=rel+size;
  }
  need(dst===out.length&&prev+56===b.length,'COMP total coverage');
  return {header:h,payload:out,blocks,template:b.subarray(0,start),enc:t.enc};
}
function encodeBlock(data,enc,old){
  let bits=0;for(const v of data)bits+=enc[v].len;
  const sameBits=bits===old.bits;
  // Observed COMP block lengths are 32-bit aligned. The decoder consumes want
  // symbols, then admits <=31 suffix bits (last block <=256). Reuse every
  // original suffix bit on an unchanged bit extent; otherwise align to 32 bits
  // with zero suffix bits, recording that explicit encoding choice.
  const packed=sameBits?old.packed:Math.ceil(bits/32)*4;
  need(packed>0&&packed<=LIMIT,'COMP encoded block size bound');
  const out=Buffer.alloc(packed);let acc=0,n=0,p=0;
  for(const v of data){const e=enc[v];acc=acc*(1<<e.len)+e.code;n+=e.len;
    while(n>=8){n-=8;out[p++]=Math.floor(acc/(2**n))&255;acc%=2**n;}
  }
  if(n)out[p]=acc<<(8-n);
  if(sameBits){
    const a=Math.floor(bits/8),shift=bits&7;
    if(shift){const mask=(1<<(8-shift))-1;out[a]|=old.source[a]&mask;old.source.copy(out,a+1,a+1);}
    else old.source.copy(out,a,a);
  }
  return {data:out,bits,encoded_symbols:data.length,padding_bits:packed*8-bits,padding_policy:sameBits?'preserve-original-suffix-bits':'zero-pad-to-32-bit-boundary'};
}
export function encodeComp(decoded,payload){
  need(payload.length>0&&payload.length<=LIMIT,'COMP encoded expanded bound');
  const prefix=Buffer.from(decoded.template),parts=[prefix],audit=[];let relative=prefix.length-56;
  const fixedPrefix=decoded.blocks.slice(0,-1).reduce((s,b)=>s+b.expanded,0);
  need(payload.length>fixedPrefix,'COMP payload must cover preserved nonfinal blocks');
  for(const block of decoded.blocks){
    const want=block.index===decoded.blocks.length-1?payload.length-fixedPrefix:block.expanded;
    const e=encodeBlock(payload.subarray(block.offset,block.offset+want),decoded.enc,block);
    need(relative+e.data.length+56<=LIMIT,'COMP encoded container bound');
    const at=408+block.index*16;prefix.writeUInt32LE(relative,at+4);prefix.writeUInt32LE(e.data.length,at+8);prefix.writeUInt32LE(want,at+12);
    parts.push(e.data);audit.push({index:block.index,encoded_symbols:e.encoded_symbols,meaningful_bits:e.bits,padding_bits:e.padding_bits,padding_policy:e.padding_policy,packed_bytes:e.data.length,source_packed_bytes:block.packed,sha256:digest(e.data),source_packed_identical:e.data.equals(block.source)});relative+=e.data.length;
  }
  const raw=Buffer.concat(parts);raw.writeUInt32LE(raw.length-56,12);raw.writeUInt32LE(payload.length,16);
  Buffer.from(digest(raw.subarray(56),'md5'),'hex').copy(raw,20);
  return {raw,audit};
}
export function parseToc(payload){
  need(payload.length>=40&&u32(payload,0)===0x70c4ead3&&u32(payload,32)===0x8f3b152c,'TOC magic');
  const count=u32(payload,20),size=u32(payload,24),off=u32(payload,28);
  need(count>=1&&count<=4096&&size===count*584&&off===36&&off+size+4<=payload.length,'TOC dimensions');
  need(u32(payload,off+size)===0xda7ada7a,'TOC terminator');
  need(digest(payload.subarray(off,off+size),'md5')===payload.subarray(4,20).toString('hex'),'TOC directory MD5');
  const base=off+size+4,entries=[];
  for(let i=0;i<count;i++){
    const at=off+i*584,rec=payload.subarray(at,at+584),rel=u32(rec,24),len=u32(rec,28),start=base+rel;
    need(len>0&&start>=base&&start+len<=payload.length,'TOC component bounds');
    const data=payload.subarray(start,start+len);need(digest(data,'md5')===rec.subarray(8,24).toString('hex'),'TOC component MD5');
    const label=rec.subarray(52,72),z=label.indexOf(0);
    entries.push({index:i,name:label.subarray(0,z<0?20:z).toString('ascii'),path:text(rec.subarray(72)),relative:rel,offset:start,bytes:len,md5:digest(data,'md5'),sha256:digest(data),record_sha256:digest(rec),record:rec,data});
  }
  const spans=entries.map(e=>[e.offset,e.offset+e.bytes]).sort((a,b)=>a[0]-b[0]);
  for(let i=1;i<spans.length;i++)need(spans[i-1][1]<=spans[i][0],'TOC component overlap');
  return {count,size,off,base,entries};
}
export function rebuildToc(payload,toc){
  // Preserve every unknown field, payload gap and trailer, then actually rebuild
  // every complete record and body and recompute all stored digest fields.
  const out=Buffer.from(payload),directory=Buffer.alloc(toc.count*584);
  for(const e of toc.entries){const rec=Buffer.from(e.record);Buffer.from(digest(e.data,'md5'),'hex').copy(rec,8);
    rec.writeUInt32LE(e.relative,24);rec.writeUInt32LE(e.bytes,28);rec.copy(directory,e.index*584);e.data.copy(out,toc.base+e.relative);}
  out.writeUInt32LE(toc.count,20);out.writeUInt32LE(directory.length,24);out.writeUInt32LE(toc.off,28);
  directory.copy(out,toc.off);Buffer.from(digest(directory,'md5'),'hex').copy(out,4);
  return out;
}
function safeEntry(e){return Object.fromEntries(Object.entries(e).filter(([k])=>!['record','data'].includes(k)));}
export function compare(a,b){
  const da=decodeComp(a),db=decodeComp(b),ta=parseToc(da.payload),tb=parseToc(db.payload);
  const rows=ta.entries.map((e,i)=>{const z=tb.entries[i];return {index:i,name:e.name,before:safeEntry(e),after:z?safeEntry(z):null,metadata_584_bytes_identical:!!z&&e.record.equals(z.record),component_bytes_identical:!!z&&e.data.equals(z.data)};});
  let differing=0,first=null;const ranges=[];let begin=null;
  for(let i=0;i<Math.max(a.length,b.length);i++){
    if(a[i]!==b[i]){differing++;if(first===null)first=i;if(begin===null)begin=i;}
    else if(begin!==null){if(ranges.length<64)ranges.push([begin,i]);begin=null;}
  }if(begin!==null&&ranges.length<64)ranges.push([begin,Math.max(a.length,b.length)]);
  return {schema:'leica.reference-component-diff/v1',before:da.header,after:db.header,components_before:ta.count,components_after:tb.count,components:rows,
    all_component_metadata_identical:ta.count===tb.count&&rows.every(r=>r.metadata_584_bytes_identical),all_component_bytes_identical:ta.count===tb.count&&rows.every(r=>r.component_bytes_identical),
    expanded_payload_identical:da.payload.equals(db.payload),expanded_payload_sha256:digest(db.payload),outer_header_identical:a.subarray(0,56).equals(b.subarray(0,56)),
    package_byte_identical:a.equals(b),different_file_bytes:differing,first_difference:first,difference_ranges_first_64:ranges};
}
export function qualification(){return {schema:'leica.reference-qualification/v1',artifact_class:'offline-reference-reconstruction',deployment_ready:false,device_accepted:false,digital_signature_verified:false,
  integrity_scope:'MD5 validates compressed body, directory and components; it is not a digital signature or boot trust proof.',
  blockers:[{id:'NO_ADMITTED_FUNCTIONAL_PATCH',reason:'No approved M10 4.26.36.10 patch bytes, owned executable extent, complete ABI and feature integration exist.'},
    {id:'MAIN_BOOT_TRUST_UNKNOWN',reason:'Actual primary /system/system.bin boot consumer, accepted length/entry, authenticity and fallback remain unidentified.'},
    {id:'ORDINARY_SAME_VERSION_GATE',reason:'9027 ordinary updater rejects versions <= installed version. This reference retains 4.26.36.10 and cannot be offered as an upgrade on that baseline.'},
    {id:'RECOVERY_UNQUALIFIED',reason:'No complete main persistent backup and demonstrated primary-IMG failure recovery are available.'}],
  fw_info:'Non-CAL update rewrites the received outer header + full received TOC, without observed old-directory merge. All 277 records are retained here.',
  mutation_policy:'No arbitrary hook, model/serial/type/version rewrite, CALIBRATION substitution, transport, flash or target execution.'};}
export function replacementReasons(manifest){
  const reasons=[];
  if(!manifest||typeof manifest!=='object'||Array.isArray(manifest))return ['replacement manifest object required'];
  if(manifest.schema!=='leica.admitted-main-img-patch/v1')reasons.push('unknown replacement contract schema');
  if(manifest.model!=='M10'||manifest.version!=='4.26.36.10'||manifest.base_package_sha256!==PIN||manifest.base_img_sha256!==IMG_PIN)reasons.push('replacement must bind exact M10 4.26.36.10 package and main-IMG SHA profile; M10-P is not interchangeable');
  for(const k of ['patches','abi_evidence','owned_code_extents','loader_evidence','independent_admission'])if(!manifest[k])reasons.push(`missing ${k}`);
  if(manifest.replacement_format==='ELF'||manifest.strategy==='append')reasons.push('ELF append is not a raw admitted IMG patch; EOF load coverage and executable ownership are unqualified');
  reasons.push('no independent admission is registered in this reference-only builder; caller-supplied claims cannot authorize changed IMG');
  return reasons;
}
function writeNew(name,data){
  need(/^[A-Za-z0-9._-]+$/.test(name)&&!name.endsWith('.FW'),'local analysis/reference filename required');
  need(fs.realpathSync(outputDirectory)===outputDirectory,'output directory symlink refused');const p=path.join(outputDirectory,name);fs.writeFileSync(p,data,{flag:'wx',mode:0o600});return p;
}
function json(v){return JSON.stringify(v,null,2)+'\n';}
function pinned(input){const b=readRegular(input);need(digest(b)===PIN&&b.length===127615696,'fixed M10 source identity');const h=outer(b);need(h.hardware===0x141&&h.version.join('.')==='4.26.36.10'&&h.type===0x07e1ea5e,'M10 profile identity');return b;}
export function fixture(payload){
  const header=Buffer.alloc(424);header.writeUInt32LE(0x1e1caf3e,0);Buffer.from([4,26,36,10]).copy(header,4);header.writeUInt32LE(0x07e1ea5e,8);header.writeUInt32LE(4,36);header.writeUInt32LE(56,40);header.writeUInt32LE(2,44);header.writeUInt32LE(0x141,48);header.writeUInt32LE(0xe1e350c1,52);header.write('COMP',56);header.write('PMOC',404);header.writeUInt32LE(1,400);
  const codes=[49152,32768,53248,61440,57344,40960,0,16384,63488],lens=[4,3,4,5,4,3,2,2,5];
  for(let i=0;i<9;i++){header.writeUInt16LE(codes[i],76+i*2);header.writeUInt16LE(lens[i],108+i*2);}for(let i=0;i<256;i++)header[140+i]=i;
  const enc=tables(header).enc;let bits=0;for(const v of payload)bits+=enc[v].len;const packed=Math.ceil(bits/8),old={bits,packed,padding_bits:packed*8-bits,source:Buffer.alloc(packed)};
  const stream=encodeBlock(payload,enc,old).data;header.writeUInt32LE(368,412);header.writeUInt32LE(packed,416);header.writeUInt32LE(payload.length,420);header.writeUInt32LE(368+packed,12);header.writeUInt32LE(payload.length,16);const raw=Buffer.concat([header,stream]);Buffer.from(digest(raw.subarray(56),'md5'),'hex').copy(raw,20);return raw;
}
async function main(){
  const [cmd,...args]=process.argv.slice(2);
  if(cmd==='rebuild-reference'){
    need(args.length===2,'rebuild-reference OFFICIAL_INPUT OUTPUT_DIRECTORY');outputDirectory=path.resolve(args[1]);fs.mkdirSync(outputDirectory,{recursive:true});const raw=pinned(args[0]),d=decodeComp(raw),toc=parseToc(d.payload);
    need(toc.count===277&&toc.entries[267].sha256===IMG_PIN,'main IMG/full catalog profile');
    const payload=rebuildToc(d.payload,toc),encoded=encodeComp(d,payload),report=compare(raw,encoded.raw);
    need(report.all_component_metadata_identical&&report.all_component_bytes_identical&&report.expanded_payload_identical,'reference reconstruction identity');
    const output=writeNew('M10-4.26.36.10.rebuilt.reference.analysis.bin',encoded.raw);
    const result={schema:'leica.reference-rebuild/v1',operation:'decode -> rebuild 277 TOC records/component MD5/directory MD5 -> COMP encode all symbols -> outer MD5 -> fresh parse',output,output_sha256:digest(encoded.raw),output_bytes:encoded.raw.length,payload_sha256:digest(payload),package_byte_identical:raw.equals(encoded.raw),blocks:encoded.audit,qualification:qualification()};
    writeNew('component-diff.json',json(report));writeNew('rebuild-result.json',json(result));process.stdout.write(json(result));
  }else if(cmd==='inspect'){
    need(args.length===1,'inspect FILE');const b=readRegular(args[0]),d=decodeComp(b),t=parseToc(d.payload);
    process.stdout.write(json({header:d.header,payload_sha256:digest(d.payload),components:t.entries.map(safeEntry),profile_matches_pinned_M10:d.header.sha256===PIN&&d.header.type===0x07e1ea5e&&d.header.hardware===0x141&&d.header.version.join('.')==='4.26.36.10'&&t.count===277&&t.entries[267].sha256===IMG_PIN,qualification:qualification()}));
  }else if(cmd==='diff'){
    need(args.length===2,'diff ORIGINAL REFERENCE_FILE');process.stdout.write(json(compare(pinned(args[0]),readRegular(args[1]))));
  }else if(cmd==='check-replacement'){
    need(args.length===1,'check-replacement MANIFEST_JSON');const m=JSON.parse(readRegular(args[0],1024*1024));process.stdout.write(json({replacement_accepted:false,reasons:replacementReasons(m),qualification:qualification()}));process.exitCode=1;
  }else throw Error('usage: package_builder.mjs rebuild-reference OFFICIAL_INPUT OUTPUT_DIRECTORY | inspect FILE | diff ORIGINAL REFERENCE_FILE | check-replacement MANIFEST_JSON');
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url))main().catch(e=>{process.stdout.write(json({error:e.message,accepted:false}));process.exitCode=2;});
