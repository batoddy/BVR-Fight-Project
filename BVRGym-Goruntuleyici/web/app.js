import {SampleBuffer,ReplayClock} from './stream.js';
import {Scene} from './scene.js';
import {Scene as Scene2D} from './scene2d.js';
const el=id=>document.getElementById(id);
const themeButton=el('toggle-theme');
function updateThemeButton(){themeButton.setAttribute('aria-checked',String(document.documentElement.dataset.theme==='dark'));}
updateThemeButton();
themeButton.onclick=()=>{
  const theme=document.documentElement.dataset.theme==='dark'?'light':'dark';
  document.documentElement.dataset.theme=theme;
  try{localStorage.setItem('bvrgym-viewer-theme',theme);}catch{}
  updateThemeButton();
};
const buffer=new SampleBuffer();let selected=null,viewTime=null,lastLabels=0,lastReceive=0,connectionError=null;
const replayClock=new ReplayClock();let playbackSpeed=1,paused=false,scrubbing=false;
const playbackRates=[1,2,3,4,5,10];
const dock=el('playback-dock'),dockResizer=el('dock-resizer');
let dockHeight=134,dockDrag=null,dockNarrow=false;
try{const saved=Number(localStorage.getItem('bvrgym-dock-height'));if(saved>=94&&saved<=400)dockHeight=saved;}catch{}
function resizeDock(height,save=false){
  const minimum=dockNarrow?160:94,maximum=dockNarrow?400:320;
  dockHeight=Math.round(Math.max(minimum,Math.min(maximum,height)));
  dock.style.setProperty('--dock-height',dockHeight+'px');
  dock.classList.toggle('compact',dockHeight<(dockNarrow?220:126));
  dockResizer.setAttribute('aria-valuemin',minimum);dockResizer.setAttribute('aria-valuemax',maximum);
  dockResizer.setAttribute('aria-valuenow',dockHeight);dockResizer.setAttribute('aria-valuetext',dockHeight+' piksel');
  if(save)try{localStorage.setItem('bvrgym-dock-height',String(dockHeight));}catch{}
}
resizeDock(dockHeight);
new ResizeObserver(entries=>{
  const narrow=entries[0].contentRect.width<=560;
  if(narrow!==dockNarrow){dockNarrow=narrow;resizeDock(narrow?Math.max(250,dockHeight):Math.min(134,dockHeight));}
}).observe(dock.parentElement);
dockResizer.onpointerdown=event=>{
  if(event.button!==0)return;event.preventDefault();dockDrag={y:event.clientY,height:dockHeight,pointer:event.pointerId};
  dockResizer.setPointerCapture(event.pointerId);document.body.classList.add('resizing-dock');
};
dockResizer.onpointermove=event=>{if(dockDrag)resizeDock(dockDrag.height+dockDrag.y-event.clientY);};
function finishDockDrag(){if(!dockDrag)return;dockDrag=null;document.body.classList.remove('resizing-dock');resizeDock(dockHeight,true);}
for(const name of ['pointerup','pointercancel','lostpointercapture'])dockResizer.addEventListener(name,finishDockDrag);
dockResizer.ondblclick=()=>resizeDock(dock.classList.contains('compact')?(dockNarrow?250:134):(dockNarrow?160:94),true);
dockResizer.onkeydown=event=>{
  const values={ArrowUp:dockHeight+16,ArrowDown:dockHeight-16,Home:0,End:400};
  if(event.key in values){event.preventDefault();resizeDock(values[event.key],true);}
};
let scene,webglError=null;
const selectEntity=id=>{selected=id;renderList();};
try{scene=new Scene(el('scene'),selectEntity,()=>updateFollow());}
catch(error){
  webglError='Bu tarayıcıda 3D görünüm açılamadı; 2D görünüm kullanılıyor.';
  const oldCanvas=el('scene'),canvas=oldCanvas.cloneNode(true);oldCanvas.replaceWith(canvas);
  scene=new Scene2D(canvas,selectEntity,()=>updateFollow());
  el('view-3d').disabled=true;el('view-2d').setAttribute('aria-pressed','true');el('view-3d').setAttribute('aria-pressed','false');
  for(const id of ['camera-orbit','camera-top','fit-all'])el(id).disabled=true;
  el('view-title').textContent='Üstten görünüm';
}
const availableModes=!webglError;
function mode(value){
  if(!availableModes)return;
  scene.setMode(value);
  el('view-3d').setAttribute('aria-pressed',String(value==='3d'));el('view-2d').setAttribute('aria-pressed',String(value==='2d'));
  el('view-title').textContent=value==='3d'?'3D uçuş görünümü':'Üstten görünüm';
  el('camera-help').textContent=value==='3d'?'Sol tuş: döndür · Sağ tuş: taşı · Tekerlek: yakınlaştır':'Sürükle: taşı · Tekerlek: yakınlaştır';
}
el('view-3d').onclick=()=>mode('3d');el('view-2d').onclick=()=>mode('2d');
el('camera-orbit').onclick=()=>{mode('3d');scene.setView('orbit');};
el('camera-top').onclick=()=>{mode('3d');scene.setView('top');};
el('fit-all').onclick=()=>{if(!availableModes)return;scene.follow=false;scene.frameAll(viewTime===null?[]:buffer.entities(viewTime));updateFollow();};
const number=(value,digits=0)=>Number(value).toLocaleString('tr-TR',{minimumFractionDigits:digits,maximumFractionDigits:digits});
const roleName=e=>e.kind==='missile'?'Füze':e.team==='red'?'Bogey':e.team==='blue'?'Dost uçak':'Uçak';
function updateFollow(){el('follow').setAttribute('aria-pressed',String(scene.follow));}
function panel(name,show){
  el(name+'-content').hidden=!show;el(name).classList.toggle('collapsed',!show);
  el('layout').classList.toggle(name==='objects'?'no-list':'no-detail',!show);
  const button=el(name==='objects'?'toggle-list':'toggle-detail');
  button.setAttribute('aria-expanded',String(show));
  const label=(name==='objects'?'Nesne panelini':'Uçak bilgi panelini')+(show?' kapat':' aç');
  button.setAttribute('aria-label',label);button.title=label;
  button.textContent=name==='objects'?(show?'‹':'›'):(show?'›':'‹');
}
el('toggle-list').onclick=()=>panel('objects',el('objects-content').hidden);
el('toggle-detail').onclick=()=>panel('detail',el('detail-content').hidden);
const narrowLayout=matchMedia('(max-width:980px)');
const collapseOnNarrow=()=>{if(narrowLayout.matches){panel('objects',false);panel('detail',false);}};
collapseOnNarrow();narrowLayout.addEventListener('change',collapseOnNarrow);
el('follow').onclick=()=>{scene.follow=!scene.follow;updateFollow();};
el('zoom-in').onclick=()=>scene.zoom(1.25);el('zoom-out').onclick=()=>scene.zoom(.8);
el('trail').onchange=event=>{scene.trail=event.target.checked;};el('grid').onchange=event=>{scene.grid=event.target.checked;};
function setPlayback(value){
  playbackSpeed=Number(value);replayClock.speed=playbackSpeed;
  replayClock.seek(viewTime,performance.now()/1000);
  el('playback-speed').value=playbackRates.indexOf(playbackSpeed);
  el('playback-rate').textContent=playbackSpeed+'×';
  el('playback-speed').setAttribute('aria-valuetext',playbackSpeed+'×');
  for(const button of document.querySelectorAll('[data-rate]'))button.setAttribute('aria-pressed',String(Number(button.dataset.rate)===playbackSpeed));
}
el('playback-speed').oninput=event=>setPlayback(playbackRates[Number(event.target.value)]);
for(const button of document.querySelectorAll('[data-rate]')){
  button.style.setProperty('--stop',playbackRates.indexOf(Number(button.dataset.rate)));
  button.onclick=()=>setPlayback(button.dataset.rate);
}
el('record-end').onclick=()=>{
  const bounds=buffer.bounds();if(!bounds)return;
  paused=true;replayClock.seek(bounds.last,performance.now()/1000);viewTime=bounds.last;
};
el('restart-playback').onclick=()=>{
  const bounds=buffer.bounds();if(!bounds)return;
  paused=false;replayClock.seek(bounds.first,performance.now()/1000);viewTime=bounds.first;
};
el('play-pause').onclick=()=>{
  const bounds=buffer.bounds();if(!bounds)return;
  if(paused&&viewTime>=bounds.last){replayClock.seek(bounds.first,performance.now()/1000);viewTime=bounds.first;}
  paused=!paused;
};
el('timeline').onpointerdown=()=>{scrubbing=true;};
el('timeline').oninput=event=>{
  paused=true;viewTime=Number(event.target.value);replayClock.seek(viewTime,performance.now()/1000);
};
for(const name of ['change','pointerup','pointercancel','blur'])el('timeline').addEventListener(name,()=>{scrubbing=false;});
function formatTime(value){
  const m=Math.floor(value/60),s=value%60;
  return String(m).padStart(2,'0')+':'+s.toFixed(1).padStart(4,'0');
}
function updateTimeline(){
  const bounds=buffer.complete?buffer.bounds():null,slider=el('timeline');slider.disabled=!bounds;
  for(const id of ['play-pause','restart-playback','record-end'])el(id).disabled=!bounds;
  if(bounds){
    slider.min=Math.floor(bounds.first*100)/100;slider.max=Math.ceil(bounds.last*100)/100;
    if(!scrubbing)slider.value=viewTime??bounds.last;
    slider.setAttribute('aria-valuetext',formatTime(viewTime??bounds.first));
    el('timeline-first').textContent=formatTime(bounds.first);el('timeline-last').textContent=formatTime(bounds.last);
  }
  el('play-pause').textContent=paused?'▶':'Ⅱ';
  el('play-pause').setAttribute('aria-label',paused?'Oynat':'Duraklat');
  el('play-pause').setAttribute('aria-pressed',String(paused));
}
let listSignature='';
function renderList(){
  const entries=buffer.complete&&viewTime!==null?buffer.entities(viewTime):[];
  if(!entries.some(e=>e.id===selected))selected=entries[0]?.id??null;
  const signature=buffer.session+entries.map(e=>e.id+e.name+(e.id===selected)).join('|');
  if(signature===listSignature)return;listSignature=signature;
  el('entity-list').replaceChildren();
  for(const entry of entries){
    const button=document.createElement('button');button.className='entity'+(entry.id===selected?' selected':'');
    button.dataset.kind=entry.kind;button.dataset.team=entry.team;
    button.setAttribute('aria-pressed',String(entry.id===selected));button.textContent=entry.name;
    const model=document.createElement('span');model.textContent=roleName(entry)+' · '+entry.model;
    button.appendChild(model);button.onclick=()=>{selected=entry.id;renderList();};el('entity-list').appendChild(button);
  }
}
async function poll(){
  let again=500;
  try{
    const response=await fetch('/api/state?after='+buffer.cursor+'&session='+encodeURIComponent(buffer.session||''),{cache:'no-store',signal:AbortSignal.timeout(4000)});
    if(!response.ok)throw new Error('Veri bağlantısı: HTTP '+response.status);
    const packet=await response.json();if(packet.schema_version!==2||packet.source!=='tacview-csv')throw new Error('Desteklenmeyen kayıt kaynağı');
    if(buffer.consume(packet)){viewTime=null;selected=null;replayClock.reset();paused=false;}
    if(buffer.complete&&viewTime===null&&buffer.bounds()){
      viewTime=buffer.bounds().first;replayClock.seek(viewTime,performance.now()/1000);
    }
    renderList();updateTimeline();lastReceive=performance.now();connectionError=null;
    if(!buffer.complete)again=0;
  }catch(error){connectionError=error.message;again=1000;}
  setTimeout(poll,again);
}
function labels(){
  renderList();updateTimeline();
  const status=el('status'),message=el('message');
  let text='Kayıt bekleniyor',note='BVRGym Tacview CSV kaydı bekleniyor. Dosyalar kaydedilince otomatik açılır. '+(buffer.packet.recording||''),error=false;
  const bounds=buffer.bounds();
  if(connectionError){text='Bağlantı kesildi';note='Kayıt oynatımı duraklatıldı. '+connectionError;error=true;}
  else if(buffer.packet.phase==='error'){text='Kayıt okunamadı';note=buffer.packet.error;error=true;}
  else if(!buffer.complete){text='Kayıt yükleniyor';note='Uçak ve füze kayıtları ortak zaman çizelgesine yükleniyor.';}
  else if(buffer.packet.phase==='ready'&&bounds){
    text=paused?(viewTime>=bounds.last?'Kayıt sonu':'Duraklatıldı'):`Kayıt · ${playbackSpeed}×`;
    note=`Tacview CSV · ${buffer.packet.objects} nesne · ${paused?'Duraklatıldı':'Oynatma '+playbackSpeed+'×'} · ${buffer.packet.recording}`;
  }
  if(webglError)note+=' '+webglError;
  status.textContent=text;status.classList.toggle('error',error);message.textContent=note;message.classList.toggle('error',error);
  el('clock').textContent=viewTime===null?'—':formatTime(viewTime);
  const entity=buffer.complete?buffer.at(selected,viewTime):null;
  if(!entity){
    el('selected-name').textContent='Uçak bilgisi';el('model').textContent='Bu zamanda seçili nesne yok';
    for(const id of ['altitude','speed','heading','mach','roll','pitch','east','north'])el(id).textContent='—';
    return;
  }
  el('selected-name').textContent=entity.name;el('model').textContent=roleName(entity)+' · '+entity.model+' · Tacview CSV';
  for(const [id,key,digits] of [['altitude','altitude_m',0],['speed','true_airspeed_mps',1],['heading','heading_deg',1],['mach','mach',2],['roll','roll_deg',1],['pitch','pitch_deg',1]])el(id).textContent=entity[key]==null?'—':number(entity[key],digits);
  el('east').textContent=number(entity.east_m/1000,2)+' km';el('north').textContent=number(entity.north_m/1000,2)+' km';
}
function render(timestamp){
  const connected=!connectionError&&timestamp-lastReceive<5000;
  const canPlay=connected&&buffer.complete&&buffer.packet.phase==='ready'&&!paused&&!scrubbing;
  if(buffer.complete)viewTime=replayClock.tick(buffer,timestamp/1000,canPlay);
  const bounds=buffer.bounds();if(canPlay&&bounds&&viewTime>=bounds.last)paused=true;
  const entities=buffer.complete&&viewTime!==null?buffer.entities(viewTime):[];
  renderList();scene.draw(entities,buffer,viewTime??0,selected);
  if(timestamp-lastLabels>150){labels();lastLabels=timestamp;}
  requestAnimationFrame(render);
}
poll();requestAnimationFrame(render);
export {scene,buffer};
