'use strict';
const $=id=>document.getElementById(id), token=document.querySelector('meta[name="local-token"]').content;
let initialized=false;
function configuration(){if(!initialized)throw Error('正在读取本包配置 / Loading bundle configuration');return {schema:'leica.source-selection/v1',baseline:'M10-4.26.36.10',features:{touch:$('touch').checked,reported_serial:$('serial').value||null,styles:{enabled:[...$('styles').querySelectorAll('input:checked')].map(x=>x.value),default:$('default-style').value},frame_guides:{enabled:$('guides').checked,preset:Number($('preset').value)},jpeg_crop:$('crop').checked,wb:{enabled:$('wb').checked,gains:[$('gain-r').value,$('gain-g').value,$('gain-b').value]},low_noise_shutter:$('quiet').checked}};}
async function post(url,value){const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json','X-Local-Token':token},body:JSON.stringify(value)});if(!r.ok){const e=await r.json();throw Error(e.error||'Request rejected');}return r;}
$('styles').addEventListener('change',()=>{const enabled=[...$('styles').querySelectorAll('input:checked')].map(x=>x.value);for(const o of $('default-style').options)o.disabled=!enabled.includes(o.value);if(!enabled.includes($('default-style').value))$('default-style').value='normal';});
$('download').addEventListener('click',async()=>{const b=$('download');b.disabled=true;try{const r=await post('/api/export',configuration()),url=URL.createObjectURL(await r.blob()),a=document.createElement('a');a.href=url;a.download='leica-m10-development-source.zip';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);$('result').textContent='开发源码 ZIP 已下载。它不是相机更新文件。';}catch(e){$('result').textContent=e.message;}finally{b.disabled=!initialized;}});
$('verify').addEventListener('click',async()=>{try{const r=await post('/api/verify-official',{path:$('local-path').value});await r.json();$('result').textContent='原厂输入大小与 SHA256 正确。所选功能尚未加入此文件。';}catch(e){$('result').textContent=e.message;}});
function initialize(data){
 const f=data.default.features,a=data.available;
 $('touch').checked=f.touch;$('serial').value=f.reported_serial||'';
 for(const input of $('styles').querySelectorAll('input')){
  input.checked=f.styles.enabled.includes(input.value);input.disabled=input.value==='normal'||!a.styles.includes(input.value);
  if(!a.styles.includes(input.value))markUnavailable(input);
 }
 $('default-style').value=f.styles.default;
 $('guides').checked=f.frame_guides.enabled;$('preset').value=String(f.frame_guides.preset);
 $('crop').checked=f.jpeg_crop;$('quiet').checked=f.low_noise_shutter;$('wb').checked=f.wb.enabled;
 ['gain-r','gain-g','gain-b'].forEach((id,i)=>{$(id).value=f.wb.gains[i];});
 for(const [feature,id] of Object.entries({touch:'touch',reported_serial:'serial',frame_guides:'guides',wb:'wb'})){
  if(!a.features[feature])markUnavailable($(id));
 }
 if(!a.features.frame_guides)$('preset').disabled=true;
 if(!a.features.wb)for(const id of ['gain-r','gain-g','gain-b'])$(id).disabled=true;
 $('styles').dispatchEvent(new Event('change'));
 $('bundle-status').textContent='本包样式 / Available styles: '+a.styles.join(', ')+'. 未包含的模块不可选择。';
 initialized=true;$('download').disabled=false;
}
function markUnavailable(control){control.disabled=true;control.title='本包未包含 / Unavailable in this source bundle';const note=document.createElement('small');note.textContent=' 本包未包含 / unavailable';(control.closest('label')||control.parentElement).append(note);}
fetch('/api/catalog').then(r=>{if(!r.ok)throw Error('Cannot load bundle configuration');return r.json();}).then(data=>{initialize(data);$('official').href=data.official.url;$('pin').textContent=data.official.bytes+' bytes · SHA256 '+data.official.sha256;for(const item of data.catalog){const a=document.createElement('article'),h=document.createElement('h3'),s=document.createElement('p'),b=document.createElement('p');h.textContent=item.name;s.textContent=item.source_status;b.textContent=item.native_blockers;a.append(h,s,b);$('catalog').append(a);}}).catch(e=>{$('result').textContent=e.message;});
