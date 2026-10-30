import {SampleBuffer,ReplayClock} from './stream.js';
import {Scene} from './scene.js';
import {Scene as Scene2D} from './scene2d.js';
import {setupRecordingMenu} from './recording-menu.js';
import {ALTITUDE_UNITS,SPEED_UNITS,DISTANCE_UNITS,altitudeValue,altitudeReferenceInfo,speedValue,distanceLabel,munitionState,angleLabel,sideAngleLabel} from './units.js';
import {inventoryPanel} from './inventory.js';
import {Ruler} from './ruler.js';

const el=id=>document.getElementById(id);
const defaults={left:210,right:280,dock:76,list:true,detail:true,mode:'3d',camera:'orbit',follow:true,
  altitudeGuides:true,velocityGuides:true,objectScale:1,trail:true,trailDuration:300,history:true,grid:true,basemap:false,missileOutcomes:false,labelMode:'all',labelNames:true,labelAltitude:true,labelSpeed:true,speedUnit:'mps',altitudeUnit:'ft',distanceUnit:'nm',
  overlayLine:true,overlayAta:true,overlayBearing:true,overlayRange:true,overlayAspect:false,overlayRadar:true,overlayOwnWez:true,overlayEnemyWez:true,
  overlaySternWez:true,overlayNez:true,overlayMar:true,overlayDor:true};
const preferences={...defaults};
try{
  const saved=JSON.parse(localStorage.getItem('bvrgym-layout-v2')||'{}');
  if(typeof saved.velocityGuides!=='boolean'&&typeof saved.headingGuides==='boolean')saved.velocityGuides=saved.headingGuides;
  for(const key of Object.keys(defaults))if(typeof saved[key]===typeof defaults[key])preferences[key]=saved[key];
}catch{}
const save=()=>{try{localStorage.setItem('bvrgym-layout-v2',JSON.stringify(preferences));}catch{}};
const buffer=new SampleBuffer(),replayClock=new ReplayClock();
const playbackRates=[.25,.5,1,2,3,4,5,10],hiddenObjects=new Set();
const teamOrder={blue:0,red:1};
let selected=null,viewTime=null,paused=true,scrubbing=false,lastReceive=0,connectionError=null,
  lastLabels=0,listSignature='',filter='all',query='',scene,webglError=null;

function theme(){el('toggle-theme').setAttribute('aria-checked',String(document.documentElement.dataset.theme==='dark'));}
theme();el('toggle-theme').onclick=()=>{
  document.documentElement.dataset.theme=document.documentElement.dataset.theme==='dark'?'light':'dark';
  try{localStorage.setItem('bvrgym-viewer-theme',document.documentElement.dataset.theme);}catch{}theme();
};

function panel(name,show,persist=true){
  el(name+'-content').hidden=!show;el(name).classList.toggle('collapsed',!show);
  el('layout').classList.toggle(name==='objects'?'no-list':'no-detail',!show);
  const button=el(name==='objects'?'toggle-list':'toggle-detail');
  button.setAttribute('aria-expanded',String(show));button.textContent=name==='objects'?(show?'‹':'›'):(show?'›':'‹');
  button.setAttribute('aria-label',(name==='objects'?'Nesne panelini':'Bilgi panelini')+(show?' kapat':' aç'));
  if(persist){preferences[name==='objects'?'list':'detail']=show;save();}
}
const narrow=matchMedia('(max-width:900px)');
panel('objects',narrow.matches?false:preferences.list,false);panel('detail',narrow.matches?false:preferences.detail,false);
el('toggle-list').onclick=()=>panel('objects',el('objects-content').hidden);
el('toggle-detail').onclick=()=>panel('detail',el('detail-content').hidden);
narrow.addEventListener('change',event=>{panel('objects',event.matches?false:preferences.list,false);panel('detail',event.matches?false:preferences.detail,false);});

function panelWidth(side,width,persist=false){
  const minimum=side==='left'?170:240,maximum=side==='left'?350:420;
  const other=side==='left'?(preferences.detail?preferences.right:32):(preferences.list?preferences.left:32);
  const available=el('layout').clientWidth-other-350;
  preferences[side]=Math.round(Math.max(minimum,Math.min(maximum,width,Math.max(minimum,available))));
  el('layout').style.setProperty(side==='left'?'--left-width':'--right-width',preferences[side]+'px');
  const grip=el(side==='left'?'objects-resizer':'detail-resizer');
  grip.setAttribute('aria-valuenow',preferences[side]);grip.setAttribute('aria-valuemin',minimum);grip.setAttribute('aria-valuemax',maximum);
  if(persist)save();
}
for(const side of ['left','right']){
  panelWidth(side,preferences[side]);const grip=el(side==='left'?'objects-resizer':'detail-resizer');let drag=null;
  grip.onpointerdown=event=>{if(event.button!==0)return;drag={x:event.clientX,width:preferences[side]};grip.setPointerCapture(event.pointerId);document.body.classList.add('resizing');};
  grip.onpointermove=event=>{if(drag)panelWidth(side,drag.width+(event.clientX-drag.x)*(side==='left'?1:-1));};
  const finish=()=>{if(drag){drag=null;document.body.classList.remove('resizing');save();}};
  for(const name of ['pointerup','pointercancel','lostpointercapture'])grip.addEventListener(name,finish);
  grip.onkeydown=event=>{if(['ArrowLeft','ArrowRight'].includes(event.key)){event.preventDefault();panelWidth(side,preferences[side]+(event.key==='ArrowRight'?16:-16)*(side==='left'?1:-1),true);}};
}

