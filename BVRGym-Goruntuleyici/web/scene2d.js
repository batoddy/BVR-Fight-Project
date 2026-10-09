export class Scene {
  constructor(canvas,onSelect,onPan){
    this.canvas=canvas;this.ctx=canvas.getContext('2d');this.scale=.045;this.center={east:0,north:0};
    this.follow=true;this.grid=true;this.trail=true;this.onSelect=onSelect;this.onPan=onPan;this.positions=[];
    new ResizeObserver(()=>this.resize()).observe(canvas);
    canvas.addEventListener('wheel',event=>{event.preventDefault();this.zoom(event.deltaY<0?1.15:1/1.15);},{passive:false});
    let drag=null;
    canvas.addEventListener('pointerdown',event=>{canvas.setPointerCapture(event.pointerId);drag={x:event.clientX,y:event.clientY,east:this.center.east,north:this.center.north,moved:false};});
    canvas.addEventListener('pointermove',event=>{if(!drag)return;const dx=event.clientX-drag.x,dy=event.clientY-drag.y;if(Math.hypot(dx,dy)>4)drag.moved=true;if(drag.moved){this.follow=false;this.center.east=drag.east-dx/this.scale;this.center.north=drag.north+dy/this.scale;this.onPan();}});
    canvas.addEventListener('pointerup',event=>{if(drag&&!drag.moved){const rect=canvas.getBoundingClientRect();const x=event.clientX-rect.left,y=event.clientY-rect.top;const found=this.positions.find(p=>Math.hypot(p.x-x,p.y-y)<30);if(found)this.onSelect(found.id);}drag=null;});
    canvas.addEventListener('pointercancel',()=>{drag=null;});
    this.resize();
  }
  resize(){this.width=this.canvas.clientWidth;this.height=this.canvas.clientHeight;const ratio=devicePixelRatio||1;this.canvas.width=Math.round(this.width*ratio);this.canvas.height=Math.round(this.height*ratio);this.ctx.setTransform(ratio,0,0,ratio,0,0);}
  zoom(factor){this.scale=Math.min(.8,Math.max(.002,this.scale*factor));}
  point(entity){return {x:this.width/2+(entity.east_m-this.center.east)*this.scale,y:this.height/2-(entity.north_m-this.center.north)*this.scale};}
  aircraft(x,y,heading,color){
    const ctx=this.ctx;ctx.save();ctx.translate(x,y);ctx.rotate(heading*Math.PI/180);
    // Same fixed 50x visual length as 3D; map zoom changes the screen size.
    ctx.scale(this.scale*15.06*50/63,this.scale*15.06*50/63);
    // F-16: pointed nose, wing-root extensions, swept wings and rear stabilizers.
    const points=[[0,-32],[2,-25],[3,-17],[3.5,-10],[6,-2],[6,1],[22,10],[22,17],[7,12],[5,19],[13,25],[12,29],[4,25],[3,31],[-3,31],[-4,25],[-12,29],[-13,25],[-5,19],[-7,12],[-22,17],[-22,10],[-6,1],[-6,-2],[-3.5,-10],[-3,-17],[-2,-25]];
    ctx.beginPath();points.forEach(([px,py],i)=>i?ctx.lineTo(px,py):ctx.moveTo(px,py));ctx.closePath();ctx.fillStyle=color;ctx.fill();ctx.strokeStyle=this.colors.map;ctx.lineWidth=1.4;ctx.stroke();
    ctx.fillStyle=this.colors.map;ctx.beginPath();ctx.ellipse(0,-14,1.8,5.5,0,0,Math.PI*2);ctx.fill();
    ctx.beginPath();ctx.moveTo(0,12);ctx.lineTo(0,27);ctx.moveTo(-2.5,28);ctx.lineTo(2.5,28);ctx.strokeStyle=this.colors.map;ctx.lineWidth=1.2;ctx.stroke();ctx.restore();
  }
  missile(x,y,heading,color){
    const ctx=this.ctx;ctx.save();ctx.translate(x,y);ctx.rotate(heading*Math.PI/180);
    ctx.scale(this.scale*3.65*100/40,this.scale*3.65*100/40);
    ctx.beginPath();const points=[[0,-23],[3,-14],[3,5],[9,15],[9,18],[3,14],[3,17],[-3,17],[-3,14],[-9,18],[-9,15],[-3,5],[-3,-14]];
    points.forEach(([x,y],i)=>i?ctx.lineTo(x,y):ctx.moveTo(x,y));ctx.closePath();ctx.fillStyle=color;ctx.fill();ctx.restore();
  }
  draw(entities,buffer,time,selected){
    const ctx=this.ctx;if(!this.width||!this.height)return;
    const style=getComputedStyle(this.canvas);this.colors={map:style.getPropertyValue('--map').trim(),grid:style.getPropertyValue('--grid').trim(),text:style.getPropertyValue('--text').trim(),muted:style.getPropertyValue('--muted').trim(),blue:style.getPropertyValue('--blue').trim()};
    const focus=entities.find(e=>e.id===selected);if(this.follow&&focus)this.center={east:focus.east_m,north:focus.north_m};
    ctx.clearRect(0,0,this.width,this.height);
    let step=1000;while(step*this.scale<45)step*=2;while(step*this.scale>150)step/=2;
    if(this.grid){const pixels=step*this.scale;const origin=this.point({east_m:0,north_m:0});const x0=((origin.x%pixels)+pixels)%pixels,y0=((origin.y%pixels)+pixels)%pixels;ctx.strokeStyle=this.colors.grid;ctx.lineWidth=1;ctx.beginPath();for(let x=x0;x<this.width;x+=pixels){ctx.moveTo(x,0);ctx.lineTo(x,this.height);}for(let y=y0;y<this.height;y+=pixels){ctx.moveTo(0,y);ctx.lineTo(this.width,y);}ctx.stroke();}
    if(this.trail){for(const entity of entities){const samples=buffer.tracks.get(entity.id)?.samples||[];const stride=Math.max(1,Math.ceil(samples.length/1500));const trailTime=time;ctx.beginPath();let count=0;for(let i=0;i<samples.length;i+=stride){const s=samples[i];if(s.sim_time_s>trailTime)break;const p=this.point(s);count?ctx.lineTo(p.x,p.y):ctx.moveTo(p.x,p.y);count++;}const p=this.point(entity);if(count)ctx.lineTo(p.x,p.y);ctx.strokeStyle=entity.team==='red'?'#e88480':entity.kind==='missile'?'#ecc274':this.colors.blue;ctx.globalAlpha=.4;ctx.lineWidth=2;ctx.stroke();ctx.globalAlpha=1;}}
    this.positions=[];
    for(const entity of entities){const p=this.point(entity);this.positions.push({...p,id:entity.id});if(p.x< -70||p.x>this.width+70||p.y< -70||p.y>this.height+70)continue;
      const color=entity.kind==='missile'?(entity.team==='red'?'#f18c68':'#ecc274'):(entity.team==='red'?'#e88480':this.colors.blue);
      if(entity.kind==='aircraft')this.aircraft(p.x,p.y,entity.heading_deg,color);
      else if(entity.kind==='missile')this.missile(p.x,p.y,entity.heading_deg,color);
      const visualLength=entity.kind==='missile'?3.65*100:15.06*50;
      const labelX=Math.min(this.width-105,Math.max(8,p.x+Math.max(35,this.scale*visualLength/2+12)));
      ctx.font='14px "Segoe UI",sans-serif';ctx.fillStyle=this.colors.text;ctx.fillText(entity.name,labelX,Math.max(24,p.y-11));ctx.fillStyle=this.colors.muted;ctx.fillText(`${Math.round(entity.altitude_m).toLocaleString('tr-TR')} m`,labelX,Math.max(42,p.y+10));
    }
    ctx.font='14px "Segoe UI",sans-serif';ctx.fillStyle=this.colors.muted;ctx.fillText('K ↑',this.width-40,27);
    const length=step*this.scale;ctx.strokeStyle=this.colors.muted;ctx.lineWidth=1.5;ctx.beginPath();ctx.moveTo(20,this.height-22);ctx.lineTo(20+length,this.height-22);ctx.moveTo(20,this.height-26);ctx.lineTo(20,this.height-18);ctx.moveTo(20+length,this.height-26);ctx.lineTo(20+length,this.height-18);ctx.stroke();ctx.fillText(`${(step/1000).toLocaleString('tr-TR')} km`,20,this.height-34);
    if(!entities.length){ctx.fillStyle=this.colors.muted;ctx.textAlign='center';ctx.fillText('Uçuş verisi bekleniyor',this.width/2,this.height/2);ctx.textAlign='left';}
  }
}
