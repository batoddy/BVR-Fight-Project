from pathlib import Path
import csv

exec(Path(__file__).with_name('ui-review.py').read_text(encoding='utf-8').split('fixture = OUT /')[0])
OUT = ROOT / 'tmp' / 'new-variables-review'
OUT.mkdir(exist_ok=True)
fixture = OUT / 'flight'
fixture.mkdir(exist_ok=True)
header = 'time_s,lon_deg,lat_deg,alt_m,roll_deg,pitch_deg,yaw_deg,tas_kt\n'
for key, team, lon in [('blue', 'Blue', 18), ('red', 'Red', 18.1)]:
    (fixture / f'F-16 ({key}--{key}) [{team}].csv').write_text(
        header + ''.join(f'{t},{lon},59,7000,0,0,90,500\n' for t in [0, 5, 10, 15, 20]), encoding='utf-8')
with (fixture / 'calculations.csv').open('w', newline='', encoding='utf-8') as handle:
    writer = csv.writer(handle)
    writer.writerow(['time_s', 'id', 'target_id', 'range_3d_nm', 'a_pole_nm', 'f_pole_nm', 'tr_nm', 'dr_nm'])
    writer.writerows([[0, 'blue', 'red', 3, 12.5, 8, 10, 18], [0, 'red', 'blue', 3, 7, 6, 5, 14],
                     [5, 'blue', 'red', 3, 0, -2, -1, 18], [5, 'red', 'blue', 3, 0, 0, 0, 14],
                     [10, 'blue', 'red', 3, 0, -2, '', ''], [10, 'red', 'blue', 3, 0, 0, 0, 0],
                     [15, 'blue', 'red', 3, 0, 0, 0, 18], [15, 'red', 'blue', 3, 0, 0, 0, 14]])
