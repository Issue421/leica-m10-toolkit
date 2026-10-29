/** Local source-resource extraction only. Uses accepted codec; never writes FW. */
import fs from 'node:fs';
import path from 'node:path';
import {readRegular,digest,decodeComp,parseToc,need,PIN} from './package_builder.mjs';
const [ordinary,asc,output]=process.argv.slice(2);
try {
 need(ordinary&&asc&&output&&process.argv.length===5,'extract_asc.mjs OFFICIAL_M10_FW OFFICIAL_ASC_FW OUTPUT_DIRECTORY');
 const specs=[['ordinary-b2y.host-resource',ordinary,PIN,127615696,'419c7d6635d1905146ee372967dc01c918a41fcca1d123d7f85cea0c5792c8a3'],['asc-b2y.host-resource',asc,'aacf21aac8aa1cd1d82c281c5fa1092c158e0bd491d23d9a0ae5197d08c32463',121740584,'88e93114be60854bfc84ba00901f39774ae962eeffd216a54dcf9ba6bbca1896']];
 fs.mkdirSync(output,{recursive:true});const retained=[];
 for(const [name,input,pin,bytes,bodyPin] of specs){
  const raw=readRegular(input);need(raw.length===bytes&&digest(raw)===pin,'exact official local input required');
  const toc=parseToc(decodeComp(raw).payload),matches=toc.entries.filter(e=>e.path==='/data/calib/B2Y.bin'&&e.sha256===bodyPin);
  need(matches.length===1,'unique pinned B2Y component required');const e=matches[0];
  if(name.startsWith('ordinary'))need(e.index===247&&e.record.readUInt32LE(32)===0x141&&e.record.readUInt32LE(40)===288,'ordinary M10 resource selector');
  fs.writeFileSync(path.join(output,name),e.data,{flag:'wx',mode:0o600});retained.push({file:name,bytes:e.bytes,sha256:e.sha256});
 }
 console.log(JSON.stringify({scope:'Local host analysis inputs only; not firmware',retained},null,2));
}catch(e){console.error(e.message);process.exitCode=2;}
