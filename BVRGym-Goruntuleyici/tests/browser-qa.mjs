// Real native CSVs, folder replacement and playback through the full browser UI.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createRequire} from 'node:module';
import fs from 'node:fs/promises';
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const require=createRequire(import.meta.url);
const options=Object.fromEntries(process.argv.slice(2).reduce((pairs,value,index,all)=>index%2?pairs:[...pairs,[value.slice(2),all[index+1]]],[]));
for(const key of ['python','playwright','browser','native','recording'])assert.ok(options[key],`--${key} required`);
const {chromium}=require(options.playwright);
const root=path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const temporary=await fs.mkdtemp(path.join(os.tmpdir(),'bvrgym-csv-qa-'));
const recording=path.join(temporary,'recording');await fs.mkdir(recording);
const socket=net.createServer();await new Promise(resolve=>socket.listen(0,'127.0.0.1',resolve));
const port=socket.address().port;await new Promise(resolve=>socket.close(resolve));
const child=spawn(options.python,[path.join(root,'viewer.py'),'--recording',recording,'--port',String(port)],{cwd:root,windowsHide:true,stdio:['ignore','pipe','pipe']});
let stderr='';child.stderr.on('data',chunk=>stderr+=chunk);
let browser;
try{
  await new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(new Error('Viewer startup timed out: '+stderr)),10000);
    child.on('exit',code=>{clearTimeout(timer);reject(new Error('Viewer exited '+code+': '+stderr));});
    child.stdout.on('data',chunk=>{if(String(chunk).includes('http://')){clearTimeout(timer);resolve();}});
  });
  browser=await chromium.launch({headless:true,executablePath:options.browser,args:['--use-angle=swiftshader','--enable-unsafe-swiftshader']});
  const page=await browser.newPage({viewport:{width:1500,height:950}}),errors=[];
  page.on('pageerror',error=>errors.push(error.message));
  await page.goto(`http://127.0.0.1:${port}`,{waitUntil:'domcontentloaded'});
  await page.waitForFunction(()=>document.querySelector('#status').textContent==='Kayıt bekleniyor');
  assert.equal(await page.locator('#timeline').isDisabled(),true);
  assert.equal(await page.getByText('Canlıya git',{exact:true}).count(),0);
  const replaceFiles=async directory=>{
    for(const filename of await fs.readdir(recording))if(filename.endsWith('.csv'))await fs.unlink(path.join(recording,filename));
    for(const filename of await fs.readdir(directory))if(filename.endsWith('.csv'))await fs.copyFile(path.join(directory,filename),path.join(recording,filename));
  };
  await replaceFiles(options.native);
  await page.waitForFunction(async()=>{const {buffer}=await import('/app.js');return buffer.complete&&buffer.packet.objects===5;});
  await page.waitForFunction(()=>!document.querySelector('#timeline').disabled&&Number(document.querySelector('#timeline').max)>1);
  const nativeTimes=await page.evaluate(async()=>{
    const {buffer}=await import('/app.js');
    const missiles=[...buffer.tracks.values()].filter(track=>track.samples[0].kind==='missile');
    return {before:Math.floor((Math.min(...missiles.map(track=>track.samples[0].record_start_s))-.02)*100)/100,
      during:Math.ceil((Math.max(...missiles.map(track=>track.samples[0].record_start_s))+.02)*100)/100};
  });
  await page.locator('#timeline').fill(String(nativeTimes.before));await page.waitForTimeout(250);
  assert.equal(await page.locator('.entity[data-kind="aircraft"]').count(),2);
  assert.equal(await page.locator('.entity[data-kind="missile"]').count(),0);
  await page.locator('#timeline').fill(String(nativeTimes.during));await page.waitForTimeout(250);
  assert.equal(await page.locator('.entity[data-kind="missile"]').count(),3);
  const sceneResult=await page.evaluate(async time=>{
    const {scene,buffer}=await import('/app.js');
    for(const [id,record] of scene.models){
      const entity=buffer.at(id,time);
      assertEquivalent(record.model.position.y,entity.altitude_m);
      assertEquivalent(record.model.rotation.y,-entity.heading_deg*Math.PI/180);
    }
    function assertEquivalent(actual,expected){if(Math.abs(actual-expected)>1e-8)throw new Error(`${actual} != ${expected}`);}
    return {models:scene.models.size,objects:buffer.packet.objects};
  },nativeTimes.during);
  assert.equal(sceneResult.models,5);
  assert.equal(await page.locator('#mach').textContent(),'—');assert.equal(await page.locator('#speed').textContent(),'—');
  await page.locator('#playback-speed').fill('5');await page.waitForTimeout(200);
  assert.equal(await page.locator('#playback-rate').textContent(),'10×');
  assert.equal(await page.locator('#status').textContent(),'Duraklatıldı');
  await page.locator('#record-end').click();await page.waitForTimeout(250);
  assert.equal(await page.locator('#status').textContent(),'Kayıt sonu');
  await page.locator('#timeline').fill(String(nativeTimes.during));await page.waitForTimeout(250);
  assert.equal(await page.locator('.entity[data-kind="missile"]').count(),3);
  const oldSession=await page.evaluate(async()=>(await import('/app.js')).buffer.session);
  await replaceFiles(options.recording);
  await page.waitForFunction(async old=>{const {buffer}=await import('/app.js');return buffer.session!==old&&buffer.complete&&buffer.packet.bounds?.last>1199;},oldSession);
  await page.waitForFunction(()=>Number(document.querySelector('#timeline').max)>1199);
  await page.locator('#timeline').fill('780');await page.waitForTimeout(300);
  assert.equal(await page.locator('.entity[data-kind="aircraft"]').count(),2);
  assert.ok(await page.locator('.entity[data-kind="missile"]').count()>0);
  assert.ok(await page.locator('.entity[data-team="red"][data-kind="aircraft"]').count()>0);
  await page.locator('#timeline').fill('10');await page.waitForTimeout(250);
  assert.equal(await page.locator('.entity[data-kind="missile"]').count(),0);
  await page.locator('#playback-speed').fill('1');await page.locator('#play-pause').click();
  const start=Number(await page.locator('#timeline').inputValue());await page.waitForTimeout(1000);
  const finish=Number(await page.locator('#timeline').inputValue());assert.ok(finish-start>1.5&&finish-start<2.5);
  await page.locator('#timeline').fill('780');await page.locator('#fit-all').click();await page.waitForTimeout(250);
  if(options.screenshot)await page.screenshot({path:options.screenshot,fullPage:true});
  await page.locator('#view-2d').click();await page.waitForTimeout(150);
  assert.equal(await page.locator('#scene2d').isVisible(),true);
  const resizer=await page.locator('#dock-resizer').boundingBox();
  await page.mouse.move(resizer.x+resizer.width/2,resizer.y+resizer.height/2);await page.mouse.down();
  await page.mouse.move(resizer.x+resizer.width/2,resizer.y+resizer.height/2+60);await page.mouse.up();
  assert.equal(await page.locator('#dock-resizer').getAttribute('aria-valuenow'),'94');
  await page.setViewportSize({width:430,height:850});await page.waitForTimeout(250);
  assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+2));
  assert.deepEqual(errors,[]);
  console.log(JSON.stringify({passed:true,nativeCSV:true,models:sceneResult.models,folderReplacement:true,
    commonMissileClock:true,rewind:true,replay2x:true,view2D:true,dockResizing:true,mobile:true,errors}));
}finally{
  if(browser)await browser.close();
  if(child.exitCode===null){child.kill();await new Promise(resolve=>child.once('exit',resolve));}
  const resolved=path.resolve(temporary),prefix=path.resolve(os.tmpdir())+path.sep;
  if(!resolved.startsWith(prefix)||!path.basename(resolved).startsWith('bvrgym-csv-qa-'))throw new Error('Unsafe test cleanup path');
  await fs.rm(resolved,{recursive:true,force:true});
}
