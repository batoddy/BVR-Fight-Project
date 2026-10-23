import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createRequire} from 'node:module';
import fs from 'node:fs/promises';
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const options=Object.fromEntries(process.argv.slice(2).reduce((pairs,value,index,all)=>index%2?pairs:[...pairs,[value.slice(2),all[index+1]]],[]));
const {chromium}=createRequire(import.meta.url)(options.playwright);
const root=path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const temporary=await fs.mkdtemp(path.join(os.tmpdir(),'bvrgym-csv-qa-'));
const recording=path.join(temporary,'recording');await fs.mkdir(recording);
const socket=net.createServer();await new Promise(resolve=>socket.listen(0,'127.0.0.1',resolve));
const port=socket.address().port;await new Promise(resolve=>socket.close(resolve));
const child=spawn(options.python,[path.join(root,'viewer.py'),'--recording',recording,'--port',String(port)],{cwd:root,windowsHide:true,stdio:['ignore','pipe','pipe']});
let stderr='',browser;child.stderr.on('data',chunk=>stderr+=chunk);
try{
  await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(new Error(stderr||'Startup timeout')),10000);child.on('exit',code=>{clearTimeout(timer);reject(new Error('Exit '+code+stderr));});child.stdout.on('data',chunk=>{if(String(chunk).includes('http://')){clearTimeout(timer);resolve();}});});
  browser=await chromium.launch({headless:true,executablePath:options.browser,args:['--use-angle=swiftshader','--enable-unsafe-swiftshader']});
  const page=await browser.newPage({viewport:{width:1500,height:950}}),errors=[];
  page.on('pageerror',error=>errors.push(error.message));page.on('console',message=>{if(message.type()==='error'&&!message.text().startsWith('Failed to load resource:'))errors.push(message.text());});
  await page.goto('http://127.0.0.1:'+port,{waitUntil:'domcontentloaded'});
  async function toggleFollow(){
    await page.locator('#view-options summary').click();await page.locator('#follow').click();await page.locator('#view-options summary').click();
  }
  await page.waitForFunction(()=>document.querySelector('#status').textContent==='Kayıt bekleniyor');
  assert.equal(await page.locator('#timeline').isDisabled(),true);
  assert.equal(await page.locator('dialog').count(),0);
  assert.equal(await page.locator('#object-scale').isVisible(),true);
  const actions=await page.locator('.toolbar-actions').boundingBox();
  assert.ok(Math.abs(actions.x+actions.width-1484)<2);
  await page.locator('#open-settings').click();
  assert.equal(await page.locator('#settings-panel').isVisible(),true);
  await page.locator('#view-options summary').click();
  await page.waitForFunction(()=>!document.querySelector('#settings-options').open);assert.equal(await page.locator('#settings-panel').isVisible(),false);
  assert.equal(await page.locator('#view-options .popover').isVisible(),true);
  await page.keyboard.press('Escape');
  assert.equal(await page.locator('#view-options .popover').isVisible(),false);
  await page.locator('#open-settings').click();await page.locator('h1').click();
  await page.waitForFunction(()=>!document.querySelector('#settings-options').open);assert.equal(await page.locator('#settings-panel').isVisible(),false);
  const stopsAligned=await page.evaluate(()=>{
    const slider=document.querySelector('#playback-speed').getBoundingClientRect();
    return [...document.querySelectorAll('[data-rate]')].every((button,index)=>{
      const box=button.getBoundingClientRect();
      return Math.abs(box.x+box.width/2-(slider.x+9+index*(slider.width-18)/7))<1;
    });
  });assert.equal(stopsAligned,true);

  async function files(directory){return (await fs.readdir(directory)).filter(name=>name.endsWith('.csv')).map(name=>path.join(directory,name));}
  async function chooseDirectory(directory){
    directory=await fs.realpath(directory);
    if(!await page.locator('#record-panel').isVisible())await page.locator('#open-recording').click();
    await page.route('**/api/choose-directory',async route=>{
      await page.request.post('http://127.0.0.1:'+port+'/api/directory',{data:{path:directory}});
      await route.fulfill({json:{cancelled:false,path:directory}});
    });
    await page.locator('#change-directory').click();
    await page.waitForFunction(directory=>[...document.querySelectorAll('#recording-library .library-item')].some(item=>item.dataset.path===directory),directory);
    await page.locator('#recording-library .library-item').filter({hasText:path.basename(directory)}).first().click();
    await page.unroute('**/api/choose-directory');
  }
  await chooseDirectory(options.native);
  await page.waitForFunction(async()=>{const {buffer}=await import('/app.js');return buffer.complete&&buffer.packet.objects===5;});
  await page.waitForFunction(()=>!document.querySelector('#record-options').open);
  await page.waitForFunction(()=>!document.querySelector('#timeline').disabled);
  await page.locator('#open-recording').click();
  await page.waitForFunction(()=>document.querySelectorAll('#recording-library .library-item').length>0);
  await page.locator('#record-search').fill('no-such-recording-qa');
  assert.equal(await page.locator('.recording-lists .library-item:visible').count(),0);
  await page.locator('#record-search').fill('');
  assert.ok(await page.locator('.recording-lists .library-item:visible').count()>0);
  if(options.screenshot)await page.screenshot({path:options.screenshot.replace('.png','-record-menu.png'),fullPage:true});
  await page.locator('#close-record-panel').click();
  await page.locator('#record-info').click();
  assert.equal(await page.locator('#record-info-panel').isVisible(),true);
  await page.locator('#close-info').click();
  await page.evaluate(async()=>{
    const {scene}=await import('/app.js');
    if(scene.world.children.some(child=>child.type==='GridHelper'))throw new Error('Grid still has overlapping geometry');
    for(const distance of [1500,15000,150000]){
      scene.camera.position.copy(scene.controls.target).add({x:distance,y:distance*.12,z:distance*.6});scene.controls.update();
      scene.renderer.render(scene.world,scene.camera);
      if(scene.renderer.getContext().getError()!==0)throw new Error('Grid rendering error');
    }
    scene.camera.position.copy(scene.controls.target).add({x:9000,y:6500,z:11000});scene.controls.update();
  });
  await page.evaluate(async()=>{
    const {scene}=await import('/app.js');const previous=scene.trailDuration;scene.trailDuration=-1;
    const track={samples:[0,10371.2,34076.8,148160].map(east=>({east_m:east,north_m:0,altitude_m:5000,sim_time_s:east/1481.6}))};
    const early=scene.altitudeDrops(track,10),late=scene.altitudeDrops(track,100);
    if(early.length!==10||late.length!==82)throw new Error('Guide density changed with trail length');
    for(let i=2;i<late.length;i+=2)if(Math.abs(late[i].distanceTo(late[i-2])-3704)>.001)throw new Error('Guide spacing is not fixed');
    for(let i=0;i<early.length;i+=2)if(early[i].distanceTo(late[i])>.001)throw new Error('Past guides moved');
    scene.trailDuration=10;const window=scene.altitudeDrops(track,100);
    if(window.length!==10||Math.abs(window[0].x-133344)>.001)throw new Error('Trail window shifted guide alignment');
    const rising={samples:[{east_m:0,north_m:0,altitude_m:5000,sim_time_s:0},{east_m:8889.6,north_m:11852.8,altitude_m:16112,sim_time_s:10}]};
    scene.trailDuration=-1;const climb=scene.altitudeDrops(rising,10);
    for(let i=2;i<climb.length;i+=2)if(Math.abs(climb[i].distanceTo(climb[i-2])-3704)>.001)throw new Error('Climb spacing mismatch');
    scene.trailDuration=previous;
  });
  const times=await page.evaluate(async()=>{
    const {buffer}=await import('/app.js');
    const missiles=[...buffer.tracks.values()].filter(track=>track.samples[0].kind==='missile');
    return {before:Math.max(0,Math.min(...missiles.map(track=>track.samples[0].record_start_s))-.02),
      during:Math.max(...missiles.map(track=>track.samples[0].record_start_s))+.02};
  });
  async function seek(time){await page.locator('#timeline').evaluate((input,time)=>{input.value=time;input.dispatchEvent(new Event('input',{bubbles:true}));},time);await page.waitForTimeout(250);}
  await seek(times.before);
  assert.equal(await page.locator('.entity[data-kind="aircraft"]').count(),2);
  assert.equal(await page.locator('#entity-list .munition[data-state="carried"]').count(),4);
  await seek(times.during);
  const geometry=await page.evaluate(async time=>{
    const {scene,buffer}=await import('/app.js');let count=0;
    for(const [id,record] of scene.models){const entity=buffer.at(id,time);if(!entity)continue;count++;
      if(Math.abs(record.model.position.y-entity.altitude_m)>1e-8)throw new Error('Altitude mismatch');
      if(Math.abs(record.model.rotation.y+entity.heading_deg*Math.PI/180)>1e-8)throw new Error('Heading mismatch');
    }return count;
  },Number(await page.locator('#timeline').inputValue()));assert.equal(geometry,5);
  assert.equal(await page.locator('#entity-list .aircraft-card').count(),2);
  await page.evaluate(async()=>{const {buffer}=await import('/app.js');buffer.tracks=new Map([...buffer.tracks].reverse());});
  await page.locator('#object-filter').selectOption('red');await page.locator('#object-filter').selectOption('all');
  assert.deepEqual(await page.locator('#entity-list .entity').evaluateAll(nodes=>nodes.map(node=>node.dataset.team)),['blue','red']);
  if(!await page.locator('#follow').isChecked())await toggleFollow();
  await page.waitForTimeout(200);
  assert.equal(await page.locator('#entity-list .munition').count(),4);
  assert.equal(await page.locator('#entity-list .munition.flying').count(),3);
  assert.ok(!(await page.locator('#speed').textContent()).includes('≈'));
  assert.ok(!(await page.locator('#mach').textContent()).includes('≈'));
  assert.ok(!(await page.locator('#missing-data').textContent()).includes('≈'));
  assert.ok(Number((await page.locator('#speed').textContent()).replace(',','.').trim())>200);
  assert.ok((await page.locator('#east').textContent()).endsWith('NM'));
  await page.evaluate(async()=>{
    const {scene,buffer}=await import('/app.js');
    const time=Number(document.querySelector('#timeline').value);
    for(const [id,record] of scene.models){
      const entity=buffer.at(id,time);if(!entity)continue;
      const value=entity.true_airspeed_mps??entity.position_speed_mps;
      const expected=Math.round(value).toLocaleString('tr-TR')+' m/s';
      if(record.labelSpeedValue.textContent!==expected||!record.labelAltitudeValue.textContent.endsWith('ft'))throw new Error('Aircraft/missile label speed mismatch');
      if(record.label.textContent.includes('≈'))throw new Error('Estimate symbol remains');
      if(!record.label.hidden){
        const name=record.labelName.getBoundingClientRect(),speed=record.labelSpeedValue.getBoundingClientRect(),altitude=record.labelAltitudeValue.getBoundingClientRect(),box=record.label.getBoundingClientRect();
        if(box.height>40||Math.abs(name.y-speed.y)>.1||name.right>=speed.x||altitude.y<name.bottom||Math.abs(altitude.x+altitude.width/2-box.x-box.width/2)>.1)throw new Error('Label does not match the sketch');
        if(record.labelSpeedValue.scrollWidth>record.labelSpeedValue.clientWidth+1)throw new Error('Speed clipped');
      }
    }
  });
  const physicalBefore=await page.evaluate(async()=>{const {buffer}=await import('/app.js');return buffer.entities(Number(document.querySelector('#timeline').value)).map(e=>[e.id,e.east_m,e.north_m,e.altitude_m]);});
  for(const [unit,symbol,factor] of [['kms','km/s',.001],['kmh','km/sa',3.6],['kts','kt',3600/1852],['mach','Mach',null]]){
    await page.locator('#open-settings').click();
    await page.locator('#speed-unit').selectOption(unit);await page.locator('#altitude-unit').selectOption('m');await page.locator('#distance-unit').selectOption('km');
    if(options.screenshot&&unit==='kts')await page.screenshot({path:options.screenshot.replace('.png','-settings.png'),fullPage:true});
    await page.locator('#close-settings').click();await page.waitForTimeout(200);
    assert.equal((await page.locator('#speed-unit-label').textContent()).trim(),symbol);
    assert.equal((await page.locator('#altitude-unit-label').textContent()).trim(),'m');
    assert.ok((await page.locator('#east').textContent()).endsWith('km'));
    assert.ok((await page.locator('.distance-scale-label').textContent()).endsWith('km'));
    assert.equal(await page.locator('#mach-measure').isVisible(),unit!=='mach');
    await page.evaluate(async({unit,symbol,factor})=>{
      const {scene,buffer}=await import('/app.js'),time=Number(document.querySelector('#timeline').value);
      const id=document.querySelector('.entity.selected').closest('.aircraft-card').dataset.id,entity=buffer.at(id,time);
      const expected=unit==='mach'?(entity.mach??entity.estimated_mach):(entity.true_airspeed_mps??entity.position_speed_mps)*factor;
      const actual=Number(document.querySelector('#speed').textContent.replaceAll('.','').replace(',','.'));
      if(Math.abs(actual-expected)>(unit==='kms'?.00051:unit==='mach'?.0051:.051))throw new Error('Panel unit conversion');
      const altitude=Number(document.querySelector('#altitude').textContent.replaceAll('.','').replace(',','.'));
      if(Math.abs(altitude-entity.altitude_m)>.51)throw new Error('Panel metric altitude');
      for(const record of scene.models.values()){
        if(!record.labelSpeedValue.textContent.endsWith(symbol)||!record.labelAltitudeValue.textContent.endsWith(' m'))throw new Error('Labels unit mismatch');
      }
    },{unit,symbol,factor});
  }
  assert.deepEqual(await page.evaluate(async()=>{const {buffer}=await import('/app.js');return buffer.entities(Number(document.querySelector('#timeline').value)).map(e=>[e.id,e.east_m,e.north_m,e.altitude_m]);}),physicalBefore);
  await page.locator('#view-2d').click();await page.waitForTimeout(200);
  await page.evaluate(async()=>{
    const {scene,buffer}=await import('/app.js'),time=Number(document.querySelector('#timeline').value),seen=[];
    const ctx=scene.flat.ctx,original=ctx.fillText;ctx.fillText=function(text,...args){seen.push(text);return original.call(this,text,...args);};
    try{const entities=buffer.entities(time);scene.flat.draw(entities,buffer,time,entities[0].id);}finally{ctx.fillText=original;}
    if(!seen.some(text=>text.endsWith(' Mach'))||!seen.some(text=>text.endsWith(' m')))throw new Error('2D unit labels');
  });
  await page.locator('#view-3d').click();
  await page.reload();await page.waitForFunction(()=>Number(document.querySelector('#timeline').max)<2&&!document.querySelector('#timeline').disabled);
  await seek(times.during);
  assert.equal(await page.locator('#speed-unit').inputValue(),'mach');
  assert.equal(await page.locator('#altitude-unit').inputValue(),'m');
  assert.equal(await page.locator('#distance-unit').inputValue(),'km');
  await page.locator('#open-settings').click();await page.locator('#reset-units').click();await page.locator('#close-settings').click();await page.waitForTimeout(200);
  assert.equal(await page.locator('#speed-unit').inputValue(),'mps');
  assert.equal(await page.locator('#altitude-unit').inputValue(),'ft');
  assert.equal(await page.locator('#distance-unit').inputValue(),'nm');
  assert.ok((await page.locator('#east').textContent()).endsWith('NM'));
  assert.equal((await page.locator('#speed-unit-label').textContent()).trim(),'m/s');
  assert.equal((await page.locator('#altitude-unit-label').textContent()).trim(),'ft');
  await page.evaluate(async time=>{
    const {scene,buffer}=await import('/app.js');
    const {MODEL_LENGTHS,velocityVector}=await import('/units.js');
    const THREE=await import('three'),models=await import('/aircraft3d.js');
    const plane=models.createF16(),missile=models.createMissile();
    const a=new THREE.Box3().setFromObject(plane),b=new THREE.Box3().setFromObject(missile);
    if(Math.abs((a.max.z-a.min.z)-MODEL_LENGTHS.aircraft)>1e-6)throw new Error('Aircraft dimension');
    if(Math.abs((b.max.z-b.min.z)-MODEL_LENGTHS.missile)>1e-6)throw new Error('Missile dimension');
    models.disposeObject(plane);models.disposeObject(missile);
    for(const [id,record] of scene.models){
      const entity=buffer.at(id,time);if(!entity)continue;
      if(record.model.scale.x!==(entity.kind==='missile'?187.5:50))throw new Error('Incorrect aircraft/missile magnification');
      if(record.stem.visible!==(entity.kind==='aircraft')||record.velocityArrow.visible!==Boolean(velocityVector(entity)))throw new Error('Incorrect guides');
      if(entity.kind==='missile'&&(record.altitudeTrace.visible||record.ring.visible))throw new Error('Missile altitude guide');
      if(entity.kind==='missile'&&record.altitudeCurtain)throw new Error('Missile curtain');
      if(entity.kind==='aircraft'){
        if(!record.altitudeCurtain?.visible)throw new Error('Missing aircraft curtain');
        if(record.altitudeCurtain.material.color.getHex()!==(entity.team==='red'?0xe88480:0x65afe8))throw new Error('Curtain team color mismatch');
        if(record.stem.material.opacity>.75||record.altitudeTrace.material.opacity!==.12)throw new Error('Old line style changed');
        const geometry=record.altitudeCurtain.geometry,positions=geometry.attributes.position,colors=geometry.attributes.color;
        if(!geometry.index.count||colors.itemSize!==4)throw new Error('Missing filled geometry');
        for(let i=0;i<colors.count;i+=2){
          if(colors.getW(i)!==1||colors.getW(i+1)!==0||positions.getY(i+1)!==4)throw new Error('Curtain gradient mismatch');
        }
      }
      const stem=record.stem.geometry.attributes.position;
      if(Math.abs(stem.getY(1)-entity.altitude_m)>.002||stem.getY(0)!==4)throw new Error('Altitude guide mismatch');
      const velocity=velocityVector(entity);
      if(velocity){
        const direction=new THREE.Vector3(0,1,0).applyQuaternion(record.velocityArrow.quaternion);
        const expected=new THREE.Vector3(velocity.east,velocity.up,-velocity.north).normalize();
        if(direction.distanceTo(expected)>.001)throw new Error('Velocity direction mismatch');
      }
    }
    const aircraft=buffer.entities(time).find(entity=>entity.kind==='aircraft');
    for(const pitch of [-30,0,30])for(const heading of [0,90,180,270]){
      const entities=buffer.entities(time).map(entity=>entity.id===aircraft.id?{...entity,pitch_deg:pitch,heading_deg:heading,roll_deg:45,velocity_east_mps:300,velocity_north_mps:0,velocity_up_mps:40}:entity);
      scene.draw(entities,buffer,time,aircraft.id);
      const arrow=scene.models.get(aircraft.id).velocityArrow;
      const direction=new THREE.Vector3(0,1,0).applyQuaternion(arrow.quaternion);
      if(direction.distanceTo(new THREE.Vector3(300,40,0).normalize())>.001)throw new Error('Velocity incorrectly follows attitude');
      if(Math.abs(arrow.cone.position.y-Math.hypot(300,40)*scene.velocityMetersPerMps)>.001)throw new Error('Velocity length is not proportional to speed');
    }
    scene.draw(buffer.entities(time),buffer,time,aircraft.id);
  },Number(await page.locator('#timeline').inputValue()));
  await page.locator('#view-options summary').click();
  await page.locator('#grid').uncheck();await page.waitForTimeout(150);
  await page.locator('#label-names').uncheck();await page.locator('#label-altitude').uncheck();await page.waitForTimeout(150);
  assert.ok(await page.evaluate(async()=>{const {scene}=await import('/app.js');return [...scene.models.values()].some(record=>!record.label.hidden&&record.labelName.hidden&&record.labelAltitudeValue.hidden&&record.labelSpeedValue.textContent.endsWith('m/s'));}));
  await page.locator('#label-speed').uncheck();await page.waitForTimeout(150);
  assert.ok(await page.evaluate(async()=>{const {scene}=await import('/app.js');return [...scene.models.values()].every(record=>record.label.hidden);}));
  await page.locator('#label-speed').check();await page.locator('#label-altitude').check();await page.locator('#label-names').check();
  assert.ok(await page.evaluate(async()=>{const {scene,buffer}=await import('/app.js');return [...scene.models].every(([id,record])=>record.stem.visible===(buffer.tracks.get(id).samples[0].kind==='aircraft'));}));
  await page.locator('#object-scale').selectOption('25');await page.waitForTimeout(150);
  assert.ok(await page.evaluate(async()=>{const {scene,buffer}=await import('/app.js');return [...scene.models].every(([id,record])=>record.model.scale.x===(buffer.tracks.get(id).samples[0].kind==='missile'?93.75:25));}));
  await page.locator('#object-scale').selectOption('1');await page.waitForTimeout(150);
  assert.ok(await page.evaluate(async()=>{const {scene}=await import('/app.js');return [...scene.models.values()].every(record=>record.model.scale.x===1);}));
  await page.locator('#object-scale').selectOption('1');await page.locator('#view-options').evaluate(menu=>menu.open=true);await page.locator('#grid').check();
  await page.locator('#view-options summary').click();
  await page.locator('#view-options summary').click();await page.locator('#altitude-guides').click();await page.locator('#view-options summary').click();await page.waitForTimeout(150);
  assert.equal(await page.locator('#altitude-guides').isChecked(),false);
  assert.ok(await page.evaluate(async()=>{const {scene}=await import('/app.js');return [...scene.models.values()].every(record=>!record.stem.visible&&!record.altitudeTrace.visible&&!record.ring.visible&&!record.altitudeCurtain?.visible);}));
  await page.locator('#view-options summary').click();await page.locator('#altitude-guides').click();await page.locator('#view-options summary').click();await page.waitForTimeout(150);
  assert.equal(await page.locator('#altitude-guides').isChecked(),true);
  async function readScale(){
    assert.equal(await page.locator('#distance-scale').isVisible(),true);
    const scale=await page.locator('#distance-scale').evaluate(element=>({
      metersPerPixel:Number(element.dataset.metersPerPixel),meters:Number(element.dataset.meters),
      pixels:element.querySelector('.distance-scale-bar').getBoundingClientRect().width
    }));
    assert.ok(scale.pixels>30&&scale.pixels<=140.1);
    assert.ok(Math.abs(scale.pixels*scale.metersPerPixel/scale.meters-1)<.001);
    return scale;
  }
  const scale3d=await readScale();
  await page.locator('#zoom-in').click();await page.waitForTimeout(250);
  assert.ok((await readScale()).metersPerPixel<scale3d.metersPerPixel);
  await page.evaluate(async()=>{
    const {scene}=await import('/app.js'),THREE=await import('three');
    const bar=document.querySelector('.distance-scale-bar'),distance=Number(document.querySelector('#distance-scale').dataset.meters);
    const start=scene.controls.target.clone(),end=start.clone().add(new THREE.Vector3(1,0,0).applyQuaternion(scene.camera.quaternion).multiplyScalar(distance));
    start.project(scene.camera);end.project(scene.camera);
    const projected=(end.x-start.x)*scene.canvas.clientWidth/2;
    if(Math.abs(projected-bar.getBoundingClientRect().width)>.1)throw new Error('3D distance ruler projection');
  });
  await page.locator('#zoom-out').click();await page.waitForTimeout(250);
  await page.locator('#view-2d').click();await page.waitForTimeout(250);
  const scale2d=await readScale();
  await page.evaluate(async()=>{
    const {scene,buffer}=await import('/app.js');
    const time=Number(document.querySelector('#timeline').value),entities=buffer.entities(time),seen=[];
    const ctx=scene.flat.ctx,original=ctx.fillText;
    ctx.fillText=function(text,...args){seen.push(text);return original.call(this,text,...args);};
    try{scene.flat.draw(entities,buffer,time,entities[0].id);}finally{ctx.fillText=original;}
    for(const entity of entities){
      const p=scene.flat.point(entity);if(p.x< -70||p.x>scene.flat.width+70||p.y< -70||p.y>scene.flat.height+70)continue;
      const record=scene.models.get(entity.id);
      if(!seen.includes(record.labelSpeedValue.textContent)||!seen.includes(record.labelAltitudeValue.textContent))throw new Error('2D label does not match 3D altitude and speed');
    }
    if(!seen.some(text=>text.endsWith('ft'))||!seen.some(text=>text.endsWith('m/s')))throw new Error('2D speed labels missing');
  });
  assert.ok(await page.evaluate(async()=>{const {scene}=await import('/app.js');return Math.abs(Number(document.querySelector('#distance-scale').dataset.metersPerPixel)-1/scene.flat.scale)<1e-8;}));
  await page.locator('#scene2d').dispatchEvent('wheel',{deltaY:100});await page.waitForTimeout(250);
  assert.ok((await readScale()).metersPerPixel>scale2d.metersPerPixel);
  await page.locator('#view-3d').click();await page.waitForTimeout(250);
  async function checkFollow(){
    assert.equal(await page.locator('#follow').isChecked(),true);
    await page.evaluate(async()=>{
      const {scene}=await import('/app.js');
      const record=[...scene.models.values()].find(record=>record.label.classList.contains('selected'));
      const projected=record.model.position.clone().project(scene.camera);
      if(Math.abs(projected.x)>1e-8||Math.abs(projected.y)>1e-8||scene.controls.target.distanceTo(record.model.position)>1e-8)throw new Error('Follow object is not centered');
    });
  }
  await checkFollow();
  const surface=await page.locator('#scene').boundingBox();
  await page.mouse.move(surface.x+surface.width/2,surface.y+surface.height*.7);
  await page.mouse.down({button:'right'});await page.mouse.move(surface.x+surface.width/2+150,surface.y+surface.height*.7-70,{steps:6});await page.mouse.up({button:'right'});
  await page.waitForTimeout(300);
  assert.equal(await page.locator('#follow').isChecked(),false);
  const offset=await page.evaluate(async()=>{const {scene}=await import('/app.js');return scene.camera.position.clone().sub(scene.controls.target).toArray();});
  await toggleFollow();await page.waitForTimeout(250);await checkFollow();
  await page.evaluate(async offset=>{
    const {scene}=await import('/app.js'),THREE=await import('three');
    if(scene.camera.position.clone().sub(scene.controls.target).distanceTo(new THREE.Vector3(...offset))>.001)throw new Error('Recenter changed camera angle or zoom');
  },offset);
  await page.locator('#entity-list .entity[data-team="red"]').click();await page.waitForTimeout(200);await checkFollow();
  await seek(times.before);await checkFollow();
  await seek(times.during);await checkFollow();
  await page.locator('#zoom-in').click();await page.waitForTimeout(200);await checkFollow();
  await page.locator('#zoom-out').click();await page.locator('#view-2d').click();await page.waitForTimeout(200);
  await page.evaluate(async()=>{
    const {scene,buffer}=await import('/app.js');
    const id=document.querySelector('.entity.selected').closest('.aircraft-card').dataset.id;
    const entity=buffer.at(id,Number(document.querySelector('#timeline').value)),p=scene.flat.point(entity);
    if(Math.abs(p.x-scene.flat.width/2)>1e-8||Math.abs(p.y-scene.flat.height/2)>1e-8)throw new Error('2D follow is not centered');
  });
  await page.locator('#view-3d').click();await page.locator('#entity-list .entity[data-team="blue"]').click();await page.waitForTimeout(200);await checkFollow();
  if(options.screenshot)await page.screenshot({path:options.screenshot.replace('.png','-native.png'),fullPage:true});
  assert.equal(await page.locator('#record-drop').count(),0);
  assert.ok((await page.locator('#record-source').textContent()).includes(path.basename(options.native)));
  assert.equal(await page.locator('#object-scale option:checked').textContent(),'1×');
  assert.equal(await page.locator('#open-settings').evaluate(node=>getComputedStyle(node).userSelect),'none');
  await page.evaluate(async()=>{const {scene}=await import('/app.js');scene.follow=false;scene.zoom(.3);});
  const beforeFocus=await page.evaluate(async()=>{const {scene}=await import('/app.js');return scene.camera.position.distanceTo(scene.controls.target);});
  await page.locator('#entity-list .entity[data-team="red"]').dblclick();await page.waitForTimeout(250);
  await checkFollow();
  assert.ok(await page.evaluate(async before=>{const {scene}=await import('/app.js');return scene.camera.position.distanceTo(scene.controls.target)<before;},beforeFocus));
  await page.evaluate(async()=>{const {scene}=await import('/app.js');scene.follow=false;});await page.locator('.flight-label').filter({has:page.locator('.flight-label-name', {hasText:'Red'})}).dblclick();await page.waitForTimeout(200);await checkFollow();
  await page.locator('#toggle-theme').click();await page.waitForTimeout(200);
  assert.ok(await page.evaluate(async()=>{const {scene}=await import('/app.js');return [...scene.models.values()].every(record=>[0x0756a5,0xae2432,0xa83215,0x8a5700].includes(record.trail.material.color.getHex()));}));
  if(options.screenshot)await page.screenshot({path:options.screenshot.replace('.png','-day-3d.png'),fullPage:true});
  await page.locator('#view-2d').click();await page.waitForTimeout(200);
  await page.evaluate(async()=>{
    const {scene}=await import('/app.js'),flat=scene.flat,rect=flat.canvas.getBoundingClientRect();
    const position=flat.positions.find(p=>p.label);
    if(!position)throw new Error('Missing 2D label');
    if(flat.hitTest({clientX:rect.left+position.label.right-2,clientY:rect.top+position.label.y+3})?.id!==position.id)throw new Error('2D label edge cannot be selected');
    const saved=flat.positions;
    try{
      flat.positions=[{id:'far',x:100,y:100},{id:'near',x:110,y:100}];
      if(flat.hitTest({clientX:rect.left+111,clientY:rect.top+100})?.id!=='near')throw new Error('2D selected the first object instead of the nearest');
    }finally{flat.positions=saved;}
  });
  const flatLabel=await page.evaluate(async()=>{const {scene}=await import('/app.js');scene.follow=false;const label=scene.flat.positions.find(p=>p.label).label;const rect=scene.canvas2d.getBoundingClientRect();return {x:rect.x+label.x+5,y:rect.y+label.y+5};});
  await page.mouse.dblclick(flatLabel.x,flatLabel.y);await page.waitForTimeout(200);
  assert.equal(await page.locator('#follow').isChecked(),true);
  if(options.screenshot)await page.screenshot({path:options.screenshot.replace('.png','-day-2d.png'),fullPage:true});
  await page.locator('#toggle-theme').click();await page.locator('#view-3d').click();

  await page.locator('#entity-list .munition.flying').first().click();await page.waitForTimeout(200);
  assert.equal(await page.locator('#detail-title').textContent(),'Füze bilgisi');
  assert.equal(await page.locator('#speed-measure').isVisible(),true);
  await page.locator('#record-end').click();await page.waitForTimeout(250);
  assert.equal(await page.locator('#entity-list .munition.selected').count(),1);
  const bad=path.join(temporary,'bad');await fs.mkdir(bad);await fs.writeFile(path.join(bad,'F-16 (bad) [Blue].csv'),'Time\n0');
  await chooseDirectory(bad);
  await page.waitForFunction(()=>document.querySelector('#status').textContent==='Kayıt okunamadı');
  assert.ok((await page.locator('#scene-message').textContent()).includes('sütunları'));
  // The previous valid flight and its name survive a failed replacement.
  assert.ok(Number(await page.locator('#timeline').getAttribute('max'))<2);
  await chooseDirectory(options.recording);
  await page.waitForFunction(()=>Number(document.querySelector('#timeline').max)>1199);
  await page.waitForFunction(()=>!document.querySelector('#record-options').open);
  await seek(780);assert.equal(await page.locator('#entity-list .entity').count(),2);
  assert.equal(await page.locator('#entity-list .munition').count(),8);
  assert.ok(await page.locator('#entity-list .munition.spent').count()>0);
  assert.ok(await page.evaluate(async()=>{const {scene}=await import('/app.js');return [...scene.models.values()].some(record=>!record.model.visible&&record.trail.visible);}));
  await page.locator('#camera-mode').selectOption('all');
  assert.equal(await page.locator('#follow').isChecked(),false);
  await page.locator('#seek-time').fill('10');await page.locator('#seek-time').press('Enter');
  await page.waitForTimeout(200);assert.ok(Math.abs(Number(await page.locator('#timeline').inputValue())-10)<.01);
  await page.locator('#playback-speed').focus();await page.keyboard.press('Space');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'false');
  await page.keyboard.press('Space');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'true');
  await page.locator('#timeline').focus();await page.keyboard.down('Space');await page.keyboard.down('Space');await page.keyboard.up('Space');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'false');
  await page.keyboard.press('Space');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'true');
  const follow=await page.locator('#follow').isChecked();
  await page.locator('#zoom-in').focus();await page.keyboard.press('Space');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'false');
  assert.equal(await page.locator('#follow').isChecked(),follow);
  await page.keyboard.press('Space');
  await page.locator('#seek-time').focus();await page.keyboard.press('Space');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'true');
  await page.locator('#seek-time').press('Escape');await page.locator('#seek-time').evaluate(input=>input.blur());
  await seek(10);
  await page.locator('#playback-speed').focus();await page.keyboard.press('ArrowDown');
  assert.equal(await page.locator('#playback-rate').textContent(),'0,5×');
  await page.keyboard.press('ArrowDown');await page.keyboard.press('ArrowDown');
  assert.equal(await page.locator('#playback-rate').textContent(),'0,25×');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'true');
  await page.keyboard.press('Space');
  const quarterStart=Number(await page.locator('#timeline').inputValue());await page.waitForTimeout(800);
  const quarterAdvance=Number(await page.locator('#timeline').inputValue())-quarterStart;assert.ok(quarterAdvance>.13&&quarterAdvance<.32);
  await page.keyboard.press('ArrowUp');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'false');
  assert.equal(await page.locator('#playback-rate').textContent(),'0,5×');
  const halfStart=Number(await page.locator('#timeline').inputValue());await page.waitForTimeout(800);
  const halfAdvance=Number(await page.locator('#timeline').inputValue())-halfStart;assert.ok(halfAdvance>.3&&halfAdvance<.6);
  await page.keyboard.press('Space');
  for(const rate of [1,2,3,4,5,10]){
    await page.keyboard.press('ArrowUp');
    assert.equal(await page.locator('#playback-rate').textContent(),rate+'×');
    assert.equal(await page.locator(`[data-rate="${rate}"]`).getAttribute('aria-pressed'),'true');
  }
  await page.keyboard.press('ArrowUp');assert.equal(await page.locator('#playback-rate').textContent(),'10×');
  await page.locator('[data-rate="1"]').click();
  await page.locator('#playback-speed').fill('3');await page.locator('#play-pause').click();
  const start=Number(await page.locator('#timeline').inputValue());await page.waitForTimeout(1000);
  const end=Number(await page.locator('#timeline').inputValue());assert.ok(end-start>1.4&&end-start<2.7);
  await page.locator('#play-pause').focus();
  const beforeBack=Number(await page.locator('#timeline').inputValue());await page.keyboard.press('ArrowLeft');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'false');
  const afterBack=Number(await page.locator('#timeline').inputValue());assert.ok(afterBack<beforeBack-.5&&afterBack>beforeBack-1.1);
  await page.waitForFunction(time=>Number(document.querySelector('#timeline').value)>time+.3,afterBack,{timeout:3000});
  const beforeForward=Number(await page.locator('#timeline').inputValue());await page.keyboard.press('Shift+ArrowRight');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'false');
  assert.ok(Number(await page.locator('#timeline').inputValue())>beforeForward+9.9);
  await page.locator('#timeline').focus();await page.keyboard.press('ArrowLeft');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'false');
  await page.keyboard.press('Space');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'true');
  const pausedTime=Number(await page.locator('#timeline').inputValue());await page.keyboard.press('ArrowRight');
  assert.equal(await page.locator('#play-pause').getAttribute('aria-pressed'),'true');
  assert.ok(Math.abs(Number(await page.locator('#timeline').inputValue())-pausedTime-1)<.01);
  const pausedAfterSeek=Number(await page.locator('#timeline').inputValue());await page.waitForTimeout(200);
  assert.equal(Number(await page.locator('#timeline').inputValue()),pausedAfterSeek);
  await seek(780);await page.locator('#playback-speed').fill('7');await page.waitForTimeout(200);
  assert.equal(await page.locator('#playback-rate').textContent(),'10×');
  assert.equal(await page.locator('#status').textContent(),'Duraklatıldı');
  await page.locator('#view-options summary').click();await page.locator('#label-mode').selectOption('selected');
  await page.locator('#label-altitude').uncheck();await page.locator('#label-speed').uncheck();await page.locator('#view-options summary').click();
  const left=await page.locator('#objects-resizer').boundingBox();
  await page.mouse.move(left.x+3,left.y+100);await page.mouse.down();await page.mouse.move(left.x+53,left.y+100);await page.mouse.up();
  assert.equal(await page.locator('#objects-resizer').getAttribute('aria-valuenow'),'260');
  const dock=await page.locator('#dock-resizer').boundingBox();
  await page.mouse.move(dock.x+dock.width/2,dock.y+4);await page.mouse.down();await page.mouse.move(dock.x+dock.width/2,dock.y+64);await page.mouse.up();
  assert.equal(await page.locator('#dock-resizer').getAttribute('aria-valuenow'),'68');
  for(const id of ['play-pause','record-end','restart-playback','playback-speed'])assert.equal(await page.locator('#'+id).isVisible(),true);
  assert.ok((await page.locator('#scene').boundingBox()).height>790);
  if(options.screenshot)await page.screenshot({path:options.screenshot,fullPage:true});
  await page.reload();await page.waitForFunction(()=>Number(document.querySelector('#timeline').max)>1199);
  assert.equal(await page.locator('#objects-resizer').getAttribute('aria-valuenow'),'260');
  assert.equal(await page.locator('#label-mode').inputValue(),'selected');
  assert.equal(await page.locator('#label-altitude').isChecked(),false);
  assert.equal(await page.locator('#label-speed').isChecked(),false);
  for(const file of await files(options.native))await fs.copyFile(file,path.join(recording,path.basename(file)));
  await chooseDirectory(recording);
  await page.waitForFunction(()=>Number(document.querySelector('#timeline').max)<2);
  await page.locator('#open-recording').click();
  assert.equal(await page.locator('#directory-browser').count(),0);
  assert.equal(await page.locator('#change-directory').textContent(),'Kayıt klasörü değiştir');
  assert.equal(await page.locator('#file-input').count(),0);
  assert.equal(await page.locator('#recent-recordings').count(),0);
  await page.locator('#close-record-panel').click();
  await page.locator('#view-2d').click();await page.locator('#camera-mode').selectOption('all');await page.waitForTimeout(200);
  assert.equal(await page.locator('#scene2d').isVisible(),true);
  await page.setViewportSize({width:430,height:850});await page.waitForTimeout(300);
  await readScale();
  const ruler=await page.locator('.scene-reference').boundingBox(),zoom=await page.locator('.zoom').boundingBox();
  assert.ok(ruler.x>=0&&ruler.y>=0&&ruler.x+ruler.width<zoom.x&&ruler.y+ruler.height<=850);
  assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+2));
  assert.equal(await page.locator('#dock-resizer').getAttribute('aria-valuenow'),'128');
  for(const id of ['play-pause','record-end','playback-speed'])assert.equal(await page.locator('#'+id).isVisible(),true);
  for(const id of ['scene2d','play-pause','record-end','playback-speed']){
    const rect=await page.locator('#'+id).boundingBox();
    assert.ok(rect.width>0&&rect.height>0&&rect.x>=0&&rect.x+rect.width<=431&&rect.y+rect.height<=851,id+' must fit the viewport');
  }
  if(options.screenshot)await page.screenshot({path:options.screenshot.replace('.png','-mobile.png'),fullPage:true});
  await page.locator('#open-settings').click();
  await page.waitForFunction(()=>{const menu=document.querySelector('#settings-options');const box=document.querySelector('#settings-panel').getBoundingClientRect();return menu.open&&box.x>=0&&box.x+box.width<=innerWidth&&box.y>=0&&box.y+box.height<=innerHeight;});const settings=await page.locator('#settings-panel').boundingBox();
  assert.ok(settings.x>=0&&settings.y>=0&&settings.x+settings.width<=430&&settings.y+settings.height<=850);
  for(const id of ['speed-unit','altitude-unit','distance-unit','reset-units'])assert.equal(await page.locator('#'+id).isVisible(),true);
  if(options.screenshot)await page.screenshot({path:options.screenshot.replace('.png','-settings-mobile.png'),fullPage:true});
  await page.locator('#close-settings').click();
  assert.deepEqual(errors,[]);
  console.log(JSON.stringify({passed:true,nativeObjects:geometry,inventory:true,estimatedSpeed:true,nauticalMiles:true,sharedScale:true,velocityGuides:true,altitudeGuides:true,importValidation:true,folderSelection:true,directorySelection:true,replay2x:true,retainedMissileTrails:true,preferences:true,panelResize:true,compactDock:true,mobile:true,errors}));
}finally{
  if(browser)await browser.close();
  if(child.exitCode===null){child.kill();await new Promise(resolve=>child.once('exit',resolve));}
  const resolved=path.resolve(temporary),prefix=path.resolve(os.tmpdir())+path.sep;
  if(!resolved.startsWith(prefix)||!path.basename(resolved).startsWith('bvrgym-csv-qa-'))throw new Error('Unsafe cleanup path');
  await fs.rm(resolved,{recursive:true,force:true});
}








