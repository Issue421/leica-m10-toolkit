import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {HERE,PIN,IMG_PIN,fixture,decodeComp,encodeComp,parseToc,rebuildToc,outer,digest,replacementReasons} from './package_builder.mjs';
function rehash(b){Buffer.from(digest(b.subarray(56),'md5'),'hex').copy(b,20);return b;}
function dirhash(b){Buffer.from(digest(b.subarray(36,36+b.readUInt32LE(24)),'md5'),'hex').copy(b,4);return b;}
function payload(){
  const n=2,base=36+n*584+4,out=Buffer.alloc(base+83,0x67);
  out.writeUInt32LE(0x70c4ead3,0);out.writeUInt32LE(n,20);out.writeUInt32LE(n*584,24);out.writeUInt32LE(36,28);out.writeUInt32LE(0x8f3b152c,32);out.writeUInt32LE(0xda7ada7a,base-4);
  for(let i=0;i<n;i++){
    const rec=out.subarray(36+i*584,36+(i+1)*584),data=Buffer.from('component-'+i),rel=7+i*23;
    // Intentionally preserve nonzero unknown fields, fixed-text suffixes and gaps.
    rec.writeUInt32LE(0x01020001,0);rec.writeUInt32LE(0x0a241a04,4);rec.writeUInt32LE(rel,24);rec.writeUInt32LE(data.length,28);rec.writeUInt32LE(0x141,32);rec.writeUInt32LE(0x120,40);rec.writeUInt32LE(0,44);rec.write('fixture'+i+'\0',52);rec.write('/host-fixture/'+i+'\0',72);Buffer.from(digest(data,'md5'),'hex').copy(rec,8);data.copy(out,base+rel);
  }return dirhash(out);
}
test('COMP encodes all 256 byte values and reconstructs actual symbols',()=>{
 const p=Buffer.from(Array.from({length:4096},(_,i)=>i&255)),raw=fixture(p),d=decodeComp(raw),e=encodeComp(d,p);
 assert.deepEqual(d.payload,p);assert.deepEqual(e.raw,raw);assert.equal(e.audit[0].encoded_symbols,p.length);
});
test('COMP nonidentity byte permutation roundtrip',()=>{
 const raw=fixture(Buffer.alloc(256,200));for(let i=0;i<256;i++)raw[140+i]=255-i;rehash(raw);
 const d=decodeComp(raw);assert.deepEqual(d.payload,Buffer.alloc(256,55));assert.deepEqual(encodeComp(d,d.payload).raw,raw);
});
test('COMP preserves nonzero partial-byte padding',()=>{
 const raw=fixture(Buffer.from([128]));raw[raw.length-1]|=15;rehash(raw);const d=decodeComp(raw);
 assert.equal(d.blocks[0].padding_bits,4);assert.deepEqual(encodeComp(d,d.payload).raw,raw);
});
test('codec recomputes changed bit count, block length and outer lengths',()=>{
 const d=decodeComp(fixture(Buffer.from([128]))),p=Buffer.from([0,1,2,3,4,255,128]),e=encodeComp(d,p);
 assert.deepEqual(decodeComp(e.raw).payload,p);assert.equal(e.audit[0].padding_policy,'zero-pad-to-32-bit-boundary');assert.equal(e.audit[0].packed_bytes%4,0);assert.equal(e.raw.readUInt32LE(16),p.length);assert.equal(e.raw.readUInt32LE(12),e.raw.length-56);
});
test('compressed checksum corruption rejected',()=>{
 const raw=fixture(Buffer.from([1,2,3]));raw[raw.length-1]^=128;assert.throws(()=>decodeComp(raw),/outer MD5/);
});
test('outer length and magic rejected',()=>{
 const a=fixture(Buffer.from([1]));a.writeUInt32LE(a.length,12);assert.throws(()=>outer(a),/dimensions/);
 const b=fixture(Buffer.from([1]));b[52]^=1;assert.throws(()=>outer(b),/magic/);
});
test('COMP marker high-bit aliases rejected after valid outer checksum',()=>{
 for(let i=0;i<4;i++){
  const raw=fixture(Buffer.from([128]));raw[56+i]|=128;rehash(raw);assert.doesNotThrow(()=>outer(raw));
  assert.throws(()=>decodeComp(raw),{message:'COMP format required'},`COMP marker byte ${i}`);
 }
});
test('PMOC marker high-bit aliases rejected after valid outer checksum',()=>{
 for(let i=0;i<4;i++){
  const raw=fixture(Buffer.from([128]));raw[404+i]|=128;rehash(raw);assert.doesNotThrow(()=>outer(raw));
  assert.throws(()=>decodeComp(raw),{message:'COMP directory'},`PMOC marker byte ${i}`);
 }
});
test('COMP byte permutation collision rejected',()=>{
 const a=fixture(Buffer.from([1]));a[140]=a[141];rehash(a);assert.throws(()=>decodeComp(a),/permutation/);
});
test('COMP incomplete prefix and invalid length rejected',()=>{
 const a=fixture(Buffer.from([1]));for(let i=0;i<16;i++)a.writeUInt16LE(0,108+i*2);rehash(a);assert.throws(()=>decodeComp(a),/incomplete/);
 const b=fixture(Buffer.from([1]));b.writeUInt16LE(17,108);rehash(b);assert.throws(()=>decodeComp(b),/prefix bounds/);
});
test('COMP overlapping block directory and excess output rejected',()=>{
 const a=fixture(Buffer.from([1,2,3]));a.writeUInt32LE(367,412);rehash(a);assert.throws(()=>decodeComp(a),/block bounds/);
 const b=fixture(Buffer.from([1,2,3]));b.writeUInt32LE(4,420);rehash(b);assert.throws(()=>decodeComp(b),/block bounds/);
});
test('COMP truncated symbol rejected after valid outer checksum',()=>{
 const full=fixture(Buffer.from([0])),a=Buffer.from(full.subarray(0,-1));a.writeUInt32LE(a.length-56,12);a.writeUInt32LE(a.length-424,416);rehash(a);assert.throws(()=>decodeComp(a),/truncated/);
});
test('TOC reconstruction preserves all records, bodies, unknown bytes, gaps and trailer',()=>{
 const p=payload(),t=parseToc(p),out=rebuildToc(p,t);assert.deepEqual(out,p);assert.equal(t.entries.length,2);for(const e of t.entries)assert.equal(e.record.length,584);
});
test('TOC component and directory checksums separately enforced',()=>{
 const a=payload();a[40]^=1;assert.throws(()=>parseToc(a),/directory MD5/);
 const b=payload(),t=parseToc(b);b[t.entries[0].offset]^=1;assert.throws(()=>parseToc(b),/component MD5/);
});
test('TOC dimensions, termination and component bounds enforced',()=>{
 const a=payload();a.writeUInt32LE(0xffffffff,20);assert.throws(()=>parseToc(a),/dimensions/);
 const b=payload();b[b.readUInt32LE(24)+36]^=1;assert.throws(()=>parseToc(b),/terminator/);
 const c=payload();c.writeUInt32LE(0xffffffff,36+24);dirhash(c);assert.throws(()=>parseToc(c),/component bounds/);
});
test('TOC component overlap rejected with valid component and directory MD5',()=>{
 const a=payload();a.subarray(36+8,36+24).copy(a,36+584+8);a.writeUInt32LE(a.readUInt32LE(36+24),36+584+24);dirhash(a);assert.throws(()=>parseToc(a),/overlap/);
});
test('M10-P and caller asserted admission cannot authorize IMG replacement',()=>{
 const r=replacementReasons({schema:'leica.admitted-main-img-patch/v1',model:'M10-P',version:'5.26.35.51',base_package_sha256:PIN,base_img_sha256:IMG_PIN,patches:[],abi_evidence:true,owned_code_extents:true,loader_evidence:true,independent_admission:true});
 assert(r.some(x=>x.includes('M10-P')));assert(r.some(x=>x.includes('no independent admission')));
});
test('independent pinned Python parser decodes generated synthetic COMP/TOC',()=>{
 const original=payload(),modified=Buffer.concat([original,Buffer.alloc(29,0)]),d=decodeComp(fixture(original)),e=encodeComp(d,modified),raw=e.raw,f=path.join(HERE,'synthetic.reference.analysis.bin');
 assert.notEqual(raw.length,fixture(original).length);assert.equal(e.audit[0].padding_policy,'zero-pad-to-32-bit-boundary');fs.writeFileSync(f,raw,{flag:'wx'});
 try{const r=spawnSync(process.env.PYTHON || 'python3',['-B',path.join(HERE,'independent_verify.py'),'--fixture',f],{encoding:'utf8'});assert.equal(r.status,0,r.stdout+r.stderr);const v=JSON.parse(r.stdout);assert.equal(v.components,2);assert.equal(v.payload_sha256,digest(modified));}finally{fs.unlinkSync(f);}
});

