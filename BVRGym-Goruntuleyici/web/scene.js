import * as THREE from 'three';
import {OrbitControls} from './vendor/OrbitControls.js';
import {createF16,createMissile,disposeObject} from './aircraft3d.js';
import {Scene as Scene2D} from './scene2d.js';

const point=e=>new THREE.Vector3(e.east_m,e.altitude_m,-e.north_m);
const degrees=Math.PI/180;
const colorFor=e=>e.kind==='missile'?(e.team==='red'?0xf18c68:0xecc274):(e.team==='red'?0xe88480:0x83b9d2);

export class Scene {
  constructor(canvas,onSelect,onPan) {
    this.canvas=canvas;this.onSelect=onSelect;this.onPan=onPan;
    this.follow=true;this.grid=true;this.trail=true;this.mode='3d';
    this.models=new Map();this.lastTheme=null;this.focusId=null;this.lastFocus=null;
    this.hasData=false;this.aircraftScale=50;this.lastTrailUpdate=-Infinity;
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
        const hits=this.raycaster.intersectObjects([...this.models.values()].map(r=>r.model),true);
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

  setMode(mode) {
    this.mode=mode;this.canvas.hidden=mode==='2d';this.canvas2d.hidden=mode!=='2d';this.labels.hidden=mode==='2d';
    this.controls.enabled=mode!=='2d';this.resize();
  }

  setView(view) {
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
    this.controls.update();this.lastFocus=null;
    if(this.mode==='2d'){
      this.flat.center={east:center.x,north:-center.z};
      this.flat.scale=Math.min(.045,Math.max(.002,this.flat.width/Math.max(size.x,size.z,7000)/1.8));
    }
  }

  addEntity(entity) {
    const model=entity.kind==='missile'?createMissile(colorFor(entity)):createF16(colorFor(entity));model.userData.entityId=entity.id;this.world.add(model);
    const trail=new THREE.Line(new THREE.BufferGeometry(),new THREE.LineBasicMaterial({color:colorFor(entity),transparent:true,opacity:.78}));
    const shadowTrail=new THREE.Line(new THREE.BufferGeometry(),new THREE.LineBasicMaterial({color:0x6b8c7b,transparent:true,opacity:.4}));
    const stemGeometry=new THREE.BufferGeometry();stemGeometry.setAttribute('position',new THREE.Float32BufferAttribute(new Float32Array(6),3));
    const stem=new THREE.Line(stemGeometry,new THREE.LineBasicMaterial({color:0x7cbaaa,transparent:true,opacity:.6}));
    const ring=new THREE.Mesh(new THREE.RingGeometry(.8,1,48),new THREE.MeshBasicMaterial({color:0x81d6b1,side:THREE.DoubleSide,transparent:true,opacity:.55}));
    ring.rotation.x=-Math.PI/2;ring.position.y=3;
    this.world.add(trail,shadowTrail,stem,ring);
    const label=document.createElement('button');label.className='flight-label';label.onclick=()=>this.onSelect(entity.id);this.labels.append(label);
    const record={model,trail,shadowTrail,stem,ring,label};this.models.set(entity.id,record);
    return record;
  }

  replaceLine(line,points) {
    line.geometry.dispose();line.geometry=new THREE.BufferGeometry().setFromPoints(points);
    if(points.length>1)line.geometry.computeBoundingSphere();
  }

  updateTrail(record,track,entity,time) {
    const samples=track?.samples||[];const visible=samples.filter(s=>s.sim_time_s<=time&&s.sim_time_s>=time-300);
    const stride=Math.max(1,Math.ceil(visible.length/1800));
    const points=[];for(let i=0;i<visible.length;i+=stride)points.push(point(visible[i]));
    points.push(point(entity));this.replaceLine(record.trail,points);
    this.replaceLine(record.shadowTrail,points.map(p=>new THREE.Vector3(p.x,4,p.z)));
  }

  theme() {
    const theme=document.documentElement.dataset.theme;
    if(theme===this.lastTheme)return;
    this.lastTheme=theme;
    const dark=theme==='dark';
    const sky=dark?0x142330:0xb9cdda;
    this.world.background=new THREE.Color(sky);this.world.fog=new THREE.FogExp2(sky,.000013);
    this.ground.material.color.setHex(dark?0x26392f:0x879d82);
    this.groundGrid.material.color.setHex(dark?0x50725f:0x597c68);
  }

  draw(entities,buffer,time,selected) {
    this.flat.follow=this.follow;this.flat.grid=this.grid;this.flat.trail=this.trail;
    if(this.mode==='2d'){this.flat.draw(entities,buffer,time,selected);return;}
    this.theme();
    const ids=new Set(entities.map(e=>e.id));
    for(const [id,record] of this.models)if(!ids.has(id)){
      for(const item of [record.model,record.trail,record.shadowTrail,record.stem,record.ring]){this.world.remove(item);disposeObject(item);}
      record.label.remove();this.models.delete(id);
    }
    const focus=entities.find(e=>e.id===selected);
    if(focus){
      const position=point(focus);
      if(!this.hasData){this.controls.target.copy(position);this.camera.position.copy(position).add(new THREE.Vector3(9000,6500,11000));this.hasData=true;}
      if(this.follow){
        const delta=this.lastFocus&&this.focusId===selected?position.clone().sub(this.lastFocus):position.clone().sub(this.controls.target);
        this.controls.target.add(delta);this.camera.position.add(delta);
      }
      this.lastFocus=position;this.focusId=selected;
    }
    this.controls.update();
    this.groundGrid.visible=this.grid;this.axes.visible=this.grid;
    const now=performance.now(),updateTrails=now-this.lastTrailUpdate>160||time<this.lastTime;
    const rect=this.canvas.getBoundingClientRect();
    const labelRects=[];
    for(const entity of entities){
      let record=this.models.get(entity.id);const fresh=!record;
      if(fresh)record=this.addEntity(entity);
      const position=point(entity);record.model.position.copy(position);
      record.model.rotation.set(entity.pitch_deg*degrees,-entity.heading_deg*degrees,-entity.roll_deg*degrees,'YXZ');
      const distance=this.camera.position.distanceTo(position);
      // Fixed world-space visual scale: zoom changes the apparent size naturally.
      const scale=entity.kind==='missile'?100:this.aircraftScale;record.model.scale.setScalar(scale);
      const isSelected=entity.id===selected;
      record.model.userData.bodyMaterial.color.setHex(colorFor(entity));
      record.trail.visible=this.trail;record.shadowTrail.visible=this.trail&&this.grid;
      record.stem.visible=this.grid&&isSelected;record.ring.visible=this.grid&&isSelected;
      if(updateTrails||fresh){this.updateTrail(record,buffer.tracks.get(entity.id),entity,time);}
      record.stem.geometry.attributes.position.setXYZ(0,position.x,4,position.z);
      record.stem.geometry.attributes.position.setXYZ(1,position.x,position.y,position.z);
      record.stem.geometry.attributes.position.needsUpdate=true;record.stem.geometry.computeBoundingSphere();
      record.ring.position.set(position.x,4,position.z);record.ring.scale.setScalar(scale*2.8);
      const projected=position.clone().project(this.camera);
      const visible=projected.z>=-1&&projected.z<=1&&Math.abs(projected.x)<1.1&&Math.abs(projected.y)<1.1;
      record.label.hidden=!visible;record.label.classList.toggle('selected',isSelected);
      record.label.textContent=entity.name+' · '+Math.round(entity.altitude_m).toLocaleString('tr-TR')+' m';
      record.label.style.borderColor='#'+colorFor(entity).toString(16);
      const length=entity.kind==='missile'?3.65:15.06;
      const labelOffset=Math.max(35,length*scale*rect.height/(4*distance*Math.tan(this.camera.fov*Math.PI/360))+12);
      if(visible){
        const width=record.label.offsetWidth,height=record.label.offsetHeight;
        const x=Math.max(8,Math.min(rect.width-width-8,(projected.x*.5+.5)*rect.width+labelOffset));
        const initialY=Math.max(8,Math.min(rect.height-height-8,(-projected.y*.5+.5)*rect.height-18));
        let y=initialY;
        for(let i=0;i<labelRects.length+1;i++){
          const overlap=labelRects.find(r=>x<r.right+6&&x+width>r.x-6&&y<r.bottom+6&&y+height>r.y-6);
          if(!overlap)break;
          y=overlap.bottom+6;
          if(y+height>rect.height-8)y=Math.max(8,initialY-(i+1)*(height+6));
        }
        labelRects.push({x,y,right:x+width,bottom:y+height});
        record.label.style.transform=`translate(${x}px,${y}px)`;
      }
    }
    if(updateTrails)this.lastTrailUpdate=now;
    this.lastTime=time;this.renderer.render(this.world,this.camera);
  }
}
