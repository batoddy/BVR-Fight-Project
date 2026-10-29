// Cetvel: iz üzerindeki kayıtlı noktaları ölçer. Ara değer hesaplanmaz; imlece en yakın
// kayıtlı örnek seçilir. Aynı izde iki nokta: süre, iz boyunca yol ve düz mesafe.
// Farklı izlerde: yalnızca düz mesafe.
// C tuşu aç/kapa. Cetvel açıkken sol tık nokta koyar; orta tuş kaydırır, Shift + sol tuş döndürür (3D).
import {sampleWindow} from './stream.js';
import {SPEED_UNITS,altitudeLabel,speedLabel,speedValue,flightLabel,distanceLabel} from './units.js';

const SNAP_PIXELS=12;
const SVG='http://www.w3.org/2000/svg';

const clock=seconds=>{
  const minutes=Math.floor(seconds/60),rest=seconds-minutes*60;
  return String(minutes).padStart(2,'0')+':'+rest.toLocaleString('tr-TR',{minimumIntegerDigits:2,minimumFractionDigits:1,maximumFractionDigits:1});
};
const teamColor=sample=>sample.kind==='missile'?'var(--missile)':sample.team==='red'?'var(--red)':'var(--blue)';

export class Ruler{
  constructor(surface,getScene,onChange){
    this.surface=surface;this.getScene=getScene;this.onChange=onChange;
    this.active=false;this.pointer=null;this.hover=null;this.a=null;this.b=null;this.press=null;
    this.layer=document.createElement('div');this.layer.className='ruler-layer';this.layer.hidden=true;
    this.svg=document.createElementNS(SVG,'svg');
    this.line=document.createElementNS(SVG,'line');this.line.setAttribute('class','ruler-line');
    this.marks=['a','b','hover'].map(name=>{const mark=document.createElementNS(SVG,'circle');mark.setAttribute('class','ruler-mark ruler-mark-'+name);mark.setAttribute('r',name==='hover'?4:5);return mark;});
    this.svg.append(this.line,...this.marks);
    this.label=this.createLabel();
    this.result=document.createElement('div');this.result.className='ruler-result';
    this.layer.append(this.svg,this.label.box,this.result);surface.append(this.layer);

    // Yakalama aşamasında dinle: sol tık sahneye (seçim/döndürme) ulaşmadan ölçüme gider.
    surface.addEventListener('pointerdown',event=>{
      if(!this.active||event.target.tagName!=='CANVAS')return;
      if(event.button===1)event.preventDefault();   // orta tuş: tarayıcının otomatik kaydırması açılmasın
      if(event.button===0&&!event.shiftKey){
        event.preventDefault();event.stopPropagation();
        this.press={x:event.clientX,y:event.clientY};
      }
    },true);
    surface.addEventListener('pointerup',event=>{
      if(!this.press||event.button!==0)return;
      event.stopPropagation();
      const moved=Math.hypot(event.clientX-this.press.x,event.clientY-this.press.y);
      this.press=null;
      if(moved<5&&this.hover)this.place(this.hover);
    },true);
    surface.addEventListener('pointermove',event=>{
      const rect=surface.getBoundingClientRect();
      this.pointer={x:event.clientX-rect.left,y:event.clientY-rect.top};
    });
    surface.addEventListener('pointerleave',()=>{this.pointer=null;});
    document.addEventListener('keydown',event=>{
      // C: cetveli aç/kapa. Yazı alanındayken veya Ctrl/Alt/Cmd ile basılınca karışma.
      const typing=event.target instanceof Element&&event.target.closest('input:not([type="range"]):not([type="checkbox"]),select,textarea,[contenteditable]:not([contenteditable="false"])');
      if(event.code==='KeyC'&&!typing&&!event.repeat&&!event.ctrlKey&&!event.altKey&&!event.metaKey){
        event.preventDefault();this.setActive(!this.active);return;
      }
      if(!this.active||event.key!=='Escape')return;
      // Önce ölçümü temizle; ölçüm yoksa cetveli kapat.
      if(this.a)this.clear();else this.setActive(false);
    });
  }

  createLabel(){
    const box=document.createElement('div');box.className='flight-label ruler-label';
    const parts={box};
    for(const name of ['name','speed','altitude','flight']){parts[name]=document.createElement('span');parts[name].className='flight-label-'+name;box.append(parts[name]);}
    return parts;
  }

  setActive(active){
    this.active=active;this.layer.hidden=!active;this.press=null;
    if(!active)this.clear();
    this.getScene().setRulerMode(active);
    this.onChange(active);
  }

  clear(){this.a=null;this.b=null;}

  // 1. tık A, 2. tık B; ölçüm doluysa yeni tık yeni ölçüme başlar.
  place(hit){
    if(!this.a||this.b){this.a=hit;this.b=null;}
    else this.b=hit;
  }

