from pathlib import Path

# Reuse the local DevTools harness and fixture; never fetch public tiles in QA.
exec(Path(__file__).with_name('ui-review.py').read_text(encoding='utf-8').split('port, debug_port = free_port(), free_port()')[0])
port,debug_port=free_port(),free_port()
flags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0
server=subprocess.Popen([sys.executable,'-B',str(ROOT/'BVRGym-Goruntuleyici/viewer.py'),'--recording',str(fixture),'--port',str(port)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=flags)
chrome=None
try:
    until(lambda:get(f'http://127.0.0.1:{port}/api/state').get('phase')=='ready')
    chrome=subprocess.Popen([r'C:\Program Files\Google\Chrome\Application\chrome.exe','--headless=new',f'--remote-debugging-port={debug_port}',f'--user-data-dir={OUT/("map-profile-"+str(debug_port))}','--no-first-run','--no-default-browser-check','--use-angle=swiftshader','--enable-unsafe-swiftshader','about:blank'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=flags)
    tabs=until(lambda:get(f'http://127.0.0.1:{debug_port}/json'))
    cdp=CDP(next(tab['webSocketDebuggerUrl'] for tab in tabs if tab['type']=='page'))
    cdp.call('Page.enable');cdp.call('Runtime.enable');cdp.call('Network.enable')
    cdp.call('Network.setBlockedURLs',urls=['https://tile.openstreetmap.org/*'])
    cdp.call('Emulation.setDeviceMetricsOverride',width=1440,height=900,deviceScaleFactor=1,mobile=False)
    cdp.call('Page.addScriptToEvaluateOnNewDocument',source='window.reviewErrors=[];window.addEventListener("error",e=>{if(e.message)reviewErrors.push(e.message)});window.addEventListener("unhandledrejection",e=>reviewErrors.push(String(e.reason)));const originalError=console.error;console.error=(...args)=>{reviewErrors.push(args.map(String).join(" "));originalError(...args)};')
    cdp.call('Page.navigate',url=f'http://127.0.0.1:{port}')
    until(lambda:cdp.js('document.querySelector("#timeline")&&!document.querySelector("#timeline").disabled'))
    assert cdp.js('(async()=>{const {scene}=await import("/app.js");return !scene.basemap.enabled&&scene.basemap.cache.size===0})()')
    cdp.js('''(()=>{
      const canvas=document.createElement('canvas');canvas.width=256;canvas.height=256;const c=canvas.getContext('2d');
      c.fillStyle='#dbe4bc';c.fillRect(0,0,256,256);c.fillStyle='#8dbdd4';c.fillRect(0,180,256,76);
      c.fillStyle='#99ba87';c.fillRect(20,20,100,110);c.strokeStyle='#d59d6d';c.lineWidth=7;c.beginPath();c.moveTo(0,170);c.lineTo(256,60);c.stroke();
      c.fillStyle='#354355';c.font='15px sans-serif';c.fillText('TEST MAP',135,145);window.mapFixture=canvas.toDataURL();window.mapRequests=[];
      const NativeImage=window.Image;window.Image=class extends NativeImage{set src(value){if(value.startsWith('https://tile.openstreetmap.org/')){mapRequests.push(value);super.src=window.mapFixture;}else super.src=value;}get src(){return super.src;}};
      document.querySelector('#basemap').checked=true;document.querySelector('#basemap').dispatchEvent(new Event('change'));
    })()''')
    until(lambda:cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.mapMeshes.size>0&&scene.basemap.status===""})()'))
    time.sleep(.5)
    assert cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.renderer.getContext().getError()===0})()')
    assert cdp.js('!document.querySelector("#map-attribution").hidden')
    mesh_count=cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.mapMeshes.size})()')
    cdp.screenshot('map-3d-mock.png')
    cdp.js('document.querySelector("#grid").checked=false;document.querySelector("#grid").dispatchEvent(new Event("change"));')
    time.sleep(.3)
    assert cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.gridUniforms.gridEnabled.value===0&&scene.mapGroup.visible})()')
    cdp.js('document.querySelector("#view-2d").click()')
    until(lambda:cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.basemap.tiles.length>0&&scene.basemap.status===""&&scene.basemap.tiles.every(t=>!!t.raster)})()'))
    cdp.screenshot('map-2d-mock.png')
    assert cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.flat.ctx.getImageData(80,100,1,1).data[3]===255})()')
    cdp.js('document.querySelector("#toggle-theme").click()')
    time.sleep(.2);cdp.screenshot('map-light-mock.png')
    cdp.js('document.querySelector("#basemap").checked=false;document.querySelector("#basemap").dispatchEvent(new Event("change"));')
    time.sleep(.4)
    assert cdp.js('document.querySelector("#map-attribution").hidden')
    count=cdp.js('mapRequests.length');time.sleep(.4);assert cdp.js('mapRequests.length')==count
    # All failures are synthetic data URLs; online tile services remain blocked.
    cdp.js('''(async()=>{const {scene}=await import('/app.js');window.goodFixture=mapFixture;window.mapFixture='data:image/png;base64,invalid';scene.basemap.cache.clear();scene.basemap.active=[];scene.basemap.lastPlan=-Infinity;document.querySelector('#basemap').checked=true;document.querySelector('#basemap').dispatchEvent(new Event('change'));})()''')
    until(lambda:cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.basemap.status.includes("yüklenemedi")&&scene.basemap.loading===0})()'))
    assert cdp.js('document.querySelector("#scene-message").hidden'), 'Map failures must not replace playback status'
    cdp.js('document.querySelector("#basemap").checked=false;document.querySelector("#basemap").dispatchEvent(new Event("change"));window.mapFixture=goodFixture;')
    time.sleep(.3)
    cdp.js('document.querySelector("#basemap").checked=true;document.querySelector("#basemap").dispatchEvent(new Event("change"));')
    until(lambda:cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.basemap.tiles.length>0&&scene.basemap.status===""})()'))
    cdp.js('document.querySelector("#view-3d").click()')
    time.sleep(.4)
    assert cdp.js('(async()=>{const {scene}=await import("/app.js");return scene.mapGroup.visible&&scene.mapMeshes.size>0&&scene.renderer.getContext().getError()===0})()')
    cdp.js('document.querySelector("#basemap").checked=false;document.querySelector("#basemap").dispatchEvent(new Event("change"));')
    time.sleep(.3)
    assert cdp.js('(async()=>{const {scene}=await import("/app.js");return !scene.mapGroup.visible})()')
    assert not cdp.js('window.reviewErrors'),cdp.js('window.reviewErrors')
    print(json.dumps({'passed':True,'checks':['disabled makes no requests','3D textures and grid shader','2D geographic raster','attribution','independent grid toggle','map toggle','offline fallback','explicit retry','shared 2D/3D cache','no WebGL or runtime errors'],'3d_tiles':mesh_count,'network':'public tile requests blocked; synthetic images only','screenshots':str(OUT)},ensure_ascii=False),flush=True)
finally:
    if chrome:chrome.terminate();chrome.wait(timeout=15)
    server.terminate();server.wait(timeout=15)
