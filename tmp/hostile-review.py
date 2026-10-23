from pathlib import Path

exec(Path(__file__).with_name('ui-review.py').read_text(encoding='utf-8').split('fixture = OUT /')[0])
OUT=ROOT/'tmp'/'hostile-review'
OUT.mkdir(exist_ok=True)
fixture=OUT/'legacy-flight'
fixture.mkdir(exist_ok=True)
header='time_s,lon_deg,lat_deg,alt_m,roll_deg,pitch_deg,yaw_deg,mach\n'
for name,team,lon,heading in [('KZL--blue','Blue',18,0),('Bogey--red','Red',18.08,180)]:
    (fixture/f'F-16 ({name}) [{team}].csv').write_text(header+f'0,{lon},59,7000,0,0,{heading},0.8\n60,{lon+.01},59,7000,0,0,{heading},0.8\n',encoding='utf-8')
(fixture/'AIM-120 AMRAAM (Bogey--red--0) [Red].csv').write_text(header+'10,18.07,59,7000,0,0,180,2\n30,18.04,59,7000,0,0,180,2\n',encoding='utf-8')
(fixture/'calculations.csv').write_text('time_s,id,target_id,phase,range_3d_m,wez_max_range_m\n0,blue,red,4,9100,31000\n0,red,blue,4,9100,31000\n60,blue,red,4,9100,31000\n60,red,blue,4,9100,31000\n',encoding='utf-8')
port,debug_port=free_port(),free_port()
flags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0
server=subprocess.Popen([sys.executable,'-B',str(ROOT/'BVRGym-Goruntuleyici/viewer.py'),'--recording',str(fixture),'--port',str(port)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=flags)
chrome=None
try:
    until(lambda:get(f'http://127.0.0.1:{port}/api/state').get('phase')=='ready')
    chrome=subprocess.Popen([r'C:\Program Files\Google\Chrome\Application\chrome.exe','--headless=new',f'--remote-debugging-port={debug_port}',f'--user-data-dir={OUT/("profile-"+str(debug_port))}','--no-first-run','--no-default-browser-check','--use-angle=swiftshader','--enable-unsafe-swiftshader','about:blank'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=flags)
    tabs=until(lambda:get(f'http://127.0.0.1:{debug_port}/json'))
    cdp=CDP(next(tab['webSocketDebuggerUrl'] for tab in tabs if tab['type']=='page'))
    cdp.call('Page.enable');cdp.call('Runtime.enable');cdp.call('Network.enable')
    cdp.call('Network.setBlockedURLs',urls=['https://tile.openstreetmap.org/*'])
    cdp.call('Emulation.setDeviceMetricsOverride',width=1440,height=1000,deviceScaleFactor=1,mobile=False)
    cdp.call('Page.addScriptToEvaluateOnNewDocument',source='window.reviewErrors=[];window.addEventListener("error",e=>{if(e.message)reviewErrors.push(e.message)});window.addEventListener("unhandledrejection",e=>reviewErrors.push(String(e.reason)));')
    cdp.call('Page.navigate',url=f'http://127.0.0.1:{port}')
    until(lambda:cdp.js('document.querySelector("#timeline")&&!document.querySelector("#timeline").disabled'))
    until(lambda:cdp.js('document.querySelector("#relative-target").textContent.includes("Hostile")'))
    assert cdp.js('document.querySelector("#calc-phase").textContent')=='Crank'
    assert cdp.js('parseFloat(getComputedStyle(document.querySelector("#calc-phase")).fontSize)')==28
    assert cdp.js('getComputedStyle(document.querySelector("#calc-phase")).fontWeight')=='700'
    cdp.js('document.querySelector("#entity-list .entity[data-team=red]").click()')
    until(lambda:cdp.js('document.querySelector("#selected-name").textContent==="Hostile"'))
    assert cdp.js('document.querySelector("#entity-list").textContent.includes("Hostile-0")')
    cdp.js('document.querySelector("#seek-time").value="15";document.querySelector("#seek-time").dispatchEvent(new Event("change"));')
    time.sleep(.4)
    assert cdp.js('[...document.querySelectorAll(".flight-label-name")].some(e=>e.textContent==="Hostile")')
    assert cdp.js('[...document.querySelectorAll(".flight-label-name")].some(e=>e.textContent==="Hostile-0")')
    assert cdp.js('''(async()=>{const {buffer}=await import('/app.js');const missile=[...buffer.tracks.values()].find(t=>t.samples[0].kind==='missile').samples[0];return missile.id==='AIM-120 AMRAAM (Bogey--red--0) [Red].csv'&&missile.owner_id==='F-16 (Bogey--red) [Red].csv';})()''')
    cdp.screenshot('hostile-phase.png')
    cdp.js('document.querySelector("#view-2d").click()')
    time.sleep(.3)
    assert cdp.js('document.querySelector("#selected-name").textContent==="Hostile"')
    cdp.js('document.querySelector("#toggle-theme").click()')
    time.sleep(.2)
    assert cdp.js('parseFloat(getComputedStyle(document.querySelector("#calc-phase")).fontSize)')==28
    assert cdp.js('document.querySelector("#calc-phase").scrollWidth<=document.querySelector("#calc-phase").clientWidth')
    assert not cdp.js('window.reviewErrors'),cdp.js('window.reviewErrors')
    print(json.dumps({'passed':True,'phase':'28px / 700','legacy_aircraft':'Hostile','legacy_missile':'Hostile-0','ids_and_ownership':'preserved','runtime_errors':[],'screenshot':str(OUT/'hostile-phase.png')},ensure_ascii=False),flush=True)
finally:
    if chrome:chrome.terminate();chrome.wait(timeout=15)
    server.terminate();server.wait(timeout=15)
