from pathlib import Path

exec(Path(__file__).with_name('ui-review.py').read_text(encoding='utf-8').split('fixture = OUT /')[0])
OUT = ROOT / 'tmp' / 'missile-label-review'
OUT.mkdir(exist_ok=True)
fixture = OUT / 'flight'
fixture.mkdir(exist_ok=True)
header = 'time_s,lon_deg,lat_deg,alt_m,roll_deg,pitch_deg,yaw_deg,tas_kt\n'
for name, team, key, lon in [('KZL', 'Blue', 'blue', 18), ('Hostile', 'Red', 'red', 18.1)]:
    for model, suffix, lat in [('F-16', '', 59), ('AIM-120 AMRAAM', '--0', 59.01)]:
        rows = ''.join(f'{t},{lon+t*.0001},{lat},7000,0,0,90,1000\n' for t in [0,10,20])
        (fixture / f'{model} ({name}--{key}{suffix}) [{team}].csv').write_text(header+rows,encoding='utf-8')
import csv
with (fixture/'calculations.csv').open('w',newline='',encoding='utf-8') as handle:
    writer=csv.writer(handle)
    writer.writerow(['time_s','id','target_id','pitbull_missile_slots'])
    for t in [0,10]:
        writer.writerow([t,'blue','red','[0]' if t==10 else '[]'])
        writer.writerow([t,'red','blue','[]'])
port,debug_port=free_port(),free_port()
flags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0
server=subprocess.Popen([sys.executable,'-B',str(ROOT/'BVRGym-Goruntuleyici/viewer.py'),'--recording',str(fixture),'--port',str(port)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=flags)
chrome=None
chrome_log=(OUT/'chrome.log').open('w')
try:
    until(lambda:get(f'http://127.0.0.1:{port}/api/state').get('phase')=='ready')
    chrome=subprocess.Popen([r'C:\Program Files\Google\Chrome\Application\chrome.exe','--headless=new',f'--remote-debugging-port={debug_port}',f'--user-data-dir={OUT/("profile-"+str(debug_port))}','--no-first-run','--no-default-browser-check','--use-angle=swiftshader','--enable-unsafe-swiftshader','about:blank'],stdout=chrome_log,stderr=chrome_log,creationflags=flags)
    tabs=until(lambda:get(f'http://127.0.0.1:{debug_port}/json'))
    cdp=CDP(next(tab['webSocketDebuggerUrl'] for tab in tabs if tab['type']=='page'))
    cdp.call('Page.enable');cdp.call('Runtime.enable')
    cdp.call('Emulation.setDeviceMetricsOverride',width=1440,height=900,deviceScaleFactor=1,mobile=False)
    cdp.call('Page.addScriptToEvaluateOnNewDocument',source='window.reviewErrors=[];window.addEventListener("error",e=>reviewErrors.push(e.message));window.addEventListener("unhandledrejection",e=>reviewErrors.push(String(e.reason)));')
    cdp.call('Page.navigate',url=f'http://127.0.0.1:{port}')
    until(lambda:cdp.js('document.querySelector("#timeline")&&!document.querySelector("#timeline").disabled'))
    result=cdp.js('''(async()=>{
      const {scene,buffer}=await import('/app.js');
      const blue=[...buffer.tracks.values()].find(t=>t.samples[0].kind==='missile'&&t.samples[0].team==='blue').samples[0];
      const red=[...buffer.tracks.values()].find(t=>t.samples[0].kind==='missile'&&t.samples[0].team==='red').samples[0];
      if(buffer.at(blue.id,9.9).is_pitbull!==false||buffer.at(blue.id,10).is_pitbull!==true||buffer.at(blue.id,20).is_pitbull!==true||buffer.at(red.id,10).is_pitbull!==false)throw Error('Pitbull timing/carrier/final latch');
      scene.follow=false;scene.frameAll(buffer.entities(10));
      scene.draw(buffer.entities(10),buffer,10,null);
      const label=scene.models.get(blue.id).label,record=scene.models.get(blue.id);
      if(record.labelPitbull.hidden||!record.labelFlightValue.hidden)throw Error('Compact label/PB');
      const compact={width:label.offsetWidth,height:label.offsetHeight};
      scene.draw(buffer.entities(10),buffer,10,blue.id);
      const expanded={width:label.offsetWidth,height:label.offsetHeight};
      if(record.labelFlightValue.hidden||expanded.height<=compact.height)throw Error('Selected label expansion');
      scene.draw(buffer.entities(0),buffer,0,null);
      if(!record.labelPitbull.hidden)throw Error('PB persists before activation on seek');
      return {compact,expanded};
    })()''')
    cdp.js('document.querySelector("#seek-time").value="10";document.querySelector("#seek-time").dispatchEvent(new Event("change"));')
    time.sleep(.3);cdp.screenshot('missile-labels-3d.png')
    cdp.js('document.querySelector("#view-2d").click()')
    cdp.js('''(async()=>{
      const {scene,buffer}=await import('/app.js'),flat=scene.flat,ctx=flat.ctx;
      const missile=buffer.entities(10).find(e=>e.kind==='missile'&&e.team==='blue');
      flat.follow=false;flat.center={east:missile.east_m,north:missile.north_m};flat.scale=.02;
      const texts=[],original=ctx.fillText;ctx.fillText=function(text,...rest){texts.push(text);return original.call(this,text,...rest);};
      try{
        flat.draw([missile],buffer,10,null);
        const compact=flat.positions[0].label;
        if(!texts.includes('PB')||texts.some(t=>t.startsWith('t+')))throw Error('2D compact badge');
        texts.length=0;flat.draw([missile],buffer,10,missile.id);
        if(!texts.some(t=>t.startsWith('t+')))throw Error('2D selected flight details');
        if(flat.positions[0].label.bottom-flat.positions[0].label.y<=compact.bottom-compact.y)throw Error('2D compact height');
        texts.length=0;flat.draw(buffer.entities(0).filter(e=>e.kind==='missile'),buffer,0,null);
        if(texts.includes('PB'))throw Error('2D badge before pitbull');
      }finally{ctx.fillText=original;}
    })()''')
    time.sleep(.3);cdp.screenshot('missile-labels-2d.png')
    assert not cdp.js('window.reviewErrors'),cdp.js('window.reviewErrors')
    print(json.dumps({'passed':True,'labels':result,'checks':['2D/3D compact labels','selected flight details','per-missile PB timing','backward seek','missing final calculation','no browser errors']},ensure_ascii=False))
finally:
    if chrome:chrome.terminate();chrome.wait(timeout=15)
    server.terminate();server.wait(timeout=15)
    chrome_log.close()