let dockNarrow=false,dockDrag=null;
function dockHeight(value,persist=false){
  const minimum=dockNarrow?128:68,maximum=dockNarrow?220:170;
  const height=Math.round(Math.max(minimum,Math.min(maximum,value)));
  el('playback-dock').style.setProperty('--dock-height',height+'px');
  const grip=el('dock-resizer');grip.setAttribute('aria-valuenow',height);grip.setAttribute('aria-valuemin',minimum);grip.setAttribute('aria-valuemax',maximum);
  if(persist){preferences.dock=height;save();}
}
dockHeight(preferences.dock);
new ResizeObserver(entries=>{dockNarrow=entries[0].contentRect.width<=620;dockHeight(preferences.dock);}).observe(el('playback-dock').parentElement);
el('dock-resizer').onpointerdown=event=>{if(event.button!==0)return;dockDrag={y:event.clientY,height:Number(el('dock-resizer').getAttribute('aria-valuenow'))};el('dock-resizer').setPointerCapture(event.pointerId);document.body.classList.add('resizing');};
el('dock-resizer').onpointermove=event=>{if(dockDrag)dockHeight(dockDrag.height+dockDrag.y-event.clientY);};
const finishDock=()=>{if(dockDrag){dockDrag=null;document.body.classList.remove('resizing');dockHeight(Number(el('dock-resizer').getAttribute('aria-valuenow')),true);}};
for(const name of ['pointerup','pointercancel','lostpointercapture'])el('dock-resizer').addEventListener(name,finishDock);
el('dock-resizer').ondblclick=()=>dockHeight(Number(el('dock-resizer').getAttribute('aria-valuenow'))>(dockNarrow?128:76)?0:170,true);
el('dock-resizer').onkeydown=event=>{if(['ArrowUp','ArrowDown','Home','End'].includes(event.key)){event.preventDefault();const current=Number(el('dock-resizer').getAttribute('aria-valuenow'));dockHeight(event.key==='Home'?0:event.key==='End'?220:current+(event.key==='ArrowUp'?12:-12),true);}};

function updateFollow(){
  el('follow').checked=scene.follow;
  const name=buffer.tracks.get(selected)?.samples[0].name;
  el('follow-caption').textContent=scene.follow&&name?'Takip · '+name:'Seçili nesneyi takip et';
}
function selectEntity(id){selected=id;renderList();updateFollow();}
function focusEntity(id){
  const track=buffer.tracks.get(id);if(!track)return;
  let entity=buffer.at(id,viewTime);
  if(!entity){seek(track.samples[0].record_start_s);entity=buffer.at(id,viewTime);}
  if(!entity)return;
  hiddenObjects.delete(id);selectEntity(id);scene.focusEntity(entity);scene.follow=true;
  preferences.follow=true;save();updateFollow();
}
el('entity-list').ondblclick=event=>{const button=event.target.closest('.entity');if(button)focusEntity(button.closest('.aircraft-card').dataset.id);};
try{scene=new Scene(el('scene'),selectEntity,()=>{preferences.follow=scene.follow;save();updateFollow();});}
catch{
  webglError='3D açılamadı; 2D görünüm kullanılıyor.';
  const canvas=el('scene').cloneNode(true);el('scene').replaceWith(canvas);
  scene=new Scene2D(canvas,selectEntity,()=>{preferences.follow=scene.follow;save();updateFollow();});
  el('view-3d').disabled=true;preferences.mode='2d';
}
scene.onFocus=focusEntity;if(scene.flat)scene.flat.onFocus=focusEntity;scene.follow=preferences.follow;
function setMode(value){
  if(!['2d','3d'].includes(value))value='3d';
  if(webglError)value='2d';else scene.setMode(value);
  preferences.mode=value;save();
  el('view-3d').setAttribute('aria-pressed',String(value==='3d'));el('view-2d').setAttribute('aria-pressed',String(value==='2d'));
  el('view-title').textContent=value==='3d'?'3D uçuş görünümü':'Üstten görünüm';
  el('camera-help').textContent=value==='3d'?'Sol tuş: döndür · Sağ tuş: taşı':'Sürükle: taşı · Tekerlek: yakınlaştır';
}
setMode(preferences.mode);el('view-3d').onclick=()=>setMode('3d');el('view-2d').onclick=()=>setMode('2d');
const ruler=new Ruler(el('scene').parentElement,()=>scene,active=>el('ruler').setAttribute('aria-pressed',String(active)));
el('ruler').onclick=()=>ruler.setActive(!ruler.active);
function camera(value){
  preferences.camera=value;scene.follow=false;preferences.follow=false;
  if(value==='all'){
    scene.frameAll(visibleEntities());
  }else if(!webglError){setMode('3d');scene.setView(value);}
  updateFollow();save();
}
el('camera-mode').value=['orbit','top','all'].includes(preferences.camera)?preferences.camera:'orbit';
el('camera-mode').onchange=event=>camera(event.target.value);
if(webglError){el('camera-mode').querySelector('[value=orbit]').disabled=true;el('camera-mode').querySelector('[value=top]').disabled=true;}
el('follow').onclick=()=>{scene.follow=!scene.follow;preferences.follow=scene.follow;save();updateFollow();};
el('zoom-in').onclick=()=>scene.zoom(1.25);el('zoom-out').onclick=()=>scene.zoom(.8);

