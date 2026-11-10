// NODE_PATH must point to a node_modules directory containing Playwright.
const fs=require('fs');
const path=require('path');
const http=require('http');
const assert=require('assert');
const {chromium}=require('playwright');
const root=path.resolve(__dirname,'..');
const html=fs.readFileSync(path.join(root,'src/web_ui.cpp'),'utf8').split('R"HTML(')[1].split(')HTML"')[0];
const status={camera:true,cameraError:'',psram:true,connected:true,ssid:'Home Wi-Fi',apName:'CyberBowl-Camera-A1B2',stationIp:'192.168.1.85',apIp:'192.168.4.1',rssi:-52,uptime:420,light:false,storage:true,image:{size:0,quality:12,fps:10,brightness:0,contrast:0,saturation:0,mirror:false,flip:false}};
const requests=[];
const server=http.createServer((req,res)=>{
 if(req.url==='/'){res.setHeader('Content-Type','text/html; charset=utf-8');res.end(html);return;}
 if(req.url==='/api/status'){res.setHeader('Content-Type','application/json');res.end(JSON.stringify(status));return;}
 if(req.method==='POST'){
  let body='';req.on('data',d=>body+=d);req.on('end',()=>{requests.push({url:req.url,body:Object.fromEntries(new URLSearchParams(body)),header:req.headers['x-cyberbowl']});res.setHeader('Content-Type','application/json');res.end('{"ok":true}');});return;
 }
 res.statusCode=404;res.end();
});
(async()=>{
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 const browser=await chromium.launch({headless:true,args:['--no-sandbox'],...(process.env.BROWSER_EXECUTABLE ? {executablePath:process.env.BROWSER_EXECUTABLE}: {})});
 try{
  const page=await browser.newPage({viewport:{width:1120,height:1100},deviceScaleFactor:1});
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto('http://127.0.0.1:'+server.address().port);
  await page.waitForFunction(()=>document.getElementById('badge').textContent==='Камера работает');
  assert.equal(await page.locator('#stationIp').innerText(),'192.168.1.85');
  await page.locator('#quality').fill('20'); await page.locator('#mirror').check();
  await page.locator('#imageForm button').click();
  await page.waitForFunction(()=>document.getElementById('message').textContent.includes('изображения сохранены'));
  assert.equal(requests[0].url,'/api/image');assert.equal(requests[0].body.quality,'20');assert.equal(requests[0].body.mirror,'1');assert.equal(requests[0].header,'1');
  await page.locator('#ssid').fill('Cats & Home+WiFi');await page.locator('#password').fill('test+123&');
  await page.locator('#wifiForm button').click();
  await page.waitForFunction(()=>document.getElementById('message').textContent.includes('Сеть сохранена'));
  assert.equal(requests[1].body.ssid,'Cats & Home+WiFi');assert.equal(requests[1].body.password,'test+123&');assert.equal(await page.locator('#password').inputValue(),'');
  await page.locator('#light').click(); await page.waitForTimeout(200);assert.equal(requests[2].url,'/api/light');assert.equal(requests[2].body.enabled,'1');
  // Restore normal defaults for the preview. No simulated camera picture is shown.
  await page.reload();await page.waitForFunction(()=>document.getElementById('badge').textContent==='Камера работает');
  await page.screenshot({path:path.join(root,'docs/camera-desktop.png'),fullPage:true});
  await page.setViewportSize({width:390,height:844});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth));
  await page.screenshot({path:path.join(root,'docs/camera-mobile.png'),fullPage:true});
  status.psram=false;status.camera=false;status.cameraError='esp_camera_init: 0x0105';
  await page.reload();await page.waitForFunction(()=>document.getElementById('badge').textContent==='Ошибка камеры');
  assert(await page.locator('#play').isDisabled());assert(await page.locator('#size option[value="2"]').isDisabled());
  status.camera=true;status.psram=true;
  await page.route('**/api/image',route=>route.fulfill({status:500,contentType:'application/json',body:'{"error":"Не удалось сохранить параметры"}'}));
  await page.locator('#imageForm button').click();
  await page.waitForFunction(()=>document.getElementById('message').textContent==='Не удалось сохранить параметры');
  assert.equal(errors.length,0);
  console.log('Web UI passed: image/Wi-Fi/light requests, password clearing, error state, mobile layout, save failure. Desktop and mobile previews generated with mock status data.');
 }finally{await browser.close();server.close();}
})().catch(error=>{console.error(error);server.close();process.exitCode=1;});
