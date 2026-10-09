import * as THREE from 'three';
import {OrbitControls} from './vendor/OrbitControls.js';
import {createF16,createMissile,disposeObject} from './aircraft3d.js';
import {Scene as Scene2D,updateScaleBar} from './scene2d.js';
import {MODEL_LENGTHS,altitudeLabel,speedLabel,visualScale} from './units.js';

const point=e=>new THREE.Vector3(e.east_m,e.altitude_m,-e.north_m);
const degrees=Math.PI/180;
const colorFor=e=>document.documentElement.dataset.theme==='dark'?(e.kind==='missile'?(e.team==='red'?0xf18c68:0xecc274):(e.team==='red'?0xe88480:0x83b9d2)):(e.kind==='missile'?(e.team==='red'?0xa83215:0x8a5700):(e.team==='red'?0xae2432:0x0756a5));

export class Scene {
  constructor(canvas,onSelect,onPan) {
    this.canvas=canvas;this.onSelect=onSelect;this.onPan=onPan;
    this.distanceScale=canvas.parentElement.querySelector('.distance-scale');
    this.follow=true;this.grid=true;this.trail=true;this.mode='3d';
    this.speedUnit='mps';this.altitudeUnit='ft';this.distanceUnit='nm';
    this.trailDuration=300;this.labelMode='all';this.labelNames=true;this.labelAltitude=true;this.labelSpeed=true;
    this.models=new Map();this.lastTheme=null;
    this.hasData=false;this.objectScale=50;this.altitudeGuides=true;this.headingGuides=true;this.lastTrailUpdate=-Infinity;
    this.world=new THREE.Scene();
    this.renderer=new THREE.WebGLRenderer({canvas,antialias:true,alpha:false});
    this.renderer.setPixelRatio(Math.min(devicePixelRatio||1,2));
    this.renderer.outputColorSpace=THREE.SRGBColorSpace;
    this.renderer.toneMapping=THREE.ACESFilmicToneMapping;this.renderer.toneMappingExposure=1.1;
    this.camera=new THREE.PerspectiveCamera(44,1,10,1000000);
    this.camera.position.set(9000,11500,11000);
    this.controls=new OrbitControls(this.camera,canvas);
    this.controls.enableDamping=true;this.controls.dampingFactor=.12;
    this.controls.minDistance=900;this.controls.maxDistance=200000;
    this.controls.maxPolarAngle=Math.PI/2-.02;
    this.controls.screenSpacePanning=false;
    // Keep orbiting compatible with follow; explicit panning releases it.
    canvas.addEventListener('pointerdown',event=>{
      if(event.button===2||event.ctrlKey||event.shiftKey||event.metaKey){this.follow=false;this.onPan();}
    });
    canvas.addEventListener('touchstart',event=>{if(event.touches.length>1){this.follow=false;this.onPan();}},{passive:true});
    this.world.add(new THREE.HemisphereLight(0xdceeff,0x394438,2.5));
    const sun=new THREE.DirectionalLight(0xffffff,3);sun.position.set(-15000,30000,10000);this.world.add(sun);
    this.ground=new THREE.Mesh(new THREE.PlaneGeometry(400000,400000),new THREE.MeshStandardMaterial({color:0x24352b,roughness:1,metalness:0}));
    this.ground.rotation.x=-Math.PI/2;this.ground.position.y=-1;this.world.add(this.ground);
    this.groundGrid=new THREE.GridHelper(160000,80,0x5c7765,0x344f43);this.world.add(this.groundGrid);
    const axisGeometry=new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(-80000,2,0),new THREE.Vector3(80000,2,0),new THREE.Vector3(0,2,-80000),new THREE.Vector3(0,2,80000)]);
    this.axes=new THREE.LineSegments(axisGeometry,new THREE.LineBasicMaterial({color:0x83a395,transparent:true,opacity:.5}));this.world.add(this.axes);
    this.labels=document.createElement('div');this.labels.className='scene-labels';canvas.parentElement.append(this.labels);
    this.canvas2d=document.createElement('canvas');this.canvas2d.id='scene2d';this.canvas2d.hidden=true;
    this.canvas2d.setAttribute('aria-label','Uçakların üstten görünümü');canvas.parentElement.insertBefore(this.canvas2d,this.labels);
    this.flat=new Scene2D(this.canvas2d,onSelect,()=>{this.follow=this.flat.follow;this.onPan();});
    this.raycaster=new THREE.Raycaster();this.pointer=new THREE.Vector2();
    let press=null;
    canvas.addEventListener('pointerdown',event=>{if(event.button===0)press={x:event.clientX,y:event.clientY};});
    canvas.addEventListener('pointerup',event=>{
      if(press&&Math.hypot(event.clientX-press.x,event.clientY-press.y)<5){
        const rect=canvas.getBoundingClientRect();this.pointer.set((event.clientX-rect.left)/rect.width*2-1,-(event.clientY-rect.top)/rect.height*2+1);
        this.raycaster.setFromCamera(this.pointer,this.camera);
        const hits=this.raycaster.intersectObjects([...this.models.values()].filter(r=>r.model.visible).map(r=>r.model),true);
        if(hits.length){let object=hits[0].object;while(object&&!object.userData.entityId)object=object.parent;if(object)this.onSelect(object.userData.entityId);}
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
    const length=(MODEL_LENGTHS[entity.kind]||MODEL_LENGTHS.aircraft)*visualScale(entity.kind,this.objectScale);
    offset.setLength(Math.max(3500,length*5));
    this.controls.target.copy(target);this.camera.position.copy(target).add(offset);this.controls.update();this.hasData=true;
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
    const model=entity.kind==='missile'?createMissile(colorFor(entity)):createF16(colorFor(entity));model.userData.entityId=entity.id;this.world.add(model);
    const noseOffset=-new THREE.Box3().setFromObject(model).min.z;
    const trail=new THREE.Line(new THREE.BufferGeometry(),new THREE.LineBasicMaterial({color:colorFor(entity),transparent:true,opacity:.9,fog:false}));
    const shadowTrail=new THREE.Line(new THREE.BufferGeometry(),new THREE.LineBasicMaterial({color:0x6b8c7b,transparent:true,opacity:.4}));
    const stemGeometry=new THREE.BufferGeometry();stemGeometry.setAttribute('position',new THREE.Float32BufferAttribute(new Float32Array(6),3));
    const stem=new THREE.Line(stemGeometry,new THREE.LineBasicMaterial({color:colorFor(entity),transparent:true,opacity:.55,fog:false}));
    const heading=new THREE.Line(new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(),new THREE.Vector3()]),
      new THREE.LineDashedMaterial({color:colorFor(entity),transparent:true,opacity:.85,dashSize:80,gapSize:55,fog:false}));
    const altitudeTrace=new THREE.LineSegments(new THREE.BufferGeometry(),
      new THREE.LineBasicMaterial({color:colorFor(entity),transparent:true,opacity:.12,fog:false}));
    const altitudeCurtain=entity.kind==='aircraft'?new THREE.Mesh(new THREE.BufferGeometry(),
      new THREE.MeshBasicMaterial({color:entity.team==='red'?0xe88480:0x65afe8,vertexColors:true,transparent:true,opacity:.28,
        side:THREE.DoubleSide,depthWrite:false,fog:false})):null;
    const ring=new THREE.Mesh(new THREE.RingGeometry(.8,1,48),new THREE.MeshBasicMaterial({color:0x81d6b1,side:THREE.DoubleSide,transparent:true,opacity:.55}));
    ring.rotation.x=-Math.PI/2;ring.position.y=3;
    this.world.add(trail,shadowTrail,stem,ring,heading,altitudeTrace);
    if(altitudeCurtain)this.world.add(altitudeCurtain);
    const label=document.createElement('button');label.className='flight-label';label.onclick=()=>this.onSelect(entity.id);label.ondblclick=event=>{event.preventDefault();this.onFocus?.(entity.id);};this.labels.append(label);
    const labelName=document.createElement('span'),labelSpeedValue=document.createElement('span'),labelAltitudeValue=document.createElement('span');
    labelName.className='flight-label-name';labelSpeedValue.className='flight-label-speed';labelAltitudeValue.className='flight-label-altitude';label.append(labelName,labelSpeedValue,labelAltitudeValue);
    const record={model,trail,shadowTrail,stem,ring,heading,altitudeTrace,altitudeCurtain,noseOffset,label,labelName,labelSpeedValue,labelAltitudeValue};this.models.set(entity.id,record);
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

  updateTrail(record,track,entity,time) {
    const samples=track?.samples||[];const visible=samples.filter(s=>s.sim_time_s<=time&&(this.trailDuration<0||s.sim_time_s>=time-this.trailDuration));
    const stride=Math.max(1,Math.ceil(visible.length/1800));
    const drops=[],dropStride=Math.max(1,Math.ceil(visible.length/12));
    if(entity.kind==='aircraft')for(let i=0;i<visible.length;i+=dropStride){const top=point(visible[i]);drops.push(top,new THREE.Vector3(top.x,4,top.z));}
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
    this.groundGrid.material.color.setHex(dark?0x50725f:0x607c66);
  }

  clear() {
    for(const record of this.models.values()){
      for(const object of [record.model,record.trail,record.shadowTrail,record.stem,record.ring,record.heading,record.altitudeTrace,record.altitudeCurtain].filter(Boolean)){this.world.remove(object);disposeObject(object);}
      record.label.remove();
    }
    this.models.clear();this.hasData=false;this.lastTime=null;this.lastTrailUpdate=-Infinity;
  }

  draw(entities,buffer,time,selected) {
    this.flat.speedUnit=this.speedUnit;this.flat.altitudeUnit=this.altitudeUnit;this.flat.distanceUnit=this.distanceUnit;
    this.flat.follow=this.follow;this.flat.grid=this.grid;this.flat.trail=this.trail;
    this.flat.objectScale=this.objectScale;this.flat.headingGuides=this.headingGuides;this.flat.trailDuration=this.trailDuration;this.flat.labelMode=this.labelMode;this.flat.labelNames=this.labelNames;this.flat.labelAltitude=this.labelAltitude;this.flat.labelSpeed=this.labelSpeed;
    if(this.mode==='2d'){this.flat.draw(entities,buffer,time,selected);return;}
    this.theme();
    const ids=new Set(entities.map(e=>e.id));
    for(const [id,record] of this.models)if(!ids.has(id)){
      for(const item of [record.model,record.trail,record.shadowTrail,record.stem,record.ring,record.heading,record.altitudeTrace,record.altitudeCurtain].filter(Boolean)){this.world.remove(item);disposeObject(item);}
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
    this.groundGrid.visible=this.grid;this.axes.visible=this.grid;
    const now=performance.now(),updateTrails=now-this.lastTrailUpdate>160||time<this.lastTime;
    const rect=this.canvas.getBoundingClientRect();
    const labelRects=[];
    for(const entity of entities){
      let record=this.models.get(entity.id);const fresh=!record;
      if(fresh)record=this.addEntity(entity);
      const position=point(entity);record.model.position.copy(position);
      record.model.visible=entity.visible!==false;
      record.model.rotation.set(entity.pitch_deg*degrees,-entity.heading_deg*degrees,-entity.roll_deg*degrees,'YXZ');
      const distance=this.camera.position.distanceTo(position);
      // Fixed world-space visual scale: zoom changes the apparent size naturally.
      const scale=visualScale(entity.kind,this.objectScale);record.model.scale.setScalar(scale);
      const isSelected=entity.id===selected;
      const dark=document.documentElement.dataset.theme==='dark',color=colorFor(entity);
      for(const line of [record.trail,record.stem,record.heading,record.altitudeTrace])line.material.color.setHex(color);
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
        record.altitudeCurtain.material.color.setHex(dark?(entity.team==='red'?0xe88480:0x65afe8):color);
        record.altitudeCurtain.material.opacity=dark?(isSelected?.32:.22):(isSelected?.42:.32);
      }
      record.heading.visible=this.headingGuides&&entity.visible!==false;
      if(record.heading.visible){
        const length=MODEL_LENGTHS[entity.kind]||MODEL_LENGTHS.aircraft;
        const guideLength=Math.max(250,length*scale*.9);
        const forward=new THREE.Vector3(0,0,-1).applyQuaternion(record.model.quaternion);
        const start=position.clone().addScaledVector(forward,record.noseOffset*scale);
        const end=start.clone().addScaledVector(forward,guideLength);
        record.heading.geometry.attributes.position.setXYZ(0,start.x,start.y,start.z);
        record.heading.geometry.attributes.position.setXYZ(1,end.x,end.y,end.z);
        record.heading.geometry.attributes.position.needsUpdate=true;
        record.heading.geometry.computeBoundingSphere();record.heading.computeLineDistances();
        record.heading.material.dashSize=guideLength/7;record.heading.material.gapSize=guideLength/11;
      }
      if(updateTrails||fresh){this.updateTrail(record,buffer.tracks.get(entity.id),entity,time);}
      record.stem.geometry.attributes.position.setXYZ(0,position.x,4,position.z);
      record.stem.geometry.attributes.position.setXYZ(1,position.x,position.y,position.z);
      record.stem.geometry.attributes.position.needsUpdate=true;record.stem.geometry.computeBoundingSphere();
      record.ring.position.set(position.x,4,position.z);record.ring.scale.setScalar(scale*2.8);
      const projected=position.clone().project(this.camera);
      const visible=entity.visible!==false&&this.labelMode!=='none'&&(this.labelMode!=='selected'||isSelected)&&(this.labelNames||this.labelAltitude||this.labelSpeed)&&projected.z>=-1&&projected.z<=1&&Math.abs(projected.x)<1.1&&Math.abs(projected.y)<1.1;
      record.label.hidden=!visible;record.label.classList.toggle('selected',isSelected);
      record.labelName.textContent=this.labelNames?entity.name:'';record.labelName.hidden=!this.labelNames;
      record.labelSpeedValue.textContent=this.labelSpeed?speedLabel(entity,this.speedUnit):'';record.labelSpeedValue.hidden=!this.labelSpeed;
      record.labelAltitudeValue.textContent=this.labelAltitude?altitudeLabel(entity.altitude_m,this.altitudeUnit):'';record.labelAltitudeValue.hidden=!this.labelAltitude;
      record.label.title=[record.labelName.textContent,record.labelSpeedValue.textContent,record.labelAltitudeValue.textContent].filter(Boolean).join(' · ');
      record.label.style.borderColor='#'+colorFor(entity).toString(16);
      const length=MODEL_LENGTHS[entity.kind]||MODEL_LENGTHS.aircraft;
      const labelOffset=Math.max(26,length*scale*rect.height/(4*distance*Math.tan(this.camera.fov*Math.PI/360))+8);
      if(visible){
        const width=record.label.offsetWidth,height=record.label.offsetHeight;
        const x=Math.max(8,Math.min(rect.width-width-8,(projected.x*.5+.5)*rect.width+labelOffset));
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
    this.lastTime=time;this.renderer.render(this.world,this.camera);
    // Perspective scale at the orbit target's depth, in CSS pixels.
    const metersPerPixel=2*this.camera.position.distanceTo(this.controls.target)*Math.tan(this.camera.fov*degrees/2)/(rect.height*this.camera.zoom);
    updateScaleBar(this.distanceScale,metersPerPixel,rect.width,'3d',this.distanceUnit);
  }
}
