import {sampleWindow} from './stream.js';
import {MODEL_LENGTHS,DISTANCE_UNITS,altitudeLabel,headingOffset,distanceScale,speedLabel,visualScale} from './units.js';

export function updateScaleBar(element,metersPerPixel,width,mode,unit='nm'){
  if(!element)return;
  const scale=distanceScale(metersPerPixel,Math.min(140,width-112),unit);
  element.hidden=!scale;
  if(!scale)return;
  const label=scale.value.toLocaleString('tr-TR',{maximumSignificantDigits:3})+' '+scale.symbol;
  element.querySelector('.distance-scale-label').textContent=label;
  element.querySelector('.distance-scale-bar').style.width=scale.pixels+'px';
  element.dataset.metersPerPixel=metersPerPixel;
  element.dataset.meters=scale.meters;
  element.setAttribute('aria-label','Mesafe ölçeği: '+label);
  element.title=mode==='3d'?'Görünüm merkezindeki mesafe ölçeği; perspektifte derinlikle değişir.':'Harita mesafe ölçeği';
}

export class Scene {
  constructor(canvas,onSelect,onPan){
    this.canvas=canvas;this.ctx=canvas.getContext('2d');this.scale=.045;this.center={east:0,north:0};
    this.speedUnit='mps';this.altitudeUnit='ft';this.distanceUnit='nm';
    this.distanceScale=canvas.parentElement.querySelector('.distance-scale');
    this.objectScale=50;this.headingGuides=true;this.follow=true;this.grid=true;this.trail=true;this.onSelect=onSelect;this.onPan=onPan;this.positions=[];this.trailCache=new WeakMap();
    this.trailDuration=300;this.labelMode='all';this.labelNames=true;this.labelAltitude=true;this.labelSpeed=true;
    new ResizeObserver(()=>this.resize()).observe(canvas);
    canvas.addEventListener('wheel',event=>{event.preventDefault();this.zoom(event.deltaY<0?1.15:1/1.15);},{passive:false});
    let drag=null;
    canvas.addEventListener('pointerdown',event=>{canvas.setPointerCapture(event.pointerId);drag={x:event.clientX,y:event.clientY,east:this.center.east,north:this.center.north,moved:false};});
    canvas.addEventListener('pointermove',event=>{if(!drag)return;const dx=event.clientX-drag.x,dy=event.clientY-drag.y;if(Math.hypot(dx,dy)>4)drag.moved=true;if(drag.moved){this.follow=false;this.center.east=drag.east-dx/this.scale;this.center.north=drag.north+dy/this.scale;this.onPan();}});
    canvas.addEventListener('pointerup',event=>{if(drag&&!drag.moved){const rect=canvas.getBoundingClientRect();const x=event.clientX-rect.left,y=event.clientY-rect.top;const found=this.positions.find(p=>Math.hypot(p.x-x,p.y-y)<30);if(found)this.onSelect(found.id);}drag=null;});
    canvas.addEventListener('dblclick',event=>{const rect=canvas.getBoundingClientRect(),x=event.clientX-rect.left,y=event.clientY-rect.top;const found=this.positions.find(p=>p.label&&x>=p.label.x&&x<=p.label.right&&y>=p.label.y&&y<=p.label.bottom)||this.positions.find(p=>Math.hypot(p.x-x,p.y-y)<30);if(found)this.onFocus?.(found.id);});
    canvas.addEventListener('pointercancel',()=>{drag=null;});
    this.resize();
  }
  resize(){this.width=this.canvas.clientWidth;this.height=this.canvas.clientHeight;const ratio=devicePixelRatio||1;this.canvas.width=Math.round(this.width*ratio);this.canvas.height=Math.round(this.height*ratio);this.ctx.setTransform(ratio,0,0,ratio,0,0);}
  zoom(factor){this.scale=Math.min(.8,Math.max(.002,this.scale*factor));}
  frameAll(entities){
    if(!entities.length)return;
    const east=entities.map(e=>e.east_m),north=entities.map(e=>e.north_m);
    this.center={east:(Math.min(...east)+Math.max(...east))/2,north:(Math.min(...north)+Math.max(...north))/2};
    this.scale=Math.min(.045,Math.max(.002,Math.min(this.width/Math.max(7000,Math.max(...east)-Math.min(...east)),this.height/Math.max(7000,Math.max(...north)-Math.min(...north)))/1.8));
  }
  focusEntity(entity){this.follow=true;this.center={east:entity.east_m,north:entity.north_m};this.scale=Math.min(.8,Math.max(.002,70/((MODEL_LENGTHS[entity.kind]||MODEL_LENGTHS.aircraft)*visualScale(entity.kind,this.objectScale))));}
  clear(){this.positions=[];this.trailCache=new WeakMap();}
  point(entity){return {x:this.width/2+(entity.east_m-this.center.east)*this.scale,y:this.height/2-(entity.north_m-this.center.north)*this.scale};}
  aircraft(x,y,heading,color){
    const ctx=this.ctx;ctx.save();ctx.translate(x,y);ctx.rotate(heading*Math.PI/180);
    // Same fixed 50x visual length as 3D; map zoom changes the screen size.
    ctx.scale(this.scale*MODEL_LENGTHS.aircraft*this.objectScale/63,this.scale*MODEL_LENGTHS.aircraft*this.objectScale/63);
    // F-16: pointed nose, wing-root extensions, swept wings and rear stabilizers.
    const points=[[0,-32],[2,-25],[3,-17],[3.5,-10],[6,-2],[6,1],[22,10],[22,17],[7,12],[5,19],[13,25],[12,29],[4,25],[3,31],[-3,31],[-4,25],[-12,29],[-13,25],[-5,19],[-7,12],[-22,17],[-22,10],[-6,1],[-6,-2],[-3.5,-10],[-3,-17],[-2,-25]];
    ctx.beginPath();points.forEach(([px,py],i)=>i?ctx.lineTo(px,py):ctx.moveTo(px,py));ctx.closePath();ctx.fillStyle=color;ctx.fill();ctx.strokeStyle=this.colors.map;ctx.lineWidth=1.4;ctx.stroke();
    ctx.fillStyle=this.colors.map;ctx.beginPath();ctx.ellipse(0,-14,1.8,5.5,0,0,Math.PI*2);ctx.fill();
    ctx.beginPath();ctx.moveTo(0,12);ctx.lineTo(0,27);ctx.moveTo(-2.5,28);ctx.lineTo(2.5,28);ctx.strokeStyle=this.colors.map;ctx.lineWidth=1.2;ctx.stroke();ctx.restore();
  }
  missile(x,y,heading,color){
    const ctx=this.ctx;ctx.save();ctx.translate(x,y);ctx.rotate(heading*Math.PI/180);
    ctx.scale(this.scale*MODEL_LENGTHS.missile*visualScale('missile',this.objectScale)/41,this.scale*MODEL_LENGTHS.missile*visualScale('missile',this.objectScale)/41);
    ctx.beginPath();const points=[[0,-23],[3,-14],[3,5],[9,15],[9,18],[3,14],[3,17],[-3,17],[-3,14],[-9,18],[-9,15],[-3,5],[-3,-14]];
    points.forEach(([x,y],i)=>i?ctx.lineTo(x,y):ctx.moveTo(x,y));ctx.closePath();ctx.fillStyle=color;ctx.fill();ctx.restore();
  }
  draw(entities,buffer,time,selected){
    const ctx=this.ctx;if(!this.width||!this.height)return;
    const style=getComputedStyle(this.canvas);this.colors={map:style.getPropertyValue('--map').trim(),grid:style.getPropertyValue('--grid').trim(),text:style.getPropertyValue('--text').trim(),muted:style.getPropertyValue('--muted').trim(),blue:style.getPropertyValue('--blue').trim(),red:style.getPropertyValue('--red').trim(),missile:style.getPropertyValue('--missile').trim()};
    const focus=entities.find(e=>e.id===selected&&e.visible!==false);if(this.follow&&focus)this.center={east:focus.east_m,north:focus.north_m};
    ctx.clearRect(0,0,this.width,this.height);
    let step=1/(DISTANCE_UNITS[this.distanceUnit]||DISTANCE_UNITS.nm).factor;while(step*this.scale<45)step*=2;while(step*this.scale>150)step/=2;
    if(this.grid){const pixels=step*this.scale;const origin=this.point({east_m:0,north_m:0});const x0=((origin.x%pixels)+pixels)%pixels,y0=((origin.y%pixels)+pixels)%pixels;ctx.strokeStyle=this.colors.grid;ctx.lineWidth=1;ctx.beginPath();for(let x=x0;x<this.width;x+=pixels){ctx.moveTo(x,0);ctx.lineTo(x,this.height);}for(let y=y0;y<this.height;y+=pixels){ctx.moveTo(0,y);ctx.lineTo(this.width,y);}ctx.stroke();}
    if(this.trail){for(const entity of entities){
      const track=buffer.tracks.get(entity.id);if(!track)continue;
      const now=performance.now();let cached=this.trailCache.get(track);
      if(!cached||now-cached.wall>160||time<cached.time||cached.duration!==this.trailDuration){
        cached={wall:now,time,duration:this.trailDuration,samples:sampleWindow(track.samples,time,this.trailDuration,1500)};this.trailCache.set(track,cached);
      }
      const samples=cached.samples;
      const stride=Math.max(1,Math.ceil(samples.length/1500));ctx.beginPath();let count=0;
      for(let i=0;i<samples.length;i+=stride){const p=this.point(samples[i]);count?ctx.lineTo(p.x,p.y):ctx.moveTo(p.x,p.y);count++;}
      if(count){const p=this.point(entity);ctx.lineTo(p.x,p.y);}
      ctx.strokeStyle=entity.team==='unknown'?this.colors.muted:entity.team==='red'?this.colors.red:entity.kind==='missile'?this.colors.missile:this.colors.blue;ctx.globalAlpha=.7;ctx.lineWidth=2;ctx.stroke();ctx.globalAlpha=1;
    }}
    this.positions=[];const labelRects=[];
    for(const entity of entities){if(entity.visible===false)continue;const p=this.point(entity);this.positions.push({...p,id:entity.id});if(p.x< -70||p.x>this.width+70||p.y< -70||p.y>this.height+70)continue;
      const color=entity.team==='unknown'?(document.documentElement.dataset.theme==='dark'?'#aeb6bf':'#46515c'):entity.kind==='missile'?(entity.team==='red'?this.colors.red:this.colors.missile):(entity.team==='red'?this.colors.red:this.colors.blue);
      ctx.globalAlpha=entity.measurement_ended?.6:1;
      if(entity.visual_model==='generic'){ctx.fillStyle=color;ctx.beginPath();ctx.arc(p.x,p.y,Math.max(3,Math.min(25,this.scale*MODEL_LENGTHS[entity.kind]*visualScale(entity.kind,this.objectScale)/2)),0,Math.PI*2);ctx.fill();}
      else if(entity.kind==='aircraft')this.aircraft(p.x,p.y,entity.heading_deg,color);
      else if(entity.kind==='missile')this.missile(p.x,p.y,entity.heading_deg,color);
      const visualLength=(MODEL_LENGTHS[entity.kind]||MODEL_LENGTHS.aircraft)*visualScale(entity.kind,this.objectScale);
      if(this.headingGuides&&entity.heading_deg_known!==false){
        const projection=Math.cos((entity.pitch_deg??0)*Math.PI/180);
        const start=headingOffset(entity.heading_deg,visualLength*.55*projection);
        const end=headingOffset(entity.heading_deg,(visualLength*.55+Math.max(250,visualLength*.9))*projection);
        ctx.save();ctx.strokeStyle=color;ctx.lineWidth=1.4;ctx.setLineDash([5,4]);ctx.beginPath();
        ctx.moveTo(p.x+start.east*this.scale,p.y-start.north*this.scale);
        ctx.lineTo(p.x+end.east*this.scale,p.y-end.north*this.scale);ctx.stroke();ctx.restore();
      }
      if(this.labelMode!=='none'&&(this.labelMode!=='selected'||entity.id===selected)){
        const name=this.labelNames?entity.name:'',speed=this.labelSpeed?speedLabel(entity,this.speedUnit):'',altitude=this.labelAltitude?altitudeLabel(entity.altitude_m,this.altitudeUnit):'';
        if(!name&&!speed&&!altitude)continue;
        ctx.font='600 12px "Segoe UI",sans-serif';const nameWidth=Math.min(90,ctx.measureText(name).width);
        ctx.font='12px "Segoe UI",sans-serif';const speedWidth=ctx.measureText(speed).width,altitudeWidth=ctx.measureText(altitude).width;
        const topHeight=name||speed?17:0,labelHeight=topHeight+(altitude?15:0)+6;
        const labelWidth=Math.min(this.width-16,Math.max(nameWidth+speedWidth+(name&&speed?10:0),altitudeWidth)+12);
        const labelX=Math.max(8,Math.min(this.width-labelWidth-8,p.x+Math.max(26,this.scale*visualLength/2+8)));
        const initialY=Math.max(8,Math.min(this.height-labelHeight-8,p.y-labelHeight/2));let labelY=initialY;
        for(let i=0;i<labelRects.length+1;i++){
          const overlap=labelRects.find(r=>labelX<r.right+3&&labelX+labelWidth>r.x-3&&labelY<r.bottom+3&&labelY+labelHeight>r.y-3);
          if(!overlap)break;
          labelY=overlap.bottom+3;
          if(labelY+labelHeight>this.height-8)labelY=Math.max(8,initialY-(i+1)*(labelHeight+3));
        }
        const labelRect={x:labelX,y:labelY,right:labelX+labelWidth,bottom:labelY+labelHeight};labelRects.push(labelRect);this.positions.at(-1).label=labelRect;
        ctx.save();ctx.fillStyle=style.getPropertyValue('--panel').trim();ctx.strokeStyle=color;ctx.lineWidth=1;
        ctx.beginPath();ctx.roundRect(labelX,labelY,labelWidth,labelHeight,3);ctx.fill();ctx.stroke();
        ctx.fillStyle=this.colors.text;
        if(name){ctx.font='600 12px "Segoe UI",sans-serif';ctx.fillText(name,labelX+6,labelY+15,Math.max(1,labelWidth-12-speedWidth-(speed?10:0)));}
        ctx.font='12px "Segoe UI",sans-serif';
        if(speed){ctx.textAlign=name?'right':'center';ctx.fillText(speed,name?labelX+labelWidth-6:labelX+labelWidth/2,labelY+15);}
        if(altitude){ctx.textAlign='center';ctx.fillText(altitude,labelX+labelWidth/2,labelY+topHeight+13);}
        ctx.restore();
      }
    }
    ctx.globalAlpha=1;
    updateScaleBar(this.distanceScale,1/this.scale,this.width,'2d',this.distanceUnit);
    ctx.font='14px "Segoe UI",sans-serif';ctx.fillStyle=this.colors.muted;
    if(!entities.length){ctx.fillStyle=this.colors.muted;ctx.textAlign='center';ctx.fillText('Uçuş verisi bekleniyor',this.width/2,this.height/2);ctx.textAlign='left';}
  }
}
