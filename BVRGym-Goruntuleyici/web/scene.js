import {sampleWindow} from './stream.js';
import * as THREE from 'three';
import {OrbitControls} from './vendor/OrbitControls.js';
import {createF16,createMissile,createGeneric,disposeObject} from './aircraft3d.js';
import {Scene as Scene2D,updateScaleBar} from './scene2d.js';
import {MODEL_LENGTHS,ENEMY_ZONES,radarColor,altitudeLabel,speedLabel,flightLabel,analysisLabel,guideLength,visualScale,visualLength,velocityVector,VELOCITY_PIXELS_PER_MPS} from './units.js';

const point=e=>new THREE.Vector3(e.east_m,e.altitude_m,-e.north_m);
const degrees=Math.PI/180;
const colorFor=e=>e.team==='unknown'?(document.documentElement.dataset.theme==='dark'?0xaeb6bf:0x46515c):document.documentElement.dataset.theme==='dark'?(e.kind==='missile'?(e.team==='red'?0xf18c68:0xecc274):(e.team==='red'?0xe88480:0x83b9d2)):(e.kind==='missile'?(e.team==='red'?0xa83215:0x8a5700):(e.team==='red'?0xae2432:0x0756a5));

export class Scene {
  constructor(canvas,onSelect,onPan) {
    this.canvas=canvas;this.onSelect=onSelect;this.onPan=onPan;
    this.distanceScale=canvas.parentElement.querySelector('.distance-scale');
    this.follow=true;this.grid=true;this.trail=true;this.mode='3d';
    this.speedUnit='mps';this.altitudeUnit='ft';this.distanceUnit='nm';
    this.trailDuration=300;this.labelMode='all';this.labelNames=true;this.labelAltitude=true;this.labelSpeed=true;
    this.models=new Map();this.altitudeGuideCache=new WeakMap();this.lastTheme=null;
    this.hasData=false;this.objectScale=1;this.altitudeGuides=true;this.velocityGuides=true;this.lastTrailUpdate=-Infinity;
    this.world=new THREE.Scene();
    // Logaritmik derinlik: 0,5 m ile 1000 km aynı sahnede; gerçek boy uçağa yakından bakılabilsin.
    this.renderer=new THREE.WebGLRenderer({canvas,antialias:true,alpha:false,logarithmicDepthBuffer:true});
    this.renderer.setPixelRatio(Math.min(devicePixelRatio||1,2));
    this.renderer.outputColorSpace=THREE.SRGBColorSpace;
    this.renderer.toneMapping=THREE.ACESFilmicToneMapping;this.renderer.toneMappingExposure=1.1;
    this.camera=new THREE.PerspectiveCamera(44,1,.5,1000000);
    this.camera.position.set(9000,11500,11000);
    this.controls=new OrbitControls(this.camera,canvas);
    this.controls.enableDamping=true;this.controls.dampingFactor=.12;
    this.controls.minDistance=25;this.controls.maxDistance=200000;
    this.controls.maxPolarAngle=Math.PI/2-.02;
    this.controls.screenSpacePanning=false;
    // Keep orbiting compatible with follow; explicit panning releases it.
    canvas.addEventListener('pointerdown',event=>{
      // Cetvelde Shift + sol tuş döndürür; yalnız orta/sağ tuş kaydırır.
      const pans=this.ruler?event.button===1||event.button===2:event.button===2||event.ctrlKey||event.shiftKey||event.metaKey;
      if(pans){this.follow=false;this.onPan();}
    });
    canvas.addEventListener('touchstart',event=>{if(event.touches.length>1){this.follow=false;this.onPan();}},{passive:true});
    this.world.add(new THREE.HemisphereLight(0xdceeff,0x394438,2.5));
    const sun=new THREE.DirectionalLight(0xffffff,3);sun.position.set(-15000,30000,10000);this.world.add(sun);
    this.ground=new THREE.Mesh(new THREE.PlaneGeometry(400000,400000),new THREE.MeshStandardMaterial({color:0x24352b,roughness:1,metalness:0}));
    this.ground.rotation.x=-Math.PI/2;this.ground.position.y=-1;this.world.add(this.ground);
    // Draw the grid in the ground surface itself: no competing depth layers.
    this.gridUniforms={gridEnabled:{value:1},gridColor:{value:new THREE.Color(0x50725f)}};
    this.ground.material.onBeforeCompile=shader=>{
      Object.assign(shader.uniforms,this.gridUniforms);
      shader.vertexShader=shader.vertexShader.replace('#include <common>',
        '#include <common>\nvarying vec2 vGridPosition;').replace('#include <project_vertex>',
        '#include <project_vertex>\nvGridPosition=(modelMatrix*vec4(transformed,1.0)).xz;');
      shader.fragmentShader=shader.fragmentShader.replace('#include <common>',
        '#include <common>\nvarying vec2 vGridPosition;\nuniform float gridEnabled;\nuniform vec3 gridColor;')
        .replace('#include <opaque_fragment>',`
          // 2 km, 200 m ve 20 m katmanlar; hücresi ekranda seçilemeyecek kadar küçülen katman söner.
          float gridAlpha=0.0;
          vec2 gridCoordinate=vGridPosition/2000.0;
          vec2 footprint=max(fwidth(gridCoordinate),vec2(0.000001));
          for(int level=0;level<3;level++){
            float cell=level==0?2000.0:level==1?200.0:20.0;
            float weight=level==0?1.0:level==1?0.7:0.45;
            vec2 coordinate=vGridPosition/cell;
            vec2 levelFootprint=max(fwidth(coordinate),vec2(0.000001));
            vec2 lineDistance=abs(fract(coordinate+0.5)-0.5)/levelFootprint;
            // Filter subpixel lines and fade cells too small to distinguish.
            vec2 lineCoverage=1.0-smoothstep(vec2(0.1),vec2(0.9),lineDistance);
            vec2 cellFade=1.0-smoothstep(vec2(0.12),vec2(0.5),levelFootprint);
            gridAlpha=max(gridAlpha,max(lineCoverage.x*cellFade.x,lineCoverage.y*cellFade.y)*weight);
          }
          vec2 axisCoverage=1.0-smoothstep(vec2(0.45),vec2(1.55),abs(gridCoordinate)/footprint);
          gridAlpha=max(gridAlpha,max(axisCoverage.x,axisCoverage.y)*0.65);
          outgoingLight=mix(outgoingLight,gridColor,gridAlpha*gridEnabled*0.45);
          #include <opaque_fragment>
        `);
    };
    this.ground.material.customProgramCacheKey=()=> 'filtered-ground-grid-v2';
    this.labels=document.createElement('div');this.labels.className='scene-labels';canvas.parentElement.append(this.labels);
    // Otopilot hesapları: hedef çizgisi, etiketi ve radar açısı. app.js her kare this.analysis'i verir.
    this.analysis=null;
    this.targetLine=new THREE.Line(new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(),new THREE.Vector3()]),
      new THREE.LineBasicMaterial({color:0xa2d4bb,transparent:true,opacity:.95,fog:false}));
    this.radarFan=new THREE.Line(new THREE.BufferGeometry(),new THREE.LineDashedMaterial({color:0x83b9d2,transparent:true,opacity:.7,fog:false}));
    this.targetLine.visible=this.radarFan.visible=false;this.world.add(this.targetLine,this.radarFan);
    // WEZ: uçağın etrafında, kendi irtifasında yatay min–max halkası. own: seçili uçak, enemy: hedefi.
    this.wezRings={own:this.createWezRing(),enemy:this.createWezRing()};
    // Düşman merkezli bölgeler (units.js ENEMY_ZONES): her biri tek çizgi çember ve kuzey ucunda etiket.
    this.zoneRings={};for(const zone of ENEMY_ZONES)this.zoneRings[zone.key]=this.createZoneRing(zone);
    this.analysisLabel=document.createElement('div');this.analysisLabel.className='analysis-label';this.analysisLabel.hidden=true;this.labels.append(this.analysisLabel);
    this.canvas2d=document.createElement('canvas');this.canvas2d.id='scene2d';this.canvas2d.hidden=true;
    this.canvas2d.setAttribute('aria-label','Uçakların üstten görünümü');canvas.parentElement.insertBefore(this.canvas2d,this.labels);
    this.flat=new Scene2D(this.canvas2d,onSelect,()=>{this.follow=this.flat.follow;this.onPan();});
    this.basemap=this.flat.basemap;this.mapEnabled=false;this.mapMeshes=new Map();
    this.mapGroup=new THREE.Group();this.world.add(this.mapGroup);
    this.raycaster=new THREE.Raycaster();this.pointer=new THREE.Vector2();
    let press=null;
    canvas.addEventListener('pointerdown',event=>{if(event.button===0)press={x:event.clientX,y:event.clientY};});
    canvas.addEventListener('pointerup',event=>{
      if(press&&Math.hypot(event.clientX-press.x,event.clientY-press.y)<5){
        const rect=canvas.getBoundingClientRect();this.pointer.set((event.clientX-rect.left)/rect.width*2-1,-(event.clientY-rect.top)/rect.height*2+1);
        this.raycaster.setFromCamera(this.pointer,this.camera);
        const hits=this.raycaster.intersectObjects([...this.models.values()].filter(r=>r.model.visible).map(r=>r.model),true);
        let id=null;
        if(hits.length){let object=hits[0].object;while(object&&!object.userData.entityId)object=object.parent;id=object?.userData.entityId??null;}
        if(!id){
          // Gerçek boy model uzaktan birkaç piksel: tıklanan noktaya 20 px içindeki en yakın nesneyi seç.
          let best=20;
          for(const record of this.models.values()){
            if(!record.model.visible)continue;
            const p=record.model.position.clone().project(this.camera);if(p.z<-1||p.z>1)continue;
            const d=Math.hypot((p.x*.5+.5)*rect.width-(event.clientX-rect.left),(-p.y*.5+.5)*rect.height-(event.clientY-rect.top));
            if(d<best){best=d;id=record.model.userData.entityId;}
          }
        }
        if(id)this.onSelect(id);
      }
      press=null;
    });
    this.resizeObserver=new ResizeObserver(()=>this.resize());this.resizeObserver.observe(canvas.parentElement);
    this.resize();
  }

  resize() {
    const parent=this.canvas.parentElement;
    const width=parent.clientWidth,height=parent.clientHeight;
    if(!width||!height)return;
    this.renderer.setSize(width,height,false);this.camera.aspect=width/height;this.camera.updateProjectionMatrix();
    this.flat.resize();
  }

  // Kameradan `distance` uzaklıktaki noktada bir CSS pikselinin metre karşılığı.
  metersPerPixel(distance,height=this.canvas.getBoundingClientRect().height){
    return 2*distance*Math.tan(this.camera.fov*degrees/2)/(Math.max(1,height)*this.camera.zoom);
  }

  zoom(factor) {
    if(this.mode==='2d'){this.flat.zoom(factor);return;}
    const offset=this.camera.position.clone().sub(this.controls.target);
    const length=THREE.MathUtils.clamp(offset.length()/factor,this.controls.minDistance,this.controls.maxDistance);
    offset.setLength(length);this.camera.position.copy(this.controls.target).add(offset);this.controls.update();
  }

  focusEntity(entity){
    this.follow=true;this.flat.objectScale=this.objectScale;
    if(this.mode==='2d'){this.flat.focusEntity(entity);return;}
    const target=point(entity),offset=this.camera.position.clone().sub(this.controls.target);
    offset.setLength(Math.max(this.controls.minDistance,visualLength(entity.kind,this.objectScale)*6));
    this.controls.target.copy(target);this.camera.position.copy(target).add(offset);this.controls.update();this.hasData=true;
  }

  // Cetvel açıkken sol tık ölçüme gider (ruler.js yakalar). Sol tuşa PAN verilir ki
  // OrbitControls'ta Shift + sol tuş döndürme olsun; orta ve sağ tuş kaydırır.
  setRulerMode(active){
    this.ruler=active;this.flat.setRulerMode(active);
    this.controls.mouseButtons=active?{LEFT:THREE.MOUSE.PAN,MIDDLE:THREE.MOUSE.PAN,RIGHT:THREE.MOUSE.PAN}:{LEFT:THREE.MOUSE.ROTATE,MIDDLE:THREE.MOUSE.DOLLY,RIGHT:THREE.MOUSE.PAN};
  }

  // Kayıt örneğini ekran pikseline çeviren fonksiyon; kamera arkasındaysa null.
  projector(){
    if(this.mode==='2d')return this.flat.projector();
    const rect=this.canvas.getBoundingClientRect(),v=new THREE.Vector3();
    return sample=>{
      v.set(sample.east_m,sample.altitude_m,-sample.north_m).project(this.camera);
      if(v.z<-1||v.z>1)return null;
      return {x:(v.x*.5+.5)*rect.width,y:(-v.y*.5+.5)*rect.height};
    };
  }

  setMode(mode) {
    this.mode=mode;this.canvas.hidden=mode==='2d';this.canvas2d.hidden=mode!=='2d';this.labels.hidden=mode==='2d';
    this.controls.enabled=mode!=='2d';this.resize();
  }

  setView(view) {
    this.viewMode=view;
    this.controls.update();
    const distance=Math.max(3000,this.camera.position.distanceTo(this.controls.target));
    const offset=view==='top'?new THREE.Vector3(0,distance,.01):
      new THREE.Vector3(.54,.38,.7).normalize().multiplyScalar(distance);
    this.camera.position.copy(this.controls.target).add(offset);this.controls.update();
  }

  frameAll(entities) {
    if(!entities.length)return;
    const box=new THREE.Box3();entities.forEach(e=>box.expandByPoint(point(e)));
    const center=box.getCenter(new THREE.Vector3()),size=box.getSize(new THREE.Vector3());
    const distance=Math.max(7000,size.length()*1.6,center.y*1.6);
    this.controls.target.copy(center);this.camera.position.copy(center).add(new THREE.Vector3(.54,.38,.7).normalize().multiplyScalar(distance));
      this.controls.update();this.hasData=true;
    if(this.mode==='2d'){
        this.flat.frameAll(entities);
    }
  }

  addEntity(entity) {
    const model=entity.visual_model==='generic'?createGeneric(entity.kind,colorFor(entity)):entity.kind==='missile'?createMissile(colorFor(entity)):createF16(colorFor(entity));model.userData.entityId=entity.id;this.world.add(model);
    const trail=new THREE.Line(new THREE.BufferGeometry(),new THREE.LineBasicMaterial({color:colorFor(entity),transparent:true,opacity:.9,fog:false}));
    const shadowTrail=new THREE.Line(new THREE.BufferGeometry(),new THREE.LineBasicMaterial({color:0x6b8c7b,transparent:true,opacity:.4}));
    const stemGeometry=new THREE.BufferGeometry();stemGeometry.setAttribute('position',new THREE.Float32BufferAttribute(new Float32Array(6),3));
    const stem=new THREE.Line(stemGeometry,new THREE.LineBasicMaterial({color:colorFor(entity),transparent:true,opacity:.55,fog:false}));
    const velocityArrow=new THREE.ArrowHelper(new THREE.Vector3(0,0,-1),new THREE.Vector3(),1,colorFor(entity));
    velocityArrow.visible=false;
    for(const part of [velocityArrow.line,velocityArrow.cone]){part.material.transparent=true;part.material.opacity=.9;part.material.fog=false;}
    const altitudeTrace=new THREE.LineSegments(new THREE.BufferGeometry(),
      new THREE.LineBasicMaterial({color:colorFor(entity),transparent:true,opacity:.12,fog:false}));
    const altitudeCurtain=entity.kind==='aircraft'?new THREE.Mesh(new THREE.BufferGeometry(),
      new THREE.MeshBasicMaterial({color:entity.team==='red'?0xe88480:0x65afe8,vertexColors:true,transparent:true,opacity:.28,
        side:THREE.DoubleSide,depthWrite:false,fog:false})):null;
    const ring=new THREE.Mesh(new THREE.RingGeometry(.8,1,48),new THREE.MeshBasicMaterial({color:0x81d6b1,side:THREE.DoubleSide,transparent:true,opacity:.55}));
    ring.rotation.x=-Math.PI/2;ring.position.y=3;
    this.world.add(trail,shadowTrail,stem,ring,velocityArrow,altitudeTrace);
    if(altitudeCurtain)this.world.add(altitudeCurtain);
    const label=document.createElement('button');label.className='flight-label';label.onclick=()=>this.onSelect(entity.id);label.ondblclick=event=>{event.preventDefault();this.onFocus?.(entity.id);};this.labels.append(label);
    const labelName=document.createElement('span'),labelSpeedValue=document.createElement('span'),labelAltitudeValue=document.createElement('span'),labelFlightValue=document.createElement('span');
    labelName.className='flight-label-name';labelSpeedValue.className='flight-label-speed';labelAltitudeValue.className='flight-label-altitude';labelFlightValue.className='flight-label-flight';label.append(labelName,labelSpeedValue,labelAltitudeValue,labelFlightValue);
    const record={model,trail,shadowTrail,stem,ring,velocityArrow,altitudeTrace,altitudeCurtain,label,labelName,labelSpeedValue,labelAltitudeValue,labelFlightValue};this.models.set(entity.id,record);
    return record;
  }

  replaceLine(line,points) {
    line.geometry.dispose();line.geometry=new THREE.BufferGeometry().setFromPoints(points);
    if(points.length>1)line.geometry.computeBoundingSphere();
  }

  updateCurtain(mesh,points){
    const positions=[],colors=[],indices=[];
    for(const p of points){
      positions.push(p.x,p.y,p.z,p.x,4,p.z);
      colors.push(1,1,1,1,1,1,1,0);
    }
    for(let i=0;i<points.length-1;i++){
      const top=i*2;
      indices.push(top,top+1,top+2,top+1,top+3,top+2);
    }
    const geometry=new THREE.BufferGeometry();
    geometry.setAttribute('position',new THREE.Float32BufferAttribute(positions,3));
    geometry.setAttribute('color',new THREE.Float32BufferAttribute(colors,4));
    geometry.setIndex(indices);
    if(points.length)geometry.computeBoundingSphere();
    mesh.geometry.dispose();mesh.geometry=geometry;
  }

  altitudeDrops(track,time){
    const samples=track?.samples||[];
    if(!samples.length)return [];
    let cached=this.altitudeGuideCache.get(samples);
    if(!cached||cached.length!==samples.length){
      const guides=[{time:samples[0].sim_time_s,point:point(samples[0])}];
      const spacing=2*1852;let travelled=0,next=spacing;
      for(let i=1;i<samples.length;i++){
        const a=samples[i-1],b=samples[i],start=point(a),end=point(b),length=start.distanceTo(end);
        if(length>0)while(next<=travelled+length){
          const fraction=(next-travelled)/length;
          guides.push({time:a.sim_time_s+(b.sim_time_s-a.sim_time_s)*fraction,point:start.clone().lerp(end,fraction)});
          next+=spacing;
        }
        travelled+=length;
      }
      cached={length:samples.length,guides};this.altitudeGuideCache.set(samples,cached);
    }
    const drops=[],first=this.trailDuration<0?-Infinity:time-this.trailDuration;
    for(const guide of cached.guides){
      if(guide.time>time)break;
      if(guide.time<first)continue;
      const top=guide.point;drops.push(top,new THREE.Vector3(top.x,4,top.z));
    }
    return drops;
  }

  updateTrail(record,track,entity,time) {
    const samples=track?.samples||[];const visible=sampleWindow(samples,time,this.trailDuration,1800);
    const stride=Math.max(1,Math.ceil(visible.length/1800));
    const drops=entity.kind==='aircraft'?this.altitudeDrops(track,time):[];
    this.replaceLine(record.altitudeTrace,drops);
    const points=[];for(let i=0;i<visible.length;i+=stride)points.push(point(visible[i]));
    points.push(point(entity));this.replaceLine(record.trail,points);
    if(record.altitudeCurtain)this.updateCurtain(record.altitudeCurtain,points);
    this.replaceLine(record.shadowTrail,points.map(p=>new THREE.Vector3(p.x,4,p.z)));
  }

  theme() {
    const theme=document.documentElement.dataset.theme;
    if(theme===this.lastTheme)return;
    this.lastTheme=theme;
    const dark=theme==='dark';
    const sky=dark?0x142330:0xb9cdda;
    this.world.background=new THREE.Color(sky);this.world.fog=new THREE.FogExp2(sky,.000002);
    this.ground.material.color.setHex(dark?0x26392f:0xc3d2bc);
    this.gridUniforms.gridColor.value.setHex(dark?0x50725f:0x607c66);
  }

  clear() {
    this.basemap.setOrigin(null);this.clearMapMeshes();
    for(const record of this.models.values()){
      for(const object of [record.model,record.trail,record.shadowTrail,record.stem,record.ring,record.velocityArrow,record.altitudeTrace,record.altitudeCurtain].filter(Boolean)){this.world.remove(object);disposeObject(object);}
      record.label.remove();
    }
    this.models.clear();this.hasData=false;this.lastTime=null;this.lastTrailUpdate=-Infinity;
    this.targetLine.visible=this.radarFan.visible=false;this.analysisLabel.hidden=true;
    for(const ring of Object.values(this.wezRings))ring.group.visible=false;
    for(const ring of Object.values(this.zoneRings)){ring.group.visible=false;ring.label.hidden=true;}
  }

  clearMapMeshes(){
    for(const mesh of this.mapMeshes.values()){this.mapGroup.remove(mesh);mesh.geometry.dispose();mesh.material.map.dispose();mesh.material.dispose();}
    this.mapMeshes.clear();this.mapStamp='';
  }

  drawBasemap(buffer,rect){
    const map=this.basemap;map.enabled=this.mapEnabled;map.setOrigin(buffer.packet.map_origin);
    this.mapGroup.visible=this.mapEnabled;
    if(!this.mapEnabled||!map.origin)return;
    const direction=this.controls.target.clone().sub(this.camera.position).normalize();
    const ray=new THREE.Ray(this.camera.position,direction),ground=ray.intersectPlane(new THREE.Plane(new THREE.Vector3(0,1,0),0),new THREE.Vector3());
    const center=ground&&ground.distanceTo(this.controls.target)<300000?ground:this.controls.target;
    const span=Math.max(4000,this.camera.position.y*3,this.camera.position.distanceTo(this.controls.target)*2);
    map.update(center.x,-center.z,span*rect.width/Math.max(1,rect.height),span,rect.width);
    const stamp=map.originKey+':'+map.revision;
    if(this.mapStamp!==stamp){
      if(this.mapOriginKey!==map.originKey){this.clearMapMeshes();this.mapOriginKey=map.originKey;}
      const tiles=map.tiles,ids=new Set(tiles.map(tile=>tile.id));
      for(const [id,mesh] of this.mapMeshes){if(ids.has(id))continue;this.mapGroup.remove(mesh);mesh.geometry.dispose();mesh.material.map.dispose();mesh.material.dispose();this.mapMeshes.delete(id);}
      for(const tile of tiles){
        if(this.mapMeshes.has(tile.id))continue;
        const {points,segments}=tile.projection,positions=[],uv=[],indices=[],stride=segments+1;
        for(const p of points){positions.push(p.east_m,-.5,-p.north_m);uv.push(p.u,1-p.v);}
        for(let y=0;y<segments;y++)for(let x=0;x<segments;x++){const i=y*stride+x;indices.push(i,i+stride,i+1,i+1,i+stride,i+stride+1);}
        const geometry=new THREE.BufferGeometry();geometry.setAttribute('position',new THREE.Float32BufferAttribute(positions,3));geometry.setAttribute('uv',new THREE.Float32BufferAttribute(uv,2));geometry.setIndex(indices);geometry.computeVertexNormals();
        const texture=new THREE.Texture(tile.image);texture.colorSpace=THREE.SRGBColorSpace;texture.needsUpdate=true;texture.anisotropy=Math.min(8,this.renderer.capabilities.getMaxAnisotropy());
        const material=new THREE.MeshBasicMaterial({map:texture,toneMapped:false,fog:false});
        material.onBeforeCompile=this.ground.material.onBeforeCompile;material.customProgramCacheKey=()=> 'basemap-grid-v1';
        const mesh=new THREE.Mesh(geometry,material);this.mapGroup.add(mesh);this.mapMeshes.set(tile.id,mesh);
      }
      this.mapStamp=stamp;
    }
    const dark=document.documentElement.dataset.theme==='dark';
    for(const mesh of this.mapMeshes.values())mesh.material.color.setHex(dark?0x8193a0:0xffffff);
  }

  createWezRing(){
    const fill=new THREE.Mesh(new THREE.BufferGeometry(),new THREE.MeshBasicMaterial({transparent:true,side:THREE.DoubleSide,depthWrite:false,fog:false}));
    const edge=new THREE.LineSegments(new THREE.BufferGeometry(),new THREE.LineBasicMaterial({transparent:true,fog:false}));
    const group=new THREE.Group();group.add(fill,edge);group.visible=false;this.world.add(group);
    return {group,fill,edge,radii:''};
  }

  // zone.style: 'line' düz, 'dash' kesikli çizgi, 'hatch' içi taralı daire + çizgi (units.js ENEMY_ZONES).
  createZoneRing(zone){
    const material=zone.style==='dash'
      ?new THREE.LineDashedMaterial({transparent:true,fog:false})
      :new THREE.LineBasicMaterial({transparent:true,fog:false});
    const line=new THREE.LineLoop(new THREE.BufferGeometry(),material);
    const group=new THREE.Group();group.add(line);group.visible=false;this.world.add(group);
    let fill=null;
    if(zone.style==='hatch'){
      // Tarama: saydam zeminde beyaz çapraz çizgi dokusu; malzeme rengi bölge rengine boyar.
      // Daire çapı boyunca 28 çizgi: yarıçap ne olursa olsun tarama sıklığı aynı görünür.
      const tile=document.createElement('canvas');tile.width=tile.height=32;
      const pen=tile.getContext('2d');pen.strokeStyle='#fff';pen.lineWidth=5;
      pen.beginPath();pen.moveTo(-8,40);pen.lineTo(40,-8);pen.moveTo(-8,8);pen.lineTo(8,-8);pen.moveTo(24,40);pen.lineTo(40,24);pen.stroke();
      const texture=new THREE.CanvasTexture(tile);texture.wrapS=texture.wrapT=THREE.RepeatWrapping;texture.repeat.set(28,28);
      fill=new THREE.Mesh(new THREE.BufferGeometry(),new THREE.MeshBasicMaterial({map:texture,transparent:true,side:THREE.DoubleSide,depthWrite:false,fog:false}));
      group.add(fill);
    }
    const label=document.createElement('div');label.className='zone-label';label.hidden=true;this.labels.append(label);
    return {group,line,fill,label,radius:''};
  }

  // Bölge çemberi: hedefin irtifasında yatay; yarıçap değişmedikçe geometri yeniden kurulmaz.
  // active: seçili uçak çemberin içinde; çizgi ve etiket belirginleşir.
  drawZoneRing(ring,entity,radius,zone,active,rect){
    const key=String(Math.round(radius));
    if(ring.radius!==key){
      ring.radius=key;
      const points=[];
      for(let i=0;i<160;i++){const a=i/160*Math.PI*2;points.push(new THREE.Vector3(Math.sin(a)*radius,0,Math.cos(a)*radius));}
      ring.line.geometry.dispose();ring.line.geometry=new THREE.BufferGeometry().setFromPoints(points);
      if(zone.style==='dash'){
        // Kesik boyu yarıçapla orantılı: her yarıçapta çevrede ~70 kesik.
        ring.line.computeLineDistances();ring.line.material.dashSize=radius/14;ring.line.material.gapSize=radius/22;
      }
      if(ring.fill){
        const disk=new THREE.CircleGeometry(radius,160);disk.rotateX(-Math.PI/2);
        ring.fill.geometry.dispose();ring.fill.geometry=disk;
      }
    }
    const center=point(entity),color=document.documentElement.dataset.theme==='dark'?zone.dark:zone.light;
    ring.group.position.copy(center);ring.line.material.color.set(color);ring.line.material.opacity=active?1:.6;
    if(ring.fill){ring.fill.material.color.set(color);ring.fill.material.opacity=active?.6:.3;}
    // Etiket çemberin kuzey ucunda (kuzey -z); kameranın arkasındaysa gizli.
    const top=center.add(new THREE.Vector3(0,0,-radius)).project(this.camera);
    ring.label.hidden=top.z<-1||top.z>1;
    if(ring.label.hidden)return;
    ring.label.textContent=zone.label;ring.label.style.color=ring.label.style.borderColor=color;ring.label.classList.toggle('active',active);
    const x=Math.round((top.x*.5+.5)*rect.width),y=Math.round((-top.y*.5+.5)*rect.height);
    ring.label.style.transform=`translate(${x}px,${y}px) translate(-50%,-100%)`;
  }

  // Halkayı min–max yarıçapla çiz; yarıçap değişmedikçe geometri yeniden kurulmaz.
  // active: karşı uçak bu WEZ'in içinde; halka belirginleşir.
  drawWezRing(ring,entity,minM,maxM,color,active){
    ring.group.visible=maxM>minM&&maxM>0;
    if(!ring.group.visible)return;
    const radii=Math.round(minM)+':'+Math.round(maxM);
    if(ring.radii!==radii){
      ring.radii=radii;
      const fill=new THREE.RingGeometry(Math.max(minM,.1),maxM,160,1);fill.rotateX(-Math.PI/2);
      ring.fill.geometry.dispose();ring.fill.geometry=fill;
      const points=[];
      for(const radius of [minM,maxM]){
        if(radius<=0)continue;
        for(let i=0;i<160;i++){
          const a=i/160*Math.PI*2,b=(i+1)/160*Math.PI*2;
          points.push(new THREE.Vector3(Math.sin(a)*radius,0,Math.cos(a)*radius),new THREE.Vector3(Math.sin(b)*radius,0,Math.cos(b)*radius));
        }
      }
      ring.edge.geometry.dispose();ring.edge.geometry=new THREE.BufferGeometry().setFromPoints(points);
    }
    ring.group.position.copy(point(entity));
    ring.fill.material.color.setHex(color);ring.fill.material.opacity=active?.22:.08;
    ring.edge.material.color.setHex(color);ring.edge.material.opacity=active?.95:.55;
  }

  // Radar açısı: radar_gimbal_deg yarım açı, burundan sağa ve sola (60 = ±60).
  // Radar menzili uzunluğunda iki kenar ve aradaki yay; uçağın irtifasında yatay.
  radarPoints(entity,calc){
    const center=point(entity),gimbal=calc.radar_gimbal_deg,range=calc.radar_range_m;
    const at=angle=>{const rad=(entity.heading_deg+angle)*degrees;return new THREE.Vector3(center.x+Math.sin(rad)*range,center.y,center.z-Math.cos(rad)*range);};
    const points=[at(-gimbal),center,at(gimbal)];
    for(let i=1;i<=64;i++)points.push(at(gimbal-2*gimbal*i/64));
    return points;
  }

  drawAnalysis(rect){
    const a=this.analysis,dark=document.documentElement.dataset.theme==='dark';
    const showLine=Boolean(a?.options.line&&a.target);
    this.targetLine.visible=showLine;this.analysisLabel.hidden=true;
    if(showLine){
      const from=point(a.entity),to=point(a.target),position=this.targetLine.geometry.attributes.position;
      position.setXYZ(0,from.x,from.y,from.z);position.setXYZ(1,to.x,to.y,to.z);position.needsUpdate=true;
      this.targetLine.geometry.computeBoundingSphere();this.targetLine.material.color.setHex(dark?0xa2d4bb:0x2c8062);
      // Çizginin ortası ekran dışındaysa etiketi seçili uçağa doğru, görünen kısma kaydır.
      const parts=analysisLabel(a.calc,a.options,this.distanceUnit);
      const inside=p=>p.z>=-1&&p.z<=1&&Math.abs(p.x)<.95&&Math.abs(p.y)<.95;
      const middle=Array.from({length:18},(_,i)=>.5/2**i).map(f=>from.clone().lerp(to,f).project(this.camera)).find(inside);
      if(parts.length&&middle){
        this.analysisLabel.textContent=parts.join(' · ');this.analysisLabel.hidden=false;
        // Değer değiştikçe (eksi, basamak) genişlik oynar, ortalanan etiket titrer: genişlik yalnız büyür,
        // gösterilen alanlar değişince sıfırlanır. Konum tam piksele yuvarlanır.
        const key=parts.map(p=>p.split(' ')[0]).join();
        if(this.analysisKey!==key){this.analysisKey=key;this.analysisWidth=0;this.analysisLabel.style.minWidth='';}
        this.analysisWidth=Math.max(this.analysisWidth,this.analysisLabel.offsetWidth);
        this.analysisLabel.style.minWidth=this.analysisWidth+'px';
        const width=this.analysisWidth,height=this.analysisLabel.offsetHeight;
        const x=Math.round(Math.max(8,Math.min(rect.width-width-8,(middle.x*.5+.5)*rect.width-width/2)));
        const y=Math.round(Math.max(8,Math.min(rect.height-height-8,(-middle.y*.5+.5)*rect.height-height-6)));
        this.analysisLabel.style.transform=`translate(${x}px,${y}px)`;
      }
    }
    const radar=Boolean(a?.options.radar&&a.entity.heading_deg_known!==false&&a.calc.radar_gimbal_deg>0&&a.calc.radar_range_m>0);
    this.radarFan.visible=radar;
    if(radar){
      this.replaceLine(this.radarFan,this.radarPoints(a.entity,a.calc));this.radarFan.computeLineDistances();
      this.radarFan.material.dashSize=a.calc.radar_range_m/200;this.radarFan.material.gapSize=a.calc.radar_range_m/260;
      this.radarFan.material.color.set(radarColor(a.entity.team,dark));
    }
    // Dost WEZ: seçili uçağın kendi hesabı. Düşman WEZ: hedefin kendi hesabı (onun satırı).
    const own=a?.options.ownWez?a.calc:null,enemy=a?.options.enemyWez&&a.target?a.target.calc:null;
    if(own)this.drawWezRing(this.wezRings.own,a.entity,own.wez_min_range_m,own.wez_max_range_m,colorFor(a.entity),Boolean(own.enemy_in_my_wez));
    else this.wezRings.own.group.visible=false;
    if(enemy)this.drawWezRing(this.wezRings.enemy,a.target,enemy.wez_min_range_m,enemy.wez_max_range_m,colorFor(a.target),Boolean(enemy.enemy_in_my_wez));
    else this.wezRings.enemy.group.visible=false;
    // Düşman merkezli bölgeler: yarıçap seçili uçağın hesabı (onun satırı), çember hedefin etrafında.
    for(const zone of ENEMY_ZONES){
      const ring=this.zoneRings[zone.key],radius=a?.calc[zone.field];
      const show=Boolean(a?.options.zones[zone.key]&&a.target&&radius>0);
      ring.group.visible=show;ring.label.hidden=!show;
      if(show)this.drawZoneRing(ring,a.target,radius,zone,a.calc.range_3d_m<radius,rect);
    }
  }

  draw(entities,buffer,time,selected) {
    this.flat.mapEnabled=this.mapEnabled;
    this.flat.speedUnit=this.speedUnit;this.flat.altitudeUnit=this.altitudeUnit;this.flat.distanceUnit=this.distanceUnit;
    this.flat.follow=this.follow;this.flat.grid=this.grid;this.flat.trail=this.trail;
    this.flat.analysis=this.analysis;this.flat.objectScale=this.objectScale;this.flat.velocityGuides=this.velocityGuides;this.flat.trailDuration=this.trailDuration;this.flat.labelMode=this.labelMode;this.flat.labelNames=this.labelNames;this.flat.labelAltitude=this.labelAltitude;this.flat.labelSpeed=this.labelSpeed;
    if(this.mode==='2d'){this.flat.draw(entities,buffer,time,selected);return;}
    this.theme();
    this.controls.minDistance=Math.max(25,visualLength('aircraft',this.objectScale)*1.7);
    const ids=new Set(entities.map(e=>e.id));
    for(const [id,record] of this.models)if(!ids.has(id)){
      for(const item of [record.model,record.trail,record.shadowTrail,record.stem,record.ring,record.velocityArrow,record.altitudeTrace,record.altitudeCurtain].filter(Boolean)){this.world.remove(item);disposeObject(item);}
      record.label.remove();this.models.delete(id);
    }
    const focus=entities.find(e=>e.id===selected&&e.visible!==false);
    const focusPosition=focus?point(focus):null;
    if(focusPosition&&!this.hasData){
      this.controls.target.copy(focusPosition);this.camera.position.copy(focusPosition).add(new THREE.Vector3(9000,6500,11000));
      this.hasData=true;if(this.viewMode==='top')this.setView('top');
    }
    this.controls.update();
    if(this.follow&&focusPosition){
      // Recenter after orbit damping, including when follow resumes after a pan.
      const delta=focusPosition.clone().sub(this.controls.target);
      this.camera.position.add(delta);this.controls.target.copy(focusPosition);
      this.camera.updateMatrixWorld();
    }
    this.gridUniforms.gridEnabled.value=this.grid?1:0;
    const now=performance.now(),updateTrails=now-this.lastTrailUpdate>160||time<this.lastTime;
    const rect=this.canvas.getBoundingClientRect();
    const velocityScale=this.metersPerPixel(this.camera.position.distanceTo(this.controls.target),rect.height)*VELOCITY_PIXELS_PER_MPS;
    this.velocityMetersPerMps=velocityScale;
    this.drawBasemap(buffer,rect);
    const labelRects=[];
    for(const entity of entities){
      let record=this.models.get(entity.id);const fresh=!record;
      if(fresh)record=this.addEntity(entity);
      const position=point(entity);record.model.position.copy(position);
      record.model.visible=entity.visible!==false;
      record.model.rotation.set(entity.pitch_deg*degrees,-entity.heading_deg*degrees,-entity.roll_deg*degrees,'YXZ');
      const distance=this.camera.position.distanceTo(position);
      // Büyütme 1× gerçek boy; yakınlaştırma ekrandaki boyu değiştirir. Çizgiler büyütülmüş boyla orantılı.
      const scale=visualScale(entity.kind,this.objectScale),modelLength=visualLength(entity.kind,this.objectScale);
      record.model.scale.setScalar(scale);
      const metersPerPixel=this.metersPerPixel(distance,rect.height);
      const isSelected=entity.id===selected;
      const dark=document.documentElement.dataset.theme==='dark',color=colorFor(entity);
      for(const line of [record.trail,record.stem,record.altitudeTrace])line.material.color.setHex(color);
      record.velocityArrow.setColor(color);
      record.shadowTrail.material.color.setHex(dark?0x6b8c7b:0x344b3c);
      record.model.userData.bodyMaterial.color.setHex(color);
      record.model.userData.bodyMaterial.emissive.setHex(colorFor(entity));record.model.userData.bodyMaterial.emissiveIntensity=dark?.15:0;
      record.trail.visible=this.trail;record.shadowTrail.visible=this.trail&&this.grid;
      const showAltitude=this.altitudeGuides&&entity.kind==='aircraft';
      record.stem.visible=showAltitude&&entity.visible!==false;
      record.stem.material.opacity=isSelected?.75:.35;
      record.ring.visible=showAltitude&&entity.visible!==false;
      record.altitudeTrace.visible=showAltitude&&this.trail;
      if(record.altitudeCurtain){
        record.altitudeCurtain.visible=showAltitude&&this.trail;
        record.altitudeCurtain.material.color.setHex(entity.team==='unknown'?color:dark?(entity.team==='red'?0xe88480:0x65afe8):color);
        record.altitudeCurtain.material.opacity=dark?(isSelected?.32:.22):(isSelected?.42:.32);
      }
      const velocity=velocityVector(entity);
      record.velocityArrow.visible=this.velocityGuides&&Boolean(velocity)&&entity.visible!==false;
      if(record.velocityArrow.visible){
        const direction=new THREE.Vector3(velocity.east,velocity.up,-velocity.north).normalize();
        const length=velocity.speed*velocityScale,head=Math.min(length*.25,metersPerPixel*10);
        record.velocityArrow.position.copy(position).addScaledVector(direction,modelLength*.55);
        record.velocityArrow.setDirection(direction);record.velocityArrow.setLength(length,head,head*.6);
      }
      if(updateTrails||fresh){this.updateTrail(record,buffer.tracks.get(entity.id),entity,time);}
      record.stem.geometry.attributes.position.setXYZ(0,position.x,4,position.z);
      record.stem.geometry.attributes.position.setXYZ(1,position.x,position.y,position.z);
      record.stem.geometry.attributes.position.needsUpdate=true;record.stem.geometry.computeBoundingSphere();
      // Yer halkası model boyunun 0,6 katı yarıçapında; uzaktan en az 6 px.
      record.ring.position.set(position.x,4,position.z);
      record.ring.scale.setScalar(guideLength(visualLength('aircraft',this.objectScale),.6,this.metersPerPixel(this.camera.position.distanceTo(record.ring.position),rect.height),6));
      const projected=position.clone().project(this.camera);
      const visible=entity.visible!==false&&this.labelMode!=='none'&&(this.labelMode!=='selected'||isSelected)&&(this.labelNames||this.labelAltitude||this.labelSpeed)&&projected.z>=-1&&projected.z<=1&&Math.abs(projected.x)<1.1&&Math.abs(projected.y)<1.1;
      record.label.hidden=!visible;record.label.classList.toggle('selected',isSelected);record.label.classList.toggle('stale',Boolean(entity.measurement_ended));
      record.labelName.textContent=this.labelNames?entity.name:'';record.labelName.hidden=!this.labelNames;
      record.labelSpeedValue.textContent=this.labelSpeed?speedLabel(entity,this.speedUnit):'';record.labelSpeedValue.hidden=!this.labelSpeed;
      record.labelAltitudeValue.textContent=this.labelAltitude?altitudeLabel(entity.altitude_m,this.altitudeUnit):'';record.labelAltitudeValue.hidden=!this.labelAltitude;
      const isMissile=entity.kind==='missile';
      record.labelFlightValue.textContent=isMissile?flightLabel(entity,this.distanceUnit):'';record.labelFlightValue.hidden=!isMissile;
      record.label.title=[entity.measurement_ended?'Son ölçüm · konum sabit':null,record.labelName.textContent,record.labelSpeedValue.textContent,record.labelAltitudeValue.textContent,isMissile?'atıştan beri '+record.labelFlightValue.textContent:null].filter(Boolean).join(' · ');
      record.label.style.borderColor='#'+colorFor(entity).toString(16);
      const labelOffset=Math.max(26,modelLength/metersPerPixel/2+8);
      if(visible){
        const width=record.label.offsetWidth,height=record.label.offsetHeight;
        const motion=velocity?position.clone().add(new THREE.Vector3(velocity.east,velocity.up,-velocity.north)).project(this.camera):null;
        const arrowPointsRight=motion&&motion.x-projected.x>.25*Math.hypot(motion.x-projected.x,motion.y-projected.y);
        const offset=record.velocityArrow.visible&&arrowPointsRight?-width-labelOffset:labelOffset;
        const x=Math.max(8,Math.min(rect.width-width-8,(projected.x*.5+.5)*rect.width+offset));
        const initialY=Math.max(8,Math.min(rect.height-height-8,(-projected.y*.5+.5)*rect.height-height/2));
        let y=initialY;
        for(let i=0;i<labelRects.length+1;i++){
          const overlap=labelRects.find(r=>x<r.right+3&&x+width>r.x-3&&y<r.bottom+3&&y+height>r.y-3);
          if(!overlap)break;
          y=overlap.bottom+3;
          if(y+height>rect.height-8)y=Math.max(8,initialY-(i+1)*(height+3));
        }
        labelRects.push({x,y,right:x+width,bottom:y+height});
        record.label.style.transform=`translate(${x}px,${y}px)`;
      }
    }
    if(updateTrails)this.lastTrailUpdate=now;
    this.drawAnalysis(rect);
    this.lastTime=time;this.renderer.render(this.world,this.camera);
    // Perspective scale at the orbit target's depth, in CSS pixels.
    updateScaleBar(this.distanceScale,this.metersPerPixel(this.camera.position.distanceTo(this.controls.target),rect.height),rect.width,'3d',this.distanceUnit);
  }
}