test('encoder creates changed compressed data from changed same-length-code symbols',()=>{
 const raw=fixture(Buffer.from([128,129,127])),d=decodeComp(raw),changed=Buffer.from([129,127,128]),e=encodeComp(d,changed);
 assert.notDeepEqual(e.raw,raw);assert.deepEqual(decodeComp(e.raw).payload,changed);assert.equal(e.audit[0].source_packed_identical,false);
});
test('multi-block COMP encoder reconstructs directory offsets and every block',()=>{
 const pa=Buffer.from([0,1,2,128,255]),pb=Buffer.from([127,129,130,254,253]),a=fixture(pa),b=fixture(pb);
 const sa=a.subarray(424),sb=b.subarray(424),raw=Buffer.alloc(440+sa.length+sb.length);a.copy(raw,0,0,408);raw.writeUInt32LE(2,400);
 for(const [i,src,p,rel] of [[0,sa,pa,384],[1,sb,pb,384+sa.length]]){const at=408+i*16;raw.writeUInt32LE(0,at);raw.writeUInt32LE(rel,at+4);raw.writeUInt32LE(src.length,at+8);raw.writeUInt32LE(p.length,at+12);src.copy(raw,56+rel);}
 raw.writeUInt32LE(raw.length-56,12);raw.writeUInt32LE(pa.length+pb.length,16);rehash(raw);const d=decodeComp(raw);assert.equal(d.blocks.length,2);assert.deepEqual(d.payload,Buffer.concat([pa,pb]));assert.deepEqual(encodeComp(d,d.payload).raw,raw);
});
test('oversized expanded length rejected before allocation',()=>{
 const a=fixture(Buffer.from([128]));a.writeUInt32LE(0xffffffff,16);assert.throws(()=>decodeComp(a),/dimensions/);
});
test('ELF append request gives concrete representation and loader refusal',()=>{
 assert(replacementReasons({replacement_format:'ELF',strategy:'append'}).some(x=>x.includes('EOF load coverage')));
});

