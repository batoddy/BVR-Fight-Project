import {sampleWindow} from './stream.js';
import {Basemap} from './basemap.js';
import {MODEL_LENGTHS,ENEMY_ZONES,radarColor,DISTANCE_UNITS,altitudeLabel,distanceScale,speedLabel,flightLabel,analysisLabel,visualLength,velocityVector,VELOCITY_PIXELS_PER_MPS} from './units.js';

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

// Uzak modeller yönü okunabilen simgelere dönüşür; dünya koordinatları değişmez.
const AIRCRAFT_OUTLINE='M0 -8 Q.2 -7.5 .3 -6.3 L.44 -4.6 .58 -3 1.15 -.7 .8 -.7 4.98 2.45 4.98 3.85 .82 2.6 .55 3.65 2.6 5.4 2.45 6.75 .43 5.8 .38 7.06 -.38 7.06 -.43 5.8 -2.45 6.75 -2.6 5.4 -.55 3.65 -.82 2.6 -4.98 3.85 -4.98 2.45 -.8 -.7 -1.15 -.7 -.58 -3 -.44 -4.6 -.3 -6.3 Q-.2 -7.5 0 -8 Z';

export class Scene {
  constructor(canvas,onSelect,onPan){
    this.canvas=canvas;this.ctx=canvas.getContext('2d');this.scale=.045;this.center={east:0,north:0};
    this.basemap=new Basemap();this.mapEnabled=false;
    this.speedUnit='mps';this.altitudeUnit='ft';this.distanceUnit='nm';
    this.distanceScale=canvas.parentElement.querySelector('.distance-scale');
    this.objectScale=1;this.velocityGuides=true;this.follow=true;this.grid=true;this.trail=true;this.onSelect=onSelect;this.onPan=onPan;this.positions=[];this.trailCache=new WeakMap();this.analysis=null;
    this.trailDuration=300;this.labelMode='all';this.labelNames=true;this.labelAltitude=true;this.labelSpeed=true;
    this.aircraftOutline=new Path2D(AIRCRAFT_OUTLINE);
    new ResizeObserver(()=>this.resize()).observe(canvas);
    canvas.addEventListener('wheel',event=>{event.preventDefault();this.zoom(event.deltaY<0?1.15:1/1.15);},{passive:false});
    let drag=null;
    // Cetvel açıkken sol tık ölçüme gider; kaydırma orta tuşla yapılır.
    canvas.addEventListener('pointerdown',event=>{if(event.button!==(this.ruler?1:0))return;if(this.ruler)event.preventDefault();canvas.setPointerCapture(event.pointerId);drag={x:event.clientX,y:event.clientY,east:this.center.east,north:this.center.north,moved:false};});
    canvas.addEventListener('pointermove',event=>{if(!drag)return;const dx=event.clientX-drag.x,dy=event.clientY-drag.y;if(Math.hypot(dx,dy)>4)drag.moved=true;if(drag.moved){this.follow=false;this.center.east=drag.east-dx/this.scale;this.center.north=drag.north+dy/this.scale;this.onPan();}});
    canvas.addEventListener('pointerup',event=>{if(drag&&!drag.moved&&!this.ruler){const found=this.hitTest(event);if(found)this.onSelect(found.id);}drag=null;});
    canvas.addEventListener('dblclick',event=>{const found=this.hitTest(event);if(found)this.onFocus?.(found.id);});
    canvas.addEventListener('pointercancel',()=>{drag=null;});
    this.resize();
  }
  hitTest(event){
    const rect=this.canvas.getBoundingClientRect(),x=event.clientX-rect.left,y=event.clientY-rect.top;
    // Etiketler modellerin üstüne çizilir; üstteki etiket tıklamayı önce alır.
    const label=[...this.positions].reverse().find(p=>p.label&&x>=p.label.x&&x<=p.label.right&&y>=p.label.y&&y<=p.label.bottom);
    if(label)return label;
    let nearest=null,best=Infinity;
    for(const p of this.positions){const distance=Math.hypot(p.x-x,p.y-y);if(distance<Math.max(16,p.radius||0)&&distance<best){nearest=p;best=distance;}}
    return nearest;
  }
  resize(){this.width=this.canvas.clientWidth;this.height=this.canvas.clientHeight;const ratio=devicePixelRatio||1;this.canvas.width=Math.round(this.width*ratio);this.canvas.height=Math.round(this.height*ratio);this.ctx.setTransform(ratio,0,0,ratio,0,0);}
  // Ölçek px/m. En yakında model ekranda ~450 px: 1× gerçek boyda 30 px/m.
  get maxScale(){return 450/visualLength('aircraft',this.objectScale);}
  zoom(factor){this.scale=Math.min(this.maxScale,Math.max(.002,this.scale*factor));}
  frameAll(entities){
    if(!entities.length)return;
    const east=entities.map(e=>e.east_m),north=entities.map(e=>e.north_m);
    this.center={east:(Math.min(...east)+Math.max(...east))/2,north:(Math.min(...north)+Math.max(...north))/2};
    this.scale=Math.min(.045,Math.max(.002,Math.min(this.width/Math.max(7000,Math.max(...east)-Math.min(...east)),this.height/Math.max(7000,Math.max(...north)-Math.min(...north)))/1.8));
  }
  focusEntity(entity){this.follow=true;this.center={east:entity.east_m,north:entity.north_m};this.scale=Math.min(this.maxScale,Math.max(.002,120/visualLength(entity.kind,this.objectScale)));}
  clear(){this.positions=[];this.trailCache=new WeakMap();}
  setRulerMode(active){this.ruler=active;}
  projector(){return sample=>this.point(sample);}
  point(entity){return {x:this.width/2+(entity.east_m-this.center.east)*this.scale,y:this.height/2-(entity.north_m-this.center.north)*this.scale};}
  aircraft(x,y,heading,color,pixels){
    const ctx=this.ctx;ctx.save();ctx.translate(x,y);ctx.rotate(heading*Math.PI/180);
    const scale=pixels/MODEL_LENGTHS.aircraft;ctx.scale(scale,scale);
    ctx.fillStyle=color;ctx.strokeStyle=this.colors.map;ctx.lineWidth=1.5/scale;ctx.lineJoin='round';
    ctx.stroke(this.aircraftOutline);ctx.fill(this.aircraftOutline);
    // İnce gövde, kanat kökleri, damla kanopi ve tek motor ağzı.
    ctx.save();ctx.globalAlpha*=.2;ctx.fillStyle=this.colors.text;
    ctx.beginPath();ctx.moveTo(0,-7.2);ctx.bezierCurveTo(.8,-3.8,.85,2.7,.37,6.8);ctx.lineTo(-.37,6.8);ctx.bezierCurveTo(-.85,2.7,-.8,-3.8,0,-7.2);ctx.fill();ctx.restore();
    ctx.fillStyle=this.colors.map;ctx.beginPath();ctx.ellipse(0,-3.55,.36,1.13,0,0,Math.PI*2);ctx.fill();
    if(pixels>38){
      ctx.save();ctx.globalAlpha*=.45;ctx.strokeStyle=this.colors.map;ctx.lineWidth=.75/scale;
      ctx.beginPath();for(const side of [-1,1]){ctx.moveTo(side*.85,1.55);ctx.lineTo(side*4.7,3.15);ctx.moveTo(side*.7,4.65);ctx.lineTo(side*2.35,6.15);}ctx.moveTo(0,1.6);ctx.lineTo(0,5.9);ctx.stroke();ctx.restore();
      ctx.fillRect(-.3,6.65,.6,.4);
    }
    ctx.restore();
  }
  missile(x,y,heading,color,pixels){
    const ctx=this.ctx;ctx.save();ctx.translate(x,y);ctx.rotate(heading*Math.PI/180);
    ctx.scale(pixels/41,pixels/41);
    ctx.beginPath();const points=[[0,-23],[3,-14],[3,5],[9,15],[9,18],[3,14],[3,17],[-3,17],[-3,14],[-9,18],[-9,15],[-3,5],[-3,-14]];
    points.forEach(([x,y],i)=>i?ctx.lineTo(x,y):ctx.moveTo(x,y));ctx.closePath();ctx.fillStyle=color;ctx.fill();ctx.restore();
  }
  draw(entities,buffer,time,selected){
    const ctx=this.ctx;if(!this.width||!this.height)return;
    const style=getComputedStyle(this.canvas);this.colors={map:style.getPropertyValue('--map').trim(),grid:style.getPropertyValue('--grid').trim(),text:style.getPropertyValue('--text').trim(),muted:style.getPropertyValue('--muted').trim(),blue:style.getPropertyValue('--blue').trim(),red:style.getPropertyValue('--red').trim(),missile:style.getPropertyValue('--missile').trim()};
    const focus=entities.find(e=>e.id===selected&&e.visible!==false);if(this.follow&&focus)this.center={east:focus.east_m,north:focus.north_m};
    ctx.clearRect(0,0,this.width,this.height);
    this.basemap.enabled=this.mapEnabled;this.basemap.setOrigin(buffer.packet.map_origin);
    this.basemap.update(this.center.east,this.center.north,this.width/this.scale,this.height/this.scale,this.width);
    this.basemap.draw2D(ctx,entity=>this.point(entity),this.scale,document.documentElement.dataset.theme==='dark');
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
    const analysis=this.analysis,accent=style.getPropertyValue('--accent').trim();
    if(analysis?.options.radar&&analysis.entity.heading_deg_known!==false&&analysis.calc.radar_gimbal_deg>0&&analysis.calc.radar_range_m>0){
      // Radar açısı: yarım açı, burundan sağa ve sola (60 = ±60), radar menzili uzunluğunda; 3D ile aynı şekil.
      const {entity,calc}=analysis,center=this.point(entity),length=calc.radar_range_m*this.scale,gimbal=calc.radar_gimbal_deg;
      const at=angle=>{const rad=(entity.heading_deg+angle)*Math.PI/180;return [center.x+Math.sin(rad)*length,center.y-Math.cos(rad)*length];};
      ctx.save();ctx.strokeStyle=radarColor(entity.team,document.documentElement.dataset.theme==='dark');ctx.globalAlpha=.75;ctx.lineWidth=1.4;ctx.setLineDash([4,3]);
      ctx.beginPath();ctx.moveTo(...at(-gimbal));ctx.lineTo(center.x,center.y);ctx.lineTo(...at(gimbal));
      for(let i=1;i<=64;i++)ctx.lineTo(...at(gimbal-2*gimbal*i/64));
      ctx.stroke();ctx.restore();
    }
    // WEZ: uçağın etrafında min–max halkası; karşı uçak içindeyse belirginleşir. 3D ile aynı.
    const wezRing=(entity,calc)=>{
      const minM=calc?.wez_min_range_m,maxM=calc?.wez_max_range_m;
      if(!(maxM>minM&&maxM>0))return;
      const p=this.point(entity),color=entity.team==='red'?this.colors.red:this.colors.blue,active=Boolean(calc.enemy_in_my_wez);
      ctx.save();ctx.beginPath();ctx.arc(p.x,p.y,maxM*this.scale,0,Math.PI*2);ctx.arc(p.x,p.y,Math.max(0,minM)*this.scale,0,Math.PI*2,true);
      ctx.fillStyle=color;ctx.globalAlpha=active?.2:.07;ctx.fill('evenodd');
      ctx.globalAlpha=active?.95:.55;ctx.strokeStyle=color;ctx.lineWidth=1.2;
      ctx.beginPath();ctx.arc(p.x,p.y,maxM*this.scale,0,Math.PI*2);ctx.stroke();
      if(minM>0){ctx.beginPath();ctx.arc(p.x,p.y,minM*this.scale,0,Math.PI*2);ctx.stroke();}
      ctx.restore();
    };
    if(analysis?.options.ownWez)wezRing(analysis.entity,analysis.calc);
    if(analysis?.options.enemyWez&&analysis.target)wezRing(analysis.target,analysis.target.calc);
    // Düşman merkezli bölgeler: yarıçap seçili uçağın hesabı, çember hedefin etrafında; içindeyse belirgin. 3D ile aynı.
    if(analysis?.target){
      const center=this.point(analysis.target),dark=document.documentElement.dataset.theme==='dark';
      for(const zone of ENEMY_ZONES){
        const radius=analysis.calc[zone.field];
        if(!analysis.options.zones[zone.key]||!(radius>0))continue;
        const active=analysis.calc.range_3d_m<radius,pixels=radius*this.scale,color=dark?zone.dark:zone.light;
        ctx.save();ctx.strokeStyle=ctx.fillStyle=color;ctx.globalAlpha=active?1:.6;ctx.lineWidth=active?2:1.2;
        ctx.beginPath();ctx.arc(center.x,center.y,pixels,0,Math.PI*2);
        if(zone.style==='hatch'){
          // Çapraz tarama: daireye kırpılmış 45° çizgiler, ekranda 9 px aralık; en fazla ~400 çizgi.
          ctx.save();ctx.clip();ctx.globalAlpha=active?.6:.3;ctx.lineWidth=1;
          const spacing=Math.max(9,4*pixels/400);
          ctx.beginPath();
          for(let k=-2*pixels;k<=2*pixels;k+=spacing){ctx.moveTo(center.x+k-pixels,center.y+pixels);ctx.lineTo(center.x+k+pixels,center.y-pixels);}
          ctx.stroke();ctx.restore();
          ctx.beginPath();ctx.arc(center.x,center.y,pixels,0,Math.PI*2);
        }
        // Kesikli: radarın kısa kesiklerinden (4,3) belirgin biçimde uzun.
        if(zone.style==='dash')ctx.setLineDash([12,7]);
        ctx.stroke();ctx.setLineDash([]);
        ctx.font='600 11px "Segoe UI",sans-serif';ctx.textAlign='center';ctx.textBaseline='bottom';
        ctx.fillText(zone.label,center.x,center.y-pixels-3);
        ctx.restore();
      }
    }
    const showLine=Boolean(analysis?.options.line&&analysis.target);
    if(showLine){
      const from=this.point(analysis.entity),to=this.point(analysis.target);
      ctx.save();ctx.strokeStyle=accent;ctx.lineWidth=1.6;ctx.beginPath();ctx.moveTo(from.x,from.y);ctx.lineTo(to.x,to.y);ctx.stroke();ctx.restore();
    }
    this.positions=[];const labelRects=[];
    for(const entity of entities){if(entity.visible===false)continue;const p=this.point(entity);
      const color=entity.team==='unknown'?(document.documentElement.dataset.theme==='dark'?'#aeb6bf':'#46515c'):entity.kind==='missile'?(entity.team==='red'?this.colors.red:this.colors.missile):(entity.team==='red'?this.colors.red:this.colors.blue);
      ctx.globalAlpha=entity.measurement_ended?.6:1;
      const modelLength=visualLength(entity.kind,this.objectScale);
      const pixels=Math.max(modelLength*this.scale,(entity.kind==='missile'?9:20)*this.objectScale);
      if(p.x< -pixels||p.x>this.width+pixels||p.y< -pixels||p.y>this.height+pixels)continue;
      this.positions.push({...p,id:entity.id,radius:pixels/2+4});
      if(entity.visual_model==='generic'){
        ctx.fillStyle=color;ctx.beginPath();ctx.arc(p.x,p.y,Math.max(entity.kind==='missile'?2.5:3.5,Math.min(25,this.scale*modelLength/2)),0,Math.PI*2);ctx.fill();
      }
      else if(entity.kind==='aircraft')this.aircraft(p.x,p.y,entity.heading_deg,color,pixels);
      else if(entity.kind==='missile')this.missile(p.x,p.y,entity.heading_deg,color,pixels);
      const velocity=this.velocityGuides?velocityVector(entity):null;
      if(velocity&&Math.hypot(velocity.east,velocity.north)>1e-3){
        const speed=Math.hypot(velocity.east,velocity.north),dx=velocity.east/speed,dy=-velocity.north/speed;
        const start={x:p.x+dx*pixels*.55,y:p.y+dy*pixels*.55};
        const length=speed*VELOCITY_PIXELS_PER_MPS,head=Math.min(10,length*.25);
        const end={x:start.x+velocity.east*VELOCITY_PIXELS_PER_MPS,y:start.y-velocity.north*VELOCITY_PIXELS_PER_MPS};
        ctx.save();ctx.strokeStyle=color;ctx.fillStyle=color;ctx.lineWidth=1.6;ctx.beginPath();ctx.moveTo(start.x,start.y);ctx.lineTo(end.x,end.y);ctx.stroke();
        ctx.beginPath();ctx.moveTo(end.x,end.y);ctx.lineTo(end.x-dx*head-dy*head*.45,end.y-dy*head+dx*head*.45);
        ctx.lineTo(end.x-dx*head+dy*head*.45,end.y-dy*head-dx*head*.45);ctx.closePath();ctx.fill();ctx.restore();
      }
      if(this.labelMode!=='none'&&(this.labelMode!=='selected'||entity.id===selected)){
        const name=this.labelNames?entity.name:'',speed=this.labelSpeed?speedLabel(entity,this.speedUnit):'',altitude=this.labelAltitude?altitudeLabel(entity.altitude_m,this.altitudeUnit):'';
        const flight=entity.kind==='missile'?flightLabel(entity,this.distanceUnit):'';
        if(!name&&!speed&&!altitude)continue;
        ctx.font='600 12px "Segoe UI",sans-serif';const nameWidth=Math.min(90,ctx.measureText(name).width);
        ctx.font='12px "Segoe UI",sans-serif';const speedWidth=ctx.measureText(speed).width,altitudeWidth=ctx.measureText(altitude).width,flightWidth=ctx.measureText(flight).width;
        const topHeight=name||speed?17:0,altitudeHeight=altitude?15:0,labelHeight=topHeight+altitudeHeight+(flight?15:0)+6;
        const labelWidth=Math.min(this.width-16,Math.max(nameWidth+speedWidth+(name&&speed?10:0),altitudeWidth,flightWidth)+12);
        const labelOffset=Math.max(26,pixels/2+8);
        const arrowPointsRight=velocity&&velocity.east>.25*Math.hypot(velocity.east,velocity.north);
        const labelX=Math.max(8,Math.min(this.width-labelWidth-8,p.x+(arrowPointsRight?-labelWidth-labelOffset:labelOffset)));
        const initialY=Math.max(8,Math.min(this.height-labelHeight-8,p.y-labelHeight/2));let labelY=initialY;
        for(let i=0;i<labelRects.length+1;i++){
          const overlap=labelRects.find(r=>labelX<r.right+3&&labelX+labelWidth>r.x-3&&labelY<r.bottom+3&&labelY+labelHeight>r.y-3);
          if(!overlap)break;
          labelY=overlap.bottom+3;
          if(labelY+labelHeight>this.height-8)labelY=Math.max(8,initialY-(i+1)*(labelHeight+3));
        }
        const labelRect={x:labelX,y:labelY,right:labelX+labelWidth,bottom:labelY+labelHeight};labelRects.push(labelRect);this.positions.at(-1).label=labelRect;
        ctx.save();ctx.fillStyle=style.getPropertyValue('--panel').trim();ctx.strokeStyle=color;ctx.lineWidth=entity.id===selected?2:1;
        ctx.beginPath();ctx.roundRect(labelX,labelY,labelWidth,labelHeight,3);ctx.fill();ctx.stroke();
        ctx.fillStyle=this.colors.text;
        if(name){ctx.font='600 12px "Segoe UI",sans-serif';ctx.fillText(name,labelX+6,labelY+15,Math.max(1,labelWidth-12-speedWidth-(speed?10:0)));}
        ctx.font='12px "Segoe UI",sans-serif';
        if(speed){ctx.textAlign=name?'right':'center';ctx.fillText(speed,name?labelX+labelWidth-6:labelX+labelWidth/2,labelY+15);}
        if(altitude){ctx.textAlign='center';ctx.fillText(altitude,labelX+labelWidth/2,labelY+topHeight+13);}
        if(flight){ctx.textAlign='center';ctx.fillText(flight,labelX+labelWidth/2,labelY+topHeight+altitudeHeight+13);}
        ctx.restore();
      }
    }
    ctx.globalAlpha=1;
    const parts=showLine?analysisLabel(analysis.calc,analysis.options,this.distanceUnit):[];
    if(parts.length){
      // Hedef çizgisinin ortasında, çizginin üstünde.
      // Ortası ekran dışındaysa çizgi boyunca seçili uçağa doğru, görünen en uzak noktaya kayar.
      const from=this.point(analysis.entity),to=this.point(analysis.target),text=parts.join(' · ');
      const fractions=Array.from({length:18},(_,i)=>.5/2**i);
      const anchor=fractions.map(f=>({x:from.x+(to.x-from.x)*f,y:from.y+(to.y-from.y)*f})).find(q=>q.x>40&&q.x<this.width-40&&q.y>40&&q.y<this.height-40);
      ctx.save();ctx.font='12px "Segoe UI",sans-serif';
      // Değer değiştikçe genişlik oynar, ortalanan etiket titrer: genişlik yalnız büyür (3D ile aynı).
      const key=parts.map(p=>p.split(' ')[0]).join();
      if(this.analysisKey!==key){this.analysisKey=key;this.analysisWidth=0;}
      this.analysisWidth=Math.max(this.analysisWidth,Math.ceil(ctx.measureText(text).width+14));
      const width=this.analysisWidth,height=20,at=anchor||from;
      const x=Math.round(Math.max(8,Math.min(this.width-width-8,at.x-width/2))),y=Math.round(Math.max(8,Math.min(this.height-height-8,at.y-height-6)));
      ctx.fillStyle=style.getPropertyValue('--panel').trim();ctx.strokeStyle=accent;ctx.lineWidth=1;
      ctx.beginPath();ctx.roundRect(x,y,width,height,3);ctx.fill();ctx.stroke();
      ctx.fillStyle=this.colors.text;ctx.textBaseline='middle';ctx.fillText(text,x+7,y+height/2+.5);ctx.restore();
    }
    updateScaleBar(this.distanceScale,1/this.scale,this.width,'2d',this.distanceUnit);
    ctx.font='14px "Segoe UI",sans-serif';ctx.fillStyle=this.colors.muted;
    if(!entities.length){ctx.fillStyle=this.colors.muted;ctx.textAlign='center';ctx.fillText('Uçuş verisi bekleniyor',this.width/2,this.height/2);ctx.textAlign='left';}
  }
}