function viewSettings(){
  scene.mapEnabled=el('basemap').checked;
  scene.altitudeGuides=el('altitude-guides').checked;scene.velocityGuides=el('velocity-guides').checked;scene.objectScale=Number(el('object-scale').value);scene.trail=el('trail').checked;scene.grid=el('grid').checked;scene.trailDuration=Number(el('trail-duration').value);
  scene.labelMode=el('label-mode').value;scene.labelNames=el('label-names').checked;scene.labelAltitude=el('label-altitude').checked;scene.labelSpeed=el('label-speed').checked;
  Object.assign(preferences,{altitudeGuides:scene.altitudeGuides,velocityGuides:scene.velocityGuides,objectScale:scene.objectScale,trail:scene.trail,grid:scene.grid,trailDuration:scene.trailDuration,history:el('show-history').checked,
    labelMode:scene.labelMode,labelNames:scene.labelNames,labelAltitude:scene.labelAltitude,labelSpeed:scene.labelSpeed,
    missileOutcomes:el('missile-outcomes').checked,basemap:scene.mapEnabled});save();
  // Mühimmat durumu ve olay işaretleri bu tercihe bağlı: bir sonraki karede yenilensinler.
  listSignature='';
}
for(const [id,key] of [['basemap','basemap'],['altitude-guides','altitudeGuides'],['velocity-guides','velocityGuides'],['trail','trail'],['grid','grid'],['show-history','history'],['label-names','labelNames'],['label-altitude','labelAltitude'],['label-speed','labelSpeed'],['missile-outcomes','missileOutcomes']])el(id).checked=preferences[key];
el('object-scale').value=[1,3,25,50,100].includes(preferences.objectScale)?preferences.objectScale:1;
el('trail-duration').value=[60,300,-1].includes(preferences.trailDuration)?preferences.trailDuration:300;
el('label-mode').value=['all','selected','none'].includes(preferences.labelMode)?preferences.labelMode:'all';
for(const id of ['basemap','altitude-guides','velocity-guides','object-scale','trail','grid','show-history','label-names','label-altitude','label-speed','missile-outcomes','trail-duration','label-mode'])el(id).onchange=viewSettings;
viewSettings();updateFollow();
// Sahnede gösterilecek otopilot hesapları: sağ paneldeki seçimler.
const overlayInputs=[['overlay-line','overlayLine'],['overlay-ata','overlayAta'],['overlay-bearing','overlayBearing'],['overlay-range','overlayRange'],['overlay-aspect','overlayAspect'],['overlay-own-wez','overlayOwnWez'],['overlay-enemy-wez','overlayEnemyWez'],['overlay-radar','overlayRadar'],
  ['overlay-stern-wez','overlaySternWez'],['overlay-nez','overlayNez'],['overlay-mar','overlayMar'],['overlay-dor','overlayDor']];
function overlaySettings(){
  for(const [id,key] of overlayInputs)preferences[key]=el(id).checked;
  for(const id of ['overlay-ata','overlay-bearing','overlay-range','overlay-aspect'])el(id).disabled=!preferences.overlayLine;
  save();
}
for(const [id,key] of overlayInputs){el(id).checked=preferences[key];el(id).onchange=overlaySettings;}
overlaySettings();
const toolbarMenus=[...document.querySelectorAll('.toolbar-menu')];
function closeMenus(except=null){for(const menu of toolbarMenus)if(menu!==except)menu.open=false;}
function positionMenu(menu){
  const panel=menu.querySelector('.popover'),anchor=menu.querySelector('summary').getBoundingClientRect();
  if(!panel||!menu.open)return;
  const width=panel.offsetWidth,top=anchor.bottom+6;
  panel.style.left=Math.max(10,Math.min(innerWidth-width-10,anchor.right-width))+'px';
  panel.style.top=top+'px';panel.style.maxHeight=Math.max(100,innerHeight-top-12)+'px';
}
for(const menu of toolbarMenus){
  menu.addEventListener('toggle',()=>{
    menu.querySelector('summary').setAttribute('aria-expanded',String(menu.open));
    if(menu.open){closeMenus(menu);positionMenu(menu);if(menu.id==='record-options'){recordingError('');libraries();}}
  });
}
document.addEventListener('pointerdown',event=>{if(!toolbarMenus.some(menu=>menu.contains(event.target)))closeMenus();});
document.addEventListener('keydown',event=>{if(event.key==='Escape'&&toolbarMenus.some(menu=>menu.open)){const summary=toolbarMenus.find(menu=>menu.open).querySelector('summary');closeMenus();summary.focus();event.preventDefault();event.stopImmediatePropagation();}},true);
addEventListener('resize',()=>toolbarMenus.filter(menu=>menu.open).forEach(positionMenu));
new ResizeObserver(()=>toolbarMenus.filter(menu=>menu.open).forEach(positionMenu)).observe(document.querySelector('.topbar'));

function unitSettings(refresh=true){
  for(const [id,key,definitions] of [['speed-unit','speedUnit',SPEED_UNITS],['altitude-unit','altitudeUnit',ALTITUDE_UNITS],['distance-unit','distanceUnit',DISTANCE_UNITS]]){
    const value=el(id).value;preferences[key]=Object.hasOwn(definitions,value)?value:defaults[key];scene[key]=preferences[key];
  }
  el('altitude-unit-label').textContent=' '+ALTITUDE_UNITS[preferences.altitudeUnit].symbol;
  el('speed-unit-label').textContent=' '+SPEED_UNITS[preferences.speedUnit].symbol;
  el('label-speed-caption').textContent='Hız · '+SPEED_UNITS[preferences.speedUnit].symbol;
  el('reference-units').textContent='İrtifa '+ALTITUDE_UNITS[preferences.altitudeUnit].symbol+' · Hız '+SPEED_UNITS[preferences.speedUnit].symbol;
  save();if(refresh)labels();
}
for(const [id,key,definitions] of [['speed-unit','speedUnit',SPEED_UNITS],['altitude-unit','altitudeUnit',ALTITUDE_UNITS],['distance-unit','distanceUnit',DISTANCE_UNITS]]){
  el(id).value=Object.hasOwn(definitions,preferences[key])?preferences[key]:defaults[key];el(id).onchange=()=>unitSettings();
}
unitSettings(false);
el('close-settings').onclick=()=>el('settings-options').open=false;
el('reset-units').onclick=()=>{el('speed-unit').value=defaults.speedUnit;el('altitude-unit').value=defaults.altitudeUnit;el('distance-unit').value=defaults.distanceUnit;unitSettings();};

