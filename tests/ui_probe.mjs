// DOM-model regression for the shipped browser script, not a real-browser claim.
import vm from 'node:vm';
let raw='';for await(const part of process.stdin)raw+=part;
const {html,script,catalog}=JSON.parse(raw);
const elements={};
function attrs(text){return Object.fromEntries([...text.matchAll(/([\w:-]+)(?:="([^"]*)")?/g)].map(m=>[m[1],m[2]??true]));}
class Element {
 constructor(tag,a={}){this.tag=tag;this.value=a.value||'';this.checked=!!a.checked;this.disabled=!!a.disabled;this.content=a.content;this.options=[];this.children=[];this.listeners={};this.textContent='';this.parentElement={append(){}};}
 addEventListener(name,fn){this.listeners[name]=fn;}
 dispatchEvent(event){return this.listeners[event.type]?.(event);}
 querySelectorAll(selector){return this.children.filter(x=>x.tag==='input'&&(selector!=='input:checked'||x.checked));}
 closest(){return this.parentElement;}
 append(...items){this.children.push(...items);}
 click(){}
}
for(const m of html.matchAll(/<(\w+)([^>]*)>/g)){const a=attrs(m[2]);if(a.id)elements[a.id]=new Element(m[1],a);}
for(const m of html.matchAll(/<select([^>]*)>([\s\S]*?)<\/select>/g)){const e=elements[attrs(m[1]).id];e.options=[...m[2].matchAll(/<option([^>]*)>/g)].map(x=>new Element('option',attrs(x[1])));e.value=e.options[0]?.value||'';}
const styleBlock=html.match(/<div id="styles">([\s\S]*?)<\/div>/)[1];
elements.styles.children=[...styleBlock.matchAll(/<input([^>]*)>/g)].map(x=>new Element('input',attrs(x[1])));
let sent;
const context={document:{getElementById:id=>elements[id],querySelector:()=>({content:'probe-token'}),createElement:tag=>new Element(tag)},Event:class{constructor(type){this.type=type;}},URL:{createObjectURL:()=> 'blob:probe',revokeObjectURL(){}},setTimeout(){},fetch:async(url,options)=>{
 if(url==='/api/catalog')return {ok:true,json:async()=>catalog};
 if(url==='/api/export'){sent=JSON.parse(options.body);return {ok:true,blob:async()=>({})};}
 throw Error('Unexpected request '+url);
}};
vm.runInNewContext(script,context);
await new Promise(resolve=>setImmediate(resolve));
if(elements.download.disabled)throw Error('Initialization failed: '+elements.result.textContent);
await elements.download.listeners.click();
process.stdout.write(JSON.stringify({configuration:context.configuration(),exported:sent,styles:elements.styles.children.map(x=>({value:x.value,checked:x.checked,disabled:x.disabled})),disabled:Object.fromEntries(['touch','guides','preset','wb','gain-r','gain-g','gain-b'].map(id=>[id,elements[id].disabled])),default_options:elements['default-style'].options.map(x=>({value:x.value,disabled:x.disabled})),bundle_status:elements['bundle-status'].textContent}));