  // Görünen izlerdeki kayıtlı örneklerden imlece en yakın olanı (SNAP_PIXELS içinde).
  nearest(entities,buffer,time,project){
    const scene=this.getScene();
    if(!this.pointer||!scene.trail)return null;
    let best=null,bestDistance=SNAP_PIXELS;
    for(const entity of entities){
      if(entity.visible===false)continue;
      const samples=buffer.tracks.get(entity.id)?.samples;
      if(!samples)continue;
      for(const sample of sampleWindow(samples,time,scene.trailDuration)){
        const p=project(sample);
        if(!p)continue;
        const distance=Math.hypot(p.x-this.pointer.x,p.y-this.pointer.y);
        if(distance<bestDistance){bestDistance=distance;best={id:entity.id,sample};}
      }
    }
    return best;
  }

  draw(entities,buffer,time){
    if(!this.active)return;
    const scene=this.getScene(),project=scene.projector();
    this.hover=this.nearest(entities,buffer,time,project);
    const width=this.surface.clientWidth,height=this.surface.clientHeight;

    const points={a:this.a&&project(this.a.sample),b:this.b&&project(this.b.sample),hover:this.hover&&project(this.hover.sample)};
    this.marks.forEach((mark,index)=>{
      const p=points[['a','b','hover'][index]];
      mark.style.display=p?'':'none';
      if(p){mark.setAttribute('cx',p.x);mark.setAttribute('cy',p.y);}
    });
    const lineEnd=points.b||(this.a&&!this.b?points.hover:null);
    this.line.style.display=points.a&&lineEnd?'':'none';
    if(points.a&&lineEnd){
      this.line.setAttribute('x1',points.a.x);this.line.setAttribute('y1',points.a.y);
      this.line.setAttribute('x2',lineEnd.x);this.line.setAttribute('y2',lineEnd.y);
    }

    // Gezinme etiketi: uçağın kendi etiketiyle aynı satırlar, o örneğin verisiyle.
    const label=this.label;
    label.box.hidden=!points.hover;
    if(points.hover){
      const sample=this.hover.sample,missile=sample.kind==='missile';
      label.name.textContent=sample.name;label.name.hidden=!scene.labelNames;
      label.speed.textContent=speedLabel(sample,scene.speedUnit);label.speed.hidden=!scene.labelSpeed;
      label.altitude.textContent=altitudeLabel(sample.altitude_m,scene.altitudeUnit);label.altitude.hidden=!scene.labelAltitude;
      label.flight.textContent=missile?flightLabel(sample,scene.distanceUnit):clock(sample.sim_time_s);
      label.box.style.borderColor=teamColor(sample);
      const boxWidth=label.box.offsetWidth,boxHeight=label.box.offsetHeight;
      const x=Math.max(8,Math.min(width-boxWidth-8,points.hover.x+14));
      const y=Math.max(8,Math.min(height-boxHeight-8,points.hover.y-boxHeight-10));
      label.box.style.transform=`translate(${x}px,${y}px)`;
    }

    // Sonuç: B konmuşsa A–B, yalnız A varsa A–imleç.
    const end=this.b||(this.a?this.hover:null);
    const anchor=points.b||points.hover;
    this.result.hidden=!(this.a&&end&&anchor);
    if(this.result.hidden)return;
    const a=this.a.sample,b=end.sample;
    const straight=Math.hypot(b.east_m-a.east_m,b.north_m-a.north_m,b.altitude_m-a.altitude_m);
    const rows=[];
    if(this.a.id===end.id){
      rows.push(['Süre',Math.abs(b.sim_time_s-a.sim_time_s).toLocaleString('tr-TR',{minimumFractionDigits:1,maximumFractionDigits:1})+' s']);
      rows.push(['Kat edilen',distanceLabel(Math.abs(b.path_m-a.path_m),scene.distanceUnit,2)]);
    }
    rows.push(['Düz mesafe',distanceLabel(straight,scene.distanceUnit,2)]);
    // Hız farkı = B − A (ikinci seçilen eksi birinci), seçili hız biriminde, işaretli.
    const speedA=speedValue(a,scene.speedUnit),speedB=speedValue(b,scene.speedUnit);
    if(speedA!=null&&speedB!=null){
      const unit=SPEED_UNITS[scene.speedUnit]||SPEED_UNITS.mps;
      const difference=speedB-speedA;
      const text=Math.abs(difference).toLocaleString('tr-TR',{minimumFractionDigits:unit.labelDigits,maximumFractionDigits:unit.labelDigits});
      rows.push(['Hız farkı',(difference<0?'−':'+')+text+' '+unit.symbol]);
    }
    this.result.replaceChildren(...rows.map(([name,value])=>{
      const row=document.createElement('div');const dt=document.createElement('span');const dd=document.createElement('strong');
      dt.textContent=name;dd.textContent=value;row.append(dt,dd);return row;
    }));
    const resultWidth=this.result.offsetWidth;
    const x=Math.max(8,Math.min(width-resultWidth-8,anchor.x+14));
    const y=Math.max(8,Math.min(height-this.result.offsetHeight-8,anchor.y+12));
    this.result.style.transform=`translate(${x}px,${y}px)`;
  }
}