const number=(value,digits=0)=>Number(value).toLocaleString('tr-TR',{minimumFractionDigits:digits,maximumFractionDigits:digits});
function formatTime(value){const ticks=Math.round(Math.max(0,value)*10),m=Math.floor(ticks/600),s=(ticks%600)/10;return String(m).padStart(2,'0')+':'+s.toFixed(1).padStart(4,'0');}
function seek(time,pausePlayback=true){const bounds=buffer.bounds();if(!bounds||!Number.isFinite(time))return;viewTime=Math.max(bounds.first,Math.min(bounds.last,time));if(pausePlayback)paused=true;replayClock.seek(viewTime,performance.now()/1000);renderList();updateTimeline();}
function updateTimeline(){
  const bounds=buffer.complete?buffer.bounds():null;
  for(const id of ['timeline','play-pause','restart-playback','record-end','seek-time'])el(id).disabled=!bounds;
  if(bounds){el('timeline').min=bounds.first;el('timeline').max=bounds.last;if(!scrubbing)el('timeline').value=viewTime??bounds.first;
    el('timeline-last').textContent=formatTime(bounds.last);el('timeline').setAttribute('aria-valuetext',formatTime(viewTime??bounds.first));}
  if(document.activeElement!==el('seek-time'))el('seek-time').value=formatTime(viewTime??0);
  el('play-icon').hidden=!paused;el('pause-icon').hidden=paused;
  el('play-pause').setAttribute('aria-label',paused?'Oynat':'Duraklat');el('play-pause').setAttribute('aria-pressed',String(paused));
  drawEventMarks(bounds);
}
// Kayittaki olaylar zaman cubuguna isaretlenir; olay yoksa cubuk eskisi gibi bos kalir.
const eventLabels={target_assigned:'Hedef atandı',target_cleared:'Hedef kaldırıldı',missile_launched:'Fırlatma',
  missile_hit:'İsabet',missile_lost:'Kayıp',health_changed:'Sağlık değişti'};
function eventText(event){
  const parts=[eventLabels[event.type]||event.type];
  if(event.agent)parts.push(event.agent);
  if(event.ammo!=null)parts.push('mühimmat '+event.ammo);
  if(event.target)parts.push('→ '+event.target);
  if(event.type==='health_changed')parts.push(event.from+' → '+event.to);
  return parts.join(' · ')+' · '+formatTime(event.sim_time_s);
}
function drawEventMarks(bounds){
  const events=preferences.missileOutcomes?buffer.packet.events||[]:[],host=el('timeline-events');
  const span=bounds?bounds.last-bounds.first:0;
  const signature=bounds&&span>0&&events.length?buffer.packet.session+':'+events.length+':'+bounds.first+':'+bounds.last:'';
  if(host.dataset.signature===signature)return;
  host.dataset.signature=signature;host.replaceChildren();
  if(!signature)return;
  for(const event of events){
    const fraction=(event.sim_time_s-bounds.first)/span;
    if(!Number.isFinite(fraction)||fraction<0||fraction>1)continue;
    const mark=document.createElement('button');
    mark.className='timeline-event';mark.dataset.event=event.type;
    mark.style.left=(fraction*100)+'%';
    mark.title=eventText(event);mark.setAttribute('aria-label',mark.title);
    mark.onclick=()=>seek(event.sim_time_s);
    host.append(mark);
  }
}
function playPause(){const bounds=buffer.bounds();if(!bounds)return;if(paused&&viewTime>=bounds.last)seek(bounds.first);paused=!paused;replayClock.seek(viewTime,performance.now()/1000);updateTimeline();}
el('play-pause').onclick=playPause;
el('restart-playback').onclick=()=>{seek(buffer.bounds()?.first);paused=false;};el('record-end').onclick=()=>seek(buffer.bounds()?.last);
el('seek-time').onchange=()=>{const parts=el('seek-time').value.trim().replace(',','.').split(':');const time=parts.length===1?Number(parts[0]):parts.length===2?Number(parts[0])*60+Number(parts[1]):NaN;if(Number.isFinite(time)&&time>=0)seek(time);else{el('seek-time').value=formatTime(viewTime??0);}};
el('seek-time').onkeydown=event=>{if(event.key==='Enter'){el('seek-time').dispatchEvent(new Event('change'));el('seek-time').blur();}};
el('timeline').onpointerdown=()=>{scrubbing=true;};el('timeline').oninput=event=>seek(Number(event.target.value));
for(const name of ['change','pointerup','pointercancel','blur'])el('timeline').addEventListener(name,()=>{scrubbing=false;});
function setSpeed(rate){if(!playbackRates.includes(rate))return;replayClock.speed=rate;replayClock.seek(viewTime,performance.now()/1000);el('playback-speed').value=playbackRates.indexOf(rate);const label=rate.toLocaleString('tr-TR')+'×';el('playback-rate').textContent=label;el('playback-speed').setAttribute('aria-valuetext',label);for(const button of document.querySelectorAll('[data-rate]'))button.setAttribute('aria-pressed',String(Number(button.dataset.rate)===rate));}
el('playback-speed').oninput=event=>setSpeed(playbackRates[Number(event.target.value)]);
for(const [index,button] of [...document.querySelectorAll('[data-rate]')].entries()){button.style.left=(100*index/(playbackRates.length-1))+'%';button.onclick=()=>setSpeed(Number(button.dataset.rate));}
document.addEventListener('keydown',event=>{
  const target=event.target instanceof Element?event.target:null;
  if(toolbarMenus.some(menu=>menu.open)||event.altKey||event.ctrlKey||event.metaKey)return;
  if(event.code==='Space'){
    if(target?.closest('input:not([type="range"]),select,textarea,summary,[contenteditable]:not([contenteditable="false"])'))return;
    event.preventDefault();
    if(!event.repeat&&buffer.complete&&!el('play-pause').disabled)playPause();
    return;
  }
  const speedArrow=['ArrowUp','ArrowDown'].includes(event.key);
  if(target!==el('timeline')&&!(speedArrow&&target===el('playback-speed'))&&target?.closest('input,select,textarea,summary,[contenteditable]:not([contenteditable="false"])'))return;
  if(speedArrow&&!target?.closest('[role=separator]')){
    event.preventDefault();
    const index=playbackRates.indexOf(replayClock.speed),next=Math.max(0,Math.min(playbackRates.length-1,index+(event.key==='ArrowUp'?1:-1)));
    if(next!==index)setSpeed(playbackRates[next]);
    return;
  }
  if(['ArrowLeft','ArrowRight'].includes(event.key)&&!target?.closest('[role=separator]')){
    event.preventDefault();seek((viewTime??0)+(event.key==='ArrowRight'?1:-1)*(event.shiftKey?10:1),false);
  }
},true);

