from pathlib import Path

exec(Path(__file__).with_name('ui-review.py').read_text(encoding='utf-8').split('fixture = OUT /')[0])
OUT=ROOT/'tmp'/'velocity-review';OUT.mkdir(exist_ok=True)
fixture=OUT/'flight';fixture.mkdir(exist_ok=True)
header='time_s,lon_deg,lat_deg,alt_m,roll_deg,pitch_deg,yaw_deg,vel_north_fps,vel_east_fps,vel_down_fps\n'
for name,team,lon,north,east,up in [('KZL--blue','Blue',18,0,300,40),('Hostile--red','Red',18.08,-600,0,0)]:
    rows=''.join(f'{t},{lon+t*.0001},59,{7000+up*t},0,0,0,{north/.3048},{east/.3048},{-up/.3048}\n' for t in [0,30,60])
    (fixture/f'F-16 ({name}) [{team}].csv').write_text(header+rows,encoding='utf-8')
(fixture/'AIM-120 AMRAAM (KZL--blue--0) [Blue].csv').write_text(header+''.join(f'{t},{18.01+(t-10)*.001},59,{7000+(t-10)*100},0,0,0,0,{900/.3048},{-100/.3048}\n' for t in [10,20,30]),encoding='utf-8')
(fixture/'F-16 (Wing) [Blue].csv').write_text('time_s,lon_deg,lat_deg,alt_m\n0,18.02,59.01,7000\n80,18.02,59.02,7000\n',encoding='utf-8')
port,debug_port=free_port(),free_port();flags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0
server=subprocess.Popen([sys.executable,'-B',str(ROOT/'BVRGym-Goruntuleyici/viewer.py'),'--recording',str(fixture),'--port',str(port)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=flags)
chrome=None
try:
    until(lambda:get(f'http://127.0.0.1:{port}/api/state').get('phase')=='ready')
    chrome=subprocess.Popen([r'C:\Program Files\Google\Chrome\Application\chrome.exe','--headless=new',f'--remote-debugging-port={debug_port}',f'--user-data-dir={OUT/("profile-"+str(debug_port))}','--no-first-run','--no-default-browser-check','--use-angle=swiftshader','--enable-unsafe-swiftshader','about:blank'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=flags)
    tabs=until(lambda:get(f'http://127.0.0.1:{debug_port}/json'));cdp=CDP(next(tab['webSocketDebuggerUrl'] for tab in tabs if tab['type']=='page'))
    cdp.call('Page.enable');cdp.call('Runtime.enable');cdp.call('Network.enable');cdp.call('Network.setBlockedURLs',urls=['https://tile.openstreetmap.org/*'])
    cdp.call('Emulation.setDeviceMetricsOverride',width=1440,height=900,deviceScaleFactor=1,mobile=False)
    cdp.call('Page.addScriptToEvaluateOnNewDocument',source='window.reviewErrors=[];window.addEventListener("error",e=>{if(e.message)reviewErrors.push(e.message)});window.addEventListener("unhandledrejection",e=>reviewErrors.push(String(e.reason)));if(!localStorage.getItem("velocity-test-seeded")){localStorage.setItem("bvrgym-layout-v2",JSON.stringify({headingGuides:false}));localStorage.setItem("velocity-test-seeded","1");}')
    cdp.call('Page.navigate',url=f'http://127.0.0.1:{port}')
    until(lambda:cdp.js('document.querySelector("#timeline")&&!document.querySelector("#timeline").disabled'))
    assert cdp.js('!document.querySelector("#velocity-guides").checked')
    cdp.js('document.querySelector("#velocity-guides").checked=true;document.querySelector("#velocity-guides").dispatchEvent(new Event("change"));document.querySelector("#seek-time").value="15";document.querySelector("#seek-time").dispatchEvent(new Event("change"));')
    until(lambda:cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.models.size===4})()'))
    results=cdp.js('''(async()=>{
      const {scene,buffer}=await import('/app.js'),THREE=await import('three');const entities=buffer.entities(15);
      const plane=entities.find(e=>e.name==='KZL'),missile=entities.find(e=>e.kind==='missile'),wing=entities.find(e=>e.name==='Wing');
      const check=e=>{const arrow=scene.models.get(e.id).velocityArrow;if(!arrow.visible)throw Error('Missing velocity arrow');const direction=new THREE.Vector3(0,1,0).applyQuaternion(arrow.quaternion);const expected=new THREE.Vector3(e.velocity_east_mps,e.velocity_up_mps,-e.velocity_north_mps).normalize();if(direction.distanceTo(expected)>.00001)throw Error('Incorrect motion direction');return arrow.cone.position.y;};
      const planeLength=check(plane),missileLength=check(missile);check(wing);
      const ratio=Math.hypot(missile.velocity_east_mps,missile.velocity_north_mps,missile.velocity_up_mps)/Math.hypot(plane.velocity_east_mps,plane.velocity_north_mps,plane.velocity_up_mps);
      if(Math.abs(missileLength/planeLength-ratio)>.00001)throw Error('Missile/aircraft scale differs');
      scene.draw(entities.map(e=>e.id===plane.id?{...e,heading_deg:180,pitch_deg:-45,velocity_east_mps:e.velocity_east_mps*2,velocity_north_mps:e.velocity_north_mps*2,velocity_up_mps:e.velocity_up_mps*2}:e),buffer,15,plane.id);
      const arrow=scene.models.get(plane.id).velocityArrow,direction=new THREE.Vector3(0,1,0).applyQuaternion(arrow.quaternion);
      if(direction.x<.9||direction.y<0)throw Error('Arrow follows nose instead of velocity');if(Math.abs(arrow.cone.position.y/planeLength-2)>.0001)throw Error('Double speed not double length');
      scene.draw(entities.map(e=>e.id===plane.id?{...e,velocity_east_mps:0,velocity_north_mps:0,velocity_up_mps:0}:e),buffer,15,plane.id);if(arrow.visible)throw Error('Stationary arrow');
      scene.draw(entities,buffer,15,plane.id);return {planeLength,missileLength,ratio,wingSource:wing.velocity_source};
    })()''')
    time.sleep(.3);cdp.screenshot('velocity-3d.png')
    cdp.js('document.querySelector("#view-2d").click()');time.sleep(.3)
    cdp.js('''(async()=>{
      const {scene,buffer}=await import('/app.js');const flat=scene.flat,e=buffer.entities(15).find(e=>e.name==='KZL'),ctx=flat.ctx;
      flat.grid=false;flat.trail=false;flat.analysis=null;flat.labelMode='none';flat.scale=.045;flat.objectScale=1;
      const strokes=[],saved={};let path=[];for(const name of ['beginPath','moveTo','lineTo','stroke'])saved[name]=ctx[name];
      ctx.beginPath=function(...args){path=[];return saved.beginPath.apply(this,args)};
      for(const name of ['moveTo','lineTo'])ctx[name]=function(x,y){path.push({x,y});return saved[name].call(this,x,y)};
      ctx.stroke=function(...args){if(!args.length)strokes.push([...path]);return saved.stroke.apply(this,args)};
      try{
        flat.draw([{...e,velocity_east_mps:300,velocity_north_mps:0,velocity_up_mps:400}],buffer,15,e.id);
        const arrow=strokes.find(p=>p.length===2&&Math.abs(p[1].x-p[0].x-54)<.001&&Math.abs(p[1].y-p[0].y)<.001);if(!arrow)throw Error('2D horizontal projection not proportional');
        strokes.length=0;flat.draw([{...e,velocity_east_mps:0,velocity_north_mps:0,velocity_up_mps:400}],buffer,15,e.id);
        if(strokes.some(p=>p.length===2))throw Error('Vertical-only velocity must have no horizontal arrow');
      }finally{for(const [name,method] of Object.entries(saved))ctx[name]=method;}
    })()''')
    time.sleep(.2);cdp.screenshot('velocity-2d.png')
    cdp.js('document.querySelector("#velocity-guides").checked=false;document.querySelector("#velocity-guides").dispatchEvent(new Event("change"));document.querySelector("#view-3d").click();')
    time.sleep(.3)
    assert cdp.js('(async()=>{const {scene}=await import("/app.js");return [...scene.models.values()].every(r=>!r.velocityArrow.visible)})()')
    cdp.js('document.querySelector("#velocity-guides").checked=true;document.querySelector("#velocity-guides").dispatchEvent(new Event("change"));document.querySelector("#seek-time").value="65";document.querySelector("#seek-time").dispatchEvent(new Event("change"));')
    time.sleep(.3)
    assert cdp.js('(async()=>{const {scene,buffer}=await import("/app.js");return [...scene.models].every(([id,r])=>buffer.at(id,65)?.measurement_ended||buffer.tracks.get(id).samples[0].record_end_s<65?!r.velocityArrow.visible:true)})()')
    assert not cdp.js('window.reviewErrors'),cdp.js('window.reviewErrors')
    print(json.dumps({'passed':True,'checks':['legacy preference migration','recorded NED motion','position fallback with unknown heading','3D climb and direction','aircraft/missile shared scale','double speed double length','stationary arrow hidden','2D horizontal projection','toggle','ended measurements hidden','no browser errors'],'measurements':results,'screenshots':str(OUT)},ensure_ascii=False),flush=True)
finally:
    if chrome:chrome.terminate();chrome.wait(timeout=15)
    server.terminate();server.wait(timeout=15)