test('multi-block changed size shifts subsequent packed offsets and last expanded length',()=>{
 const pa=Buffer.from([128]),pb=Buffer.from([128]),a=fixture(pa),b=fixture(pb),sa=a.subarray(424),sb=b.subarray(424),raw=Buffer.alloc(440+sa.length+sb.length);a.copy(raw,0,0,408);raw.writeUInt32LE(2,400);
 for(const [i,src,p,rel] of [[0,sa,pa,384],[1,sb,pb,384+sa.length]]){const at=408+i*16;raw.writeUInt32LE(0,at);raw.writeUInt32LE(rel,at+4);raw.writeUInt32LE(src.length,at+8);raw.writeUInt32LE(p.length,at+12);src.copy(raw,56+rel);}
 raw.writeUInt32LE(raw.length-56,12);raw.writeUInt32LE(2,16);rehash(raw);const d=decodeComp(raw),p=Buffer.from([0,255,0,255,0,255]),e=encodeComp(d,p),z=decodeComp(e.raw);
 assert.deepEqual(z.payload,p);assert.equal(z.blocks[1].relative,z.blocks[0].relative+z.blocks[0].packed);assert.equal(z.blocks[1].expanded,5);assert.notEqual(z.blocks[1].relative,d.blocks[1].relative);
});

test('TOC mutation -> two-block changed packed offsets -> outer hash -> independent full parse',()=>{
 const original=payload(),chunks=[original.subarray(0,256),original.subarray(256)],streams=chunks.map(x=>fixture(x).subarray(424)),h=fixture(chunks[0]),raw=Buffer.alloc(440+streams[0].length+streams[1].length);h.copy(raw,0,0,408);raw.writeUInt32LE(2,400);let rel=384;
 for(let i=0;i<2;i++){const at=408+i*16;raw.writeUInt32LE(0,at);raw.writeUInt32LE(rel,at+4);raw.writeUInt32LE(streams[i].length,at+8);raw.writeUInt32LE(chunks[i].length,at+12);streams[i].copy(raw,56+rel);rel+=streams[i].length;}raw.writeUInt32LE(raw.length-56,12);raw.writeUInt32LE(original.length,16);rehash(raw);
 const modified=Buffer.concat([original,Buffer.alloc(29,0)]),t=parseToc(original),entry=t.entries[0];modified.fill(128,160,256);modified[entry.offset]^=128;Buffer.from(digest(modified.subarray(entry.offset,entry.offset+entry.bytes),'md5'),'hex').copy(modified,36+8);dirhash(modified);
 const d=decodeComp(raw),e=encodeComp(d,modified),z=decodeComp(e.raw);assert.notEqual(z.blocks[1].relative,d.blocks[1].relative);assert.notDeepEqual(modified.subarray(4,20),original.subarray(4,20));assert.notDeepEqual(e.raw.subarray(20,36),raw.subarray(20,36));
 const f=path.join(HERE,'synthetic-composite.reference.analysis.bin');fs.writeFileSync(f,e.raw,{flag:'wx'});try{const r=spawnSync(process.env.PYTHON || 'python3',['-B',path.join(HERE,'independent_verify.py'),'--fixture',f],{encoding:'utf8'});assert.equal(r.status,0,r.stdout+r.stderr);const v=JSON.parse(r.stdout);assert.equal(v.blocks,2);assert.equal(v.components,2);assert.equal(v.payload_sha256,digest(modified));}finally{fs.unlinkSync(f);}
});