function allowed(entity){return !hiddenObjects.has(entity.id)&&!hiddenObjects.has(entity.owner_id)&&(filter==='all'||entity.kind===filter||entity.team===filter);}
function visibleEntities(){return viewTime===null?[]:buffer.entities(viewTime).filter(allowed);}
function renderEntities(){
  const entries=visibleEntities();if(!preferences.history||viewTime===null)return entries;
  // Kaydı biten füze kaybolmaz: son konumunda soluk kalır (uçaklardaki "son ölçüm" gibi).
  for(const [id,track] of buffer.tracks){const last=track.samples.at(-1);if(last.kind==='missile'&&viewTime>last.record_end_s&&allowed(last))entries.push({...last,measurement_ended:true});}
  return entries;
}
// Hesaplar seçili uçağın gözünden; füze seçiliyse taşıyıcısının, seçim yoksa dost uçağın.
function perspectiveId(){
  const first=buffer.tracks.get(selected)?.samples[0];
  if(first?.kind==='aircraft')return first.id;
  if(first?.owner_id)return first.owner_id;
  const aircraft=[...buffer.tracks.values()].map(track=>track.samples[0]).filter(entry=>entry.kind==='aircraft');
  return (aircraft.find(entry=>entry.team==='blue')??aircraft[0])?.id??null;
}
function analysis(){
  if(viewTime===null||!buffer.complete)return null;
  const entity=buffer.at(perspectiveId(),viewTime);
  if(!entity||!allowed(entity)||!entity.calc)return null;
  const target=entity.calc.target_entity_id?buffer.at(entity.calc.target_entity_id,viewTime):null;
  return {entity,calc:entity.calc,target:target&&allowed(target)?target:null,
    options:{line:preferences.overlayLine,ata:preferences.overlayAta,bearing:preferences.overlayBearing,
      range:preferences.overlayRange,aspect:preferences.overlayAspect,radar:preferences.overlayRadar,
      ownWez:preferences.overlayOwnWez,enemyWez:preferences.overlayEnemyWez,
      // Anahtarlar units.js ENEMY_ZONES key'leriyle aynı.
      zones:{sternWez:preferences.overlaySternWez,nez:preferences.overlayNez,mar:preferences.overlayMar,dor:preferences.overlayDor}}};
}
// C++ aircraft.hpp enum class Phase ile aynı sırada.
const PHASE_NAMES=['HOLD','FIRE','OUT','IN','CRANK','BEAM'];
function relativePanel(first,entity){
  el('relative').hidden=first.kind!=='aircraft';
  if(first.kind!=='aircraft')return;
  const calc=entity.calc,distance=value=>Number.isFinite(value)?distanceLabel(value,preferences.distanceUnit,2):'—';
  const target=calc?.target_entity_id?buffer.tracks.get(calc.target_entity_id)?.samples[0]:null;
  el('relative-target').textContent=target?'→ '+target.name:calc?.target_id?'→ '+calc.target_id:'';
  el('relative-empty').hidden=Boolean(calc);
  el('calc-phase').textContent=Number.isFinite(calc?.phase)?PHASE_NAMES[calc.phase]??'AŞAMA '+calc.phase:'—';
  el('calc-ata').textContent=sideAngleLabel(calc?.ata_deg);el('calc-bearing').textContent=angleLabel(calc?.bearing_deg);
  el('calc-aspect').textContent=angleLabel(calc?.aspect_deg);
  el('calc-range-3d').textContent=distance(calc?.range_3d_m);el('calc-range-2d').textContent=distance(calc?.range_2d_m);
  const yes=value=>value==null?'—':value?'Evet':'Hayır';
  el('calc-wez').textContent=Number.isFinite(calc?.wez_max_range_m)&&calc.wez_max_range_m>0
    ?distance(calc.wez_max_range_m):'—';
  el('calc-enemy-in-my-wez').textContent=yes(calc?.enemy_in_my_wez??calc?.in_wez);
  el('calc-me-in-enemy-wez').textContent=yes(calc?.me_in_enemy_wez);
  el('calc-mar').textContent=distance(calc?.mar_m);el('calc-dor').textContent=distance(calc?.dor_m);
  el('calc-stern-wez').textContent=distance(calc?.stern_wez_m);el('calc-nez').textContent=distance(calc?.nez_m);
}
function renderList(){
  if(!buffer.complete)return;
  for(const button of el('entity-list').querySelectorAll('.entity')){const active=button.closest('.aircraft-card').dataset.id===selected;button.classList.toggle('selected',active);button.setAttribute('aria-pressed',String(active));}
  const inventories=buffer.packet.inventory||{};
  const allAircraft=[...buffer.tracks.values()].map(track=>track.samples[0]).filter(entry=>entry.kind==='aircraft');
  const matches=entry=>!query||[entry.name,entry.model,entry.team,...(inventories[entry.id]||[]).flatMap(m=>[m.name,m.model])].join(' ').toLocaleLowerCase('tr').includes(query);
  const entries=allAircraft.filter(entry=>(filter==='all'||entry.team===filter)&&matches(entry))
    .sort((a,b)=>(teamOrder[a.team]??2)-(teamOrder[b.team]??2));
  if(selected===null&&entries.length)selected=entries.find(entry=>buffer.at(entry.id,viewTime))?.id??entries[0].id;
  const state=entry=>viewTime<entry.record_start_s?'Başlamadı':viewTime>=entry.record_end_s?'Son ölçüm':'Kayıtta';
  const signature=buffer.session+entries.map(entry=>entry.id+state(entry)+hiddenObjects.has(entry.id)+(inventories[entry.id]||[]).map(m=>m.id+munitionState(m,viewTime,preferences.missileOutcomes)+(m.id===selected)).join(',')).join('|')+(buffer.packet.unassigned_inventory||[]).map(m=>m.id+munitionState(m,viewTime,preferences.missileOutcomes)+(m.id===selected)).join(',');
  if(signature===listSignature)return;listSignature=signature;
  el('entity-list').replaceChildren();el('object-count').textContent=entries.length;
  el('list-empty').hidden=entries.length>0;el('list-empty').textContent=buffer.bounds()?'Bu filtrede uçak yok.':'Kayıt açarak başla.';
  for(const entry of entries){
    const card=document.createElement('section');card.className='aircraft-card';card.dataset.id=entry.id;
    const row=document.createElement('div');row.className='entity-row';const button=document.createElement('button');
    button.className='entity'+(entry.id===selected?' selected':'');button.dataset.kind='aircraft';button.dataset.team=entry.team;
    button.setAttribute('aria-pressed',String(entry.id===selected));button.textContent=entry.name;
    const subtitle=document.createElement('span');subtitle.textContent=entry.model+' · '+state(entry);button.append(subtitle);button.onclick=()=>selectEntity(entry.id);
    const eye=document.createElement('button');eye.className='entity-visibility';eye.textContent=hiddenObjects.has(entry.id)?'○':'◉';
    eye.setAttribute('aria-label',entry.name+(hiddenObjects.has(entry.id)?' göster':' gizle'));eye.setAttribute('aria-pressed',String(!hiddenObjects.has(entry.id)));
    eye.onclick=()=>{if(hiddenObjects.has(entry.id))hiddenObjects.delete(entry.id);else hiddenObjects.add(entry.id);listSignature='';renderList();};
    row.append(button,eye);card.append(row,inventoryPanel(inventories[entry.id]||[],viewTime,selected,selectEntity,preferences.missileOutcomes));el('entity-list').append(card);
  }
  const unassigned=(buffer.packet.unassigned_inventory||[]).filter(entry=>filter==='all'||entry.team===filter);
  if(unassigned.length){
    if(selected===null)selected=unassigned.find(entry=>entry.has_track)?.id??null;
    const section=document.createElement('section');section.className='aircraft-card unassigned';
    const title=document.createElement('h3');title.textContent='Sahibi eşleşmeyen mühimmat';
    const note=document.createElement('p');note.className='data-note';note.textContent='Kayıt, taşıyıcı uçağı ayırt etmiyor.';
    section.append(title,note,inventoryPanel(unassigned,viewTime,selected,selectEntity,preferences.missileOutcomes));el('entity-list').append(section);
  }
}
el('object-search').oninput=event=>{query=event.target.value.toLocaleLowerCase('tr');listSignature='';renderList();};
el('object-filter').onchange=event=>{filter=event.target.value;listSignature='';renderList();};

