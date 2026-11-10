// Host check with LinkeDOM (npm install linkedom). Tests real embedded JavaScript.
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const {parseHTML}=require('linkedom');
const text=fs.readFileSync(path.join(__dirname,'../src/web_ui.cpp'),'utf8').split('R"HTML(')[1].split(')HTML"')[0];
const {document,window}=parseHTML(text);
// LinkeDOM's select.value has only a getter; emulate the browser setter.
const select=document.getElementById('size');
Object.defineProperty(select,'value',{get(){return this.querySelector('option[selected]')?.value||this.firstElementChild.value},set(v){for(const o of this.children){o.selected=o.value===String(v)}}});
const status={camera:true,cameraError:'',psram:true,connected:true,ssid:'Home',apName:'CyberBowl-Camera-1234',stationIp:'192.168.1.85',apIp:'192.168.4.1',rssi:-52,uptime:60,light:false,image:{size:0,quality:12,fps:10,brightness:0,contrast:0,saturation:0,mirror:false,flip:false}};
const requests=[];let failSave=false;
const context=vm.createContext({document,location:{hostname:'192.168.4.1'},URLSearchParams,AbortController,Date,setTimeout,clearTimeout,setInterval(){},fetch:async(url,options)=>{
 if(options.method==='POST'){requests.push({url,body:Object.fromEntries(options.body),headers:options.headers});return {ok:!failSave,status:failSave?500:200,json:async()=>failSave?{error:'Запись не удалась'}:{ok:true}};}
 return {ok:true,json:async()=>status};
}});
vm.runInContext(text.split('<script>')[1].split('</script>')[0],context);
const settle=()=>new Promise(r=>setTimeout(r,15));
function submit(id){const form=document.getElementById(id);const e=new window.Event('submit',{cancelable:true});e.submitter=form.querySelector('button');form.dispatchEvent(e);}
(async()=>{
 await settle(); assert.equal(document.getElementById('stationIp').textContent,'192.168.1.85');
 document.getElementById('quality').value='20';document.getElementById('mirror').checked=true;submit('imageForm');await settle();
 assert.equal(requests[0].url,'/api/image');assert.equal(requests[0].body.quality,'20');assert.equal(requests[0].body.mirror,'1');assert.equal(requests[0].headers['X-CyberBowl'],'1');
 document.getElementById('ssid').value='Cats & Home+WiFi';document.getElementById('password').value='abcd+123&';submit('wifiForm');await settle();
 assert.equal(requests[1].body.ssid,'Cats & Home+WiFi');assert.equal(requests[1].body.password,'abcd+123&');assert.equal(document.getElementById('password').value,'');
 document.getElementById('light').dispatchEvent(new window.Event('click'));await settle();assert.equal(requests[2].url,'/api/light');
 failSave=true;submit('imageForm');await settle();assert.equal(document.getElementById('message').textContent,'Запись не удалась');assert(!document.querySelector('#imageForm button').disabled);
 status.camera=false;status.cameraError='init failed';await vm.runInContext('refresh()',context);assert(document.getElementById('play').disabled);assert.equal(document.getElementById('cameraStatus').textContent,'init failed');
 console.log('DOM check passed: image, Wi-Fi and light commands, password clearing, persistence error, camera failure. Layout and physical hardware are outside this check.');
})().catch(e=>{console.error(e);process.exitCode=1});