port, debug_port = free_port(), free_port()
flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
server = subprocess.Popen([sys.executable, '-B', str(ROOT / 'BVRGym-Goruntuleyici/viewer.py'), '--recording', str(fixture), '--port', str(port)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=flags)
chrome = None
try:
    until(lambda: get(f'http://127.0.0.1:{port}/api/state').get('phase') == 'ready')
    chrome = subprocess.Popen([r'C:\Program Files\Google\Chrome\Application\chrome.exe', '--headless=new', f'--remote-debugging-port={debug_port}', f'--user-data-dir={OUT / ("profile-" + str(debug_port))}', '--no-first-run', '--no-default-browser-check', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', 'about:blank'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=flags)
    tabs = until(lambda: get(f'http://127.0.0.1:{debug_port}/json'))
    cdp = CDP(next(tab['webSocketDebuggerUrl'] for tab in tabs if tab['type'] == 'page'))
    cdp.call('Page.enable'); cdp.call('Runtime.enable')
    cdp.call('Emulation.setDeviceMetricsOverride', width=1440, height=1000, deviceScaleFactor=1, mobile=False)
    cdp.call('Page.addScriptToEvaluateOnNewDocument', source='window.reviewErrors=[];window.addEventListener("error",e=>reviewErrors.push(e.message));window.addEventListener("unhandledrejection",e=>reviewErrors.push(String(e.reason)));')
    cdp.call('Page.navigate', url=f'http://127.0.0.1:{port}')
    until(lambda: cdp.js('document.querySelector("#calc-dr")?.textContent==="18,00 NM"'))
    def values():
        return cdp.js('["a-pole","f-pole","tr","dr"].map(id=>document.querySelector("#calc-"+id).textContent)')
    assert values() == ['12,50 NM', '8,00 NM', '10,00 NM', '18,00 NM'], values()
    until(lambda: cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.zoneRings.tr.group.visible})()'))
    assert cdp.js('''(async()=>{
      const {scene}=await import('/app.js'),ring=scene.zoneRings.tr,target=scene.analysis.target;
      const p=ring.line.geometry.attributes.position;
      return Math.abs(Math.hypot(p.getX(0),p.getZ(0))-18520)<.01 &&
        Math.abs(ring.group.position.x-target.east_m)<.01 &&
        Math.abs(ring.group.position.y-target.altitude_m)<.01 &&
        Math.abs(ring.group.position.z+target.north_m)<.01;
    })()''')
    cdp.js('document.querySelector("#overlay-tr").click()')
    until(lambda: cdp.js('(async()=>{const {scene}=await import("/app.js");return !scene.zoneRings.tr.group.visible})()'))
    until(lambda: cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.zoneRings.dr.group.visible})()'))
    cdp.js('document.querySelector("#overlay-dr").click()')
    until(lambda: cdp.js('(async()=>{const {scene}=await import("/app.js");return !scene.zoneRings.dr.group.visible})()'))
    cdp.call('Page.reload')
    until(lambda: cdp.js('document.querySelector("#calc-dr")?.textContent==="18,00 NM"'))
    assert cdp.js('document.querySelector("#overlay-dr").checked') is False
    assert cdp.js('document.querySelector("#overlay-tr").checked') is False
    cdp.js('document.querySelector("#overlay-tr").click()')
    cdp.js('document.querySelector("#overlay-dr").click();document.querySelector("#distance-unit").value="km";document.querySelector("#distance-unit").dispatchEvent(new Event("change"));')
    until(lambda: values()[0] == '23,15 km')
    assert values() == ['23,15 km', '14,82 km', '18,52 km', '33,34 km'], values()
    cdp.js('document.querySelector("#relative").scrollIntoView()')
    cdp.screenshot('new-variables-3d.png')
    cdp.js('document.querySelector("#view-2d").click()')
    cdp.js('(async()=>{const {scene}=await import("/app.js");scene.follow=false;scene.flat.scale=.008})()')
    assert cdp.js('''(async()=>{
      const {scene,buffer}=await import('/app.js'),flat=scene.flat,ctx=flat.ctx,texts=[];
      const original=ctx.fillText;ctx.fillText=function(text,...rest){texts.push(text);return original.call(this,text,...rest);};
      try{scene.draw(buffer.entities(0),buffer,0,buffer.entities(0)[0].id);}finally{ctx.fillText=original;}
      return texts.includes('DR')&&texts.includes('TR');
    })()''')
    cdp.screenshot('new-variables-2d.png')
    cdp.js('document.querySelector("#overlay-tr").click()')
    until(lambda: cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.flat.analysis?.options.zones.tr===false})()'))
    assert cdp.js('''(async()=>{
      const {scene,buffer}=await import('/app.js'),ctx=scene.flat.ctx,texts=[],original=ctx.fillText;
      ctx.fillText=function(text,...rest){texts.push(text);return original.call(this,text,...rest);};
      try{scene.draw(buffer.entities(0),buffer,0,null);}finally{ctx.fillText=original;}
      return !texts.includes('TR');
    })()''')
    cdp.js('document.querySelector("#overlay-tr").click()')
    cdp.js('document.querySelector("#entity-list .entity[data-team=red]").click()')
    until(lambda: values()[0] == '12,96 km')
    for time_s in [5, 10, 15, 20]:
        cdp.js(f'document.querySelector("#entity-list .entity[data-team=blue]").click();document.querySelector("#view-3d").click();document.querySelector("#seek-time").value="{time_s}";document.querySelector("#seek-time").dispatchEvent(new Event("change"));')
        expected = {5: '-1,85 km', 10: '—', 15: 'Çözüm yok', 20: '—'}[time_s]
        until(lambda: values()[2] == expected)
        until(lambda: cdp.js(f'(async()=>{{const {{scene}}=await import("/app.js");return {"scene.analysis===null" if time_s == 20 else f"scene.analysis?.entity.sim_time_s==={time_s}"}}})()'))
        until(lambda: cdp.js('(async()=>{const {scene}=await import("/app.js");return !scene.zoneRings.tr.group.visible})()'))
        cdp.js('document.querySelector("#view-2d").click()')
        until(lambda: cdp.js(f'(async()=>{{const {{scene}}=await import("/app.js");return {"scene.flat.analysis===null" if time_s == 20 else f"scene.flat.analysis?.entity.sim_time_s==={time_s}"}}})()'))
        assert cdp.js('''(async()=>{
          const {scene,buffer}=await import('/app.js'),ctx=scene.flat.ctx,texts=[],original=ctx.fillText;
          ctx.fillText=function(text,...rest){texts.push(text);return original.call(this,text,...rest);};
          try{scene.draw(buffer.entities(Number(document.querySelector('#seek-time').value)),buffer,0,null);}finally{ctx.fillText=original;}
          return !texts.includes('TR');
        })()''')
    cdp.js('document.querySelector("#entity-list .entity[data-team=blue]").click();document.querySelector("#seek-time").value="10";document.querySelector("#seek-time").dispatchEvent(new Event("change"));')
    until(lambda: values() == ['Çözüm yok', '-3,70 km', '—', '—'])
    assert values() == ['Çözüm yok', '-3,70 km', '—', '—'], values()
    cdp.js('document.querySelector("#seek-time").value="20";document.querySelector("#seek-time").dispatchEvent(new Event("change"));')
    until(lambda: values() == ['—'] * 4)
    cdp.js('document.querySelector("#seek-time").value="0";document.querySelector("#seek-time").dispatchEvent(new Event("change"));')
    until(lambda: values()[0] == '23,15 km')
    assert not cdp.js('window.reviewErrors'), cdp.js('window.reviewErrors')
    print(json.dumps({'passed': True, 'checks': ['NM/km display', 'both aircraft perspectives', '2D/3D DR/TR rings', 'TR radius and target center', 'saved TR toggle', 'TR hidden for zero/negative/missing values', 'missing final calculation', 'backward seek', 'no browser errors']}, ensure_ascii=False))
finally:
    if chrome:
        chrome.terminate(); chrome.wait(timeout=15)
    server.terminate(); server.wait(timeout=15)