async function poll(){
  let again=500;
  try{
    const response=await fetch('/api/state?after='+buffer.cursor+'&session='+encodeURIComponent(buffer.session||''),{cache:'no-store',signal:AbortSignal.timeout(5000)});
    if(!response.ok)throw new Error('Kayıt bağlantısı açılamadı');
    const packet=await response.json();if(packet.schema_version!==3||packet.source!=='tacview-csv')throw new Error('Görüntüleyiciyi yeniden başlatıp sayfayı yenile');
    if(buffer.consume(packet)){viewTime=null;selected=null;paused=true;replayClock.reset();hiddenObjects.clear();listSignature='';scene.clear?.();}
    if(buffer.complete&&viewTime===null&&buffer.bounds()){
      viewTime=buffer.bounds().first;replayClock.seek(viewTime,performance.now()/1000);renderList();
      if(preferences.camera==='all')scene.frameAll(visibleEntities());else if(!webglError&&preferences.camera==='top')scene.setView('top');
    }
    renderList();updateTimeline();lastReceive=performance.now();connectionError=null;
    if(!buffer.complete)again=0;
  }catch(error){connectionError=error.message;again=1000;}
  setTimeout(poll,again);
}
function labels(){
  updateTimeline();updateFollow();
  const packet=buffer.packet,bounds=buffer.bounds();
  el('map-attribution').hidden=!preferences.basemap;
  el('map-status').textContent=scene.basemap.status;
  const altitudeReference=altitudeReferenceInfo(packet.manifest?.altitude_reference);
  el('altitude').closest('div').querySelector('dt').textContent=altitudeReference.label;
  el('altitude').closest('div').title=altitudeReference.note;
  el('altitude-reference-note').textContent=altitudeReference.note;
  el('record-warning').textContent=(packet.warnings||[]).join(' ');el('record-warning').hidden=!packet.warnings?.length;
  let status='Kayıt bekleniyor',notice='',error=false;
  if(connectionError){status='Bağlantı kesildi';notice=connectionError;error=true;}
  else if(packet.phase==='error'){status='Kayıt okunamadı';notice=packet.error;error=true;}
  else if(!buffer.complete){status='Yükleniyor';notice='Kayıt ölçümleri yükleniyor…';}
  else if(packet.phase==='waiting'&&bounds){status='Yeni kayıt bekleniyor';notice='Önceki kayıt gösteriliyor. Yeni dosyalar tamamlanınca açılacak.';}
  else if(bounds)status=paused?(viewTime>=bounds.last?'Kayıt sonu':'Duraklatıldı'):replayClock.speed.toLocaleString('tr-TR')+'×';
  if(webglError)notice+=(notice?' · ':'')+webglError;
  el('status').textContent=status;el('status').classList.toggle('error',error);
  el('scene-message').hidden=!notice;el('scene-message').textContent=notice;el('scene-message').classList.toggle('error',error);
  el('empty-scene').hidden=Boolean(bounds);el('record-title').textContent=bounds?(packet.recording_name||'Uçuş kaydı'):'Kayıt seç';
  el('record-source').textContent=packet.catalog_root||'Dizin seçilmedi';el('record-source').title=el('record-source').textContent;
  el('record-duration').textContent=bounds?formatTime(bounds.last-bounds.first)+' · '+packet.objects+' nesne':'Tacview CSV';
  const track=buffer.tracks.get(selected),first=track?.samples[0],last=track?.samples.at(-1);
  if(!first){el('speed-measure').hidden=false;el('mach-measure').hidden=preferences.speedUnit==='mach';el('missing-data').hidden=true;el('selected-name').textContent='Nesne seç';el('detail-title').textContent='Nesne bilgisi';el('model').textContent='Kayıt ölçümleri';el('entity-state').textContent='';for(const id of ['altitude','heading','speed','mach','roll','pitch','load-factor','throttle','east','north'])el(id).textContent='—';el('relative').hidden=true;return;}
  const current=buffer.at(selected,viewTime),entity=current||(viewTime<first.record_start_s?first:last);
  el('detail-title').textContent=first.kind==='missile'?'Füze bilgisi':'Uçak bilgisi';el('selected-name').textContent=first.name;
  el('model').textContent=first.model+' · '+(first.team==='blue'?'Dost takım':first.team==='red'?'Karşı takım':'Takım belirtilmemiş');
  el('entity-state').textContent=viewTime<first.record_start_s?'Henüz başlamadı · başlangıç ölçümü':viewTime>first.record_end_s?'Ölçüm bitti · son konum sabit':'Kaydedilmiş uçuş durumu';
  el('altitude').textContent=number(altitudeValue(entity.altitude_m,preferences.altitudeUnit),0);
  for(const [id,key] of [['heading','heading_deg'],['roll','roll_deg'],['pitch','pitch_deg']])el(id).textContent=entity[key+'_known']===false?'—':number(entity[key],1);
  const speed=entity.true_airspeed_mps??entity.position_speed_mps;
  const mach=entity.mach??entity.estimated_mach;
  const estimatedSpeed=entity.true_airspeed_mps==null&&speed!=null,estimatedMach=entity.mach==null&&mach!=null;
  const displayedSpeed=speedValue(entity,preferences.speedUnit);
  el('speed').textContent=displayedSpeed==null?'—':number(displayedSpeed,SPEED_UNITS[preferences.speedUnit].digits);
  el('mach').textContent=mach==null?'—':number(mach,2);
  el('load-factor').textContent=Number.isFinite(entity.load_factor_g)?number(entity.load_factor_g,1):'—';
  el('throttle').textContent=Number.isFinite(entity.throttle_norm)?number(entity.throttle_norm*100,0):'—';
  el('speed-measure').hidden=false;el('mach-measure').hidden=preferences.speedUnit==='mach';
  el('speed-measure').querySelector('dt').textContent=preferences.speedUnit==='mach'?'Hız · Mach':estimatedSpeed?'Hız · hesaplanan':'Hava hızı · TAS';
  el('mach-measure').querySelector('dt').textContent=estimatedMach?'Mach · tahmini':'Mach';
  el('missing-data').hidden=!estimatedSpeed&&!estimatedMach&&speed!=null&&mach!=null;
  el('missing-data').textContent=[speed==null?'Hız bilgisi yok.':null,mach==null?'Mach bilgisi yok.':null].filter(Boolean).join(' ');
  el('missing-data').hidden=!el('missing-data').textContent;
  el('speed-measure').title=estimatedSpeed?'Hız hesabında rüzgâr düzeltmesi uygulanmaz.':'';
  el('east').textContent=distanceLabel(entity.east_m,preferences.distanceUnit);el('north').textContent=distanceLabel(entity.north_m,preferences.distanceUnit);
  relativePanel(first,entity);
}

const recordingMenu=setupRecordingMenu({
  open:()=>{closeMenus(el('record-options'));el('record-options').open=true;positionMenu(el('record-options'));}
});
const libraries=()=>recordingMenu.refresh();
const recordingError=message=>recordingMenu.error(message);
el('empty-open').onclick=()=>recordingMenu.open();
const decimal=value=>Number.isFinite(value)?value.toLocaleString('tr-TR',{maximumFractionDigits:6}):null;
const rate=value=>Number.isFinite(value)?value.toLocaleString('tr-TR',{maximumFractionDigits:1}):null;
function sessionRows(packet,bounds){
  const rows=[['Kayıt',packet.recording_name],['Süre',formatTime(bounds.last-bounds.first)],
    ['Nesne',String(packet.objects)],['Olay',String((packet.events||[]).length)],
    ['Kaynak',packet.recording||'Kayıt dizini']];
  const session=packet.manifest;
  // Manifest yoksa kayıt kendini bildirmiyor: uydurma yerine bunu yazıyoruz.
  if(!session)return rows.concat([['Oturum bilgisi','manifest.json yok; kaydı yerleşik BVRGym logger’ı yazmış olabilir']]);
  const snapshots=session.snapshots,span=bounds.last-bounds.first;
  return rows.concat([
    ['Senaryo',session.agent_count!=null?session.agent_count+' ajan'+(session.duration_s!=null?' · '+session.duration_s+' s hedef süre':''):null],
    ['Enlem referansı',session.latitude_reference],
    ['İrtifa referansı',session.altitude_reference],
    ['Birim dönüşümü',session.conversions],
    ['Fizik adımı',session.physics_dt_s?decimal(session.physics_dt_s)+' s · '+rate(1/session.physics_dt_s)+' Hz':null],
    ['Komut adımı',session.aircraft_sim_time_step!=null&&session.aircraft_control_time_step!=null&&session.physics_dt_s
      ?'Sim '+session.aircraft_sim_time_step+' × Control '+session.aircraft_control_time_step+' → '
        +decimal(session.aircraft_sim_time_step*session.aircraft_control_time_step*session.physics_dt_s)+' s':null],
    ['Snapshot',snapshots!=null?snapshots+' kayıt'+(span>0?' · '+rate(snapshots/span)+' Hz':''):null],
    ['Oluşturulma',session.created_at],
    ['Koordinat notu',session.coordinate_note],
  ]);
}
function columnTable(columns){
  const details=document.createElement('details');details.className='session-columns';
  const summary=document.createElement('summary');summary.textContent='Sütun kaynakları ('+Object.keys(columns).length+')';
  const table=document.createElement('table');
  const thead=document.createElement('thead'),head=document.createElement('tr');
  for(const label of ['Sütun','Kaynak','Birim']){const th=document.createElement('th');th.textContent=label;head.append(th);}
  thead.append(head);
  const tbody=document.createElement('tbody');
  for(const [column,info] of Object.entries(columns)){
    const row=document.createElement('tr');
    for(const value of [column,info.source,info.unit]){const td=document.createElement('td');td.textContent=value??'—';row.append(td);}
    tbody.append(row);
  }
  table.append(thead,tbody);
  details.append(summary,table);return details;
}
el('record-info').onclick=event=>{
  const packet=buffer.packet,bounds=buffer.bounds();
  if(!bounds){event.preventDefault();recordingMenu.open();return;}
  el('record-details').replaceChildren();
  for(const [label,value] of sessionRows(packet,bounds)){
    const dt=document.createElement('dt'),dd=document.createElement('dd');
    dt.textContent=label;dd.textContent=value??'—';dd.classList.toggle('absent',value==null);
    el('record-details').append(dt,dd);
  }
  el('session-columns')?.remove();
  if(packet.manifest?.columns){const table=columnTable(packet.manifest.columns);table.id='session-columns';el('record-details').after(table);}
};
el('close-info').onclick=()=>el('record-info-options').open=false;

function render(timestamp){
  const canPlay=!connectionError&&timestamp-lastReceive<6000&&buffer.complete&&buffer.packet.phase==='ready'&&!paused&&!scrubbing;
  if(buffer.complete)viewTime=replayClock.tick(buffer,timestamp/1000,canPlay);
  const bounds=buffer.bounds();if(canPlay&&bounds&&viewTime>=bounds.last)paused=true;
  const tracked=buffer.at(selected,viewTime);
  if(selected&&scene.follow&&(!tracked||!allowed(tracked))){scene.follow=false;preferences.follow=false;save();updateFollow();}
  renderList();scene.analysis=analysis();const entities=buffer.complete?renderEntities():[];
  scene.draw(entities,buffer,viewTime??0,selected);ruler.draw(entities,buffer,viewTime??0);
  if(!webglError&&preferences.mode==='3d'){const north=scene.controls.target.clone().set(0,0,-1).applyQuaternion(scene.camera.quaternion.clone().invert());el('north-arrow').style.transform=`rotate(${Math.atan2(north.x,north.y)*180/Math.PI}deg)`;}
  else el('north-arrow').style.transform='none';
  if(timestamp-lastLabels>150){labels();lastLabels=timestamp;}
  requestAnimationFrame(render);
}
poll();requestAnimationFrame(render);
export {scene,buffer};
