// WGS84 ground imagery projected into the same local ENU frame as recordings.py.
const RAD=Math.PI/180,A=6378137,E2=6.69437999014e-3,CIRCUMFERENCE=2*Math.PI*A;
export const MAX_MAP_LATITUDE=85.05112878;
const wrap=(value,size)=>((value%size)+size)%size;

function ecef(latitude,longitude,altitude=0){
  const lat=latitude*RAD,lon=longitude*RAD,n=A/Math.sqrt(1-E2*Math.sin(lat)**2);
  return [(n+altitude)*Math.cos(lat)*Math.cos(lon),(n+altitude)*Math.cos(lat)*Math.sin(lon),(n*(1-E2)+altitude)*Math.sin(lat)];
}

export function groundPoint(latitude,longitude,origin){
  const xyz=ecef(latitude,longitude),anchor=ecef(origin.latitude_deg,origin.longitude_deg,origin.altitude_m);
  const [x,y,z]=xyz.map((value,i)=>value-anchor[i]),lat=origin.latitude_deg*RAD,lon=origin.longitude_deg*RAD;
  return {east_m:-Math.sin(lon)*x+Math.cos(lon)*y,north_m:-Math.sin(lat)*Math.cos(lon)*x-Math.sin(lat)*Math.sin(lon)*y+Math.cos(lat)*z};
}

export function groundCoordinates(east,north,origin){
  let latitude=origin.latitude_deg+north/A/RAD;
  let longitude=origin.longitude_deg+east/(A*Math.max(.001,Math.cos(origin.latitude_deg*RAD)))/RAD;
  // Invert the actual ENU projection, including its latitude-dependent scale.
  for(let i=0;i<6;i++){
    const p=groundPoint(latitude,longitude,origin),dp=1e-5;
    const lat=groundPoint(latitude+dp,longitude,origin),lon=groundPoint(latitude,longitude+dp,origin);
    const a=(lat.east_m-p.east_m)/dp,b=(lon.east_m-p.east_m)/dp,c=(lat.north_m-p.north_m)/dp,d=(lon.north_m-p.north_m)/dp;
    const determinant=a*d-b*c;if(Math.abs(determinant)<1e-6)break;
    const de=east-p.east_m,dn=north-p.north_m;
    latitude+=(d*de-b*dn)/determinant;longitude+=(a*dn-c*de)/determinant;
  }
  return {latitude_deg:latitude,longitude_deg:wrap(longitude+180,360)-180};
}

export function tileCoordinates(latitude,longitude,zoom){
  const lat=Math.max(-MAX_MAP_LATITUDE,Math.min(MAX_MAP_LATITUDE,latitude))*RAD,n=2**zoom;
  return {x:(longitude+180)/360*n,y:(1-Math.asinh(Math.tan(lat))/Math.PI)/2*n};
}

export function tileLocation(x,y,zoom){
  const n=2**zoom;
  return {latitude_deg:Math.atan(Math.sinh(Math.PI*(1-2*y/n)))/RAD,longitude_deg:x/n*360-180};
}

export function visibleTiles(origin,east,north,width,height,pixelWidth){
  const center=groundCoordinates(east,north,origin);
  if(Math.abs(center.latitude_deg)>MAX_MAP_LATITUDE)return [];
  const resolution=Math.max(width/Math.max(1,pixelWidth),1);
  let zoom=Math.max(2,Math.min(16,Math.floor(Math.log2(CIRCUMFERENCE*Math.cos(center.latitude_deg*RAD)/(256*resolution)))));
  const corners=[[-.5,-.5],[-.5,.5],[.5,-.5],[.5,.5]].map(([x,y])=>groundCoordinates(east+x*width,north+y*height,origin));
  let bounds;
  do{
    const n=2**zoom,mid=tileCoordinates(center.latitude_deg,center.longitude_deg,zoom);
    const points=corners.map(p=>{const t=tileCoordinates(p.latitude_deg,p.longitude_deg,zoom);t.x+=Math.round((mid.x-t.x)/n)*n;return t;});
    bounds={left:Math.floor(Math.min(...points.map(p=>p.x))),right:Math.floor(Math.max(...points.map(p=>p.x))),
      top:Math.max(0,Math.floor(Math.min(...points.map(p=>p.y)))),bottom:Math.min(n-1,Math.floor(Math.max(...points.map(p=>p.y))))};
    if((bounds.right-bounds.left+1)*(bounds.bottom-bounds.top+1)<=36||zoom===2)break;
    zoom--;
  }while(true);
  const entries=[],n=2**zoom;
  for(let y=bounds.top;y<=bounds.bottom;y++)for(let x=bounds.left;x<=bounds.right;x++)entries.push({x:wrap(x,n),y,z:zoom});
  const mid=tileCoordinates(center.latitude_deg,center.longitude_deg,zoom);
  return entries.sort((a,b)=>Math.hypot(a.x-mid.x,a.y-mid.y)-Math.hypot(b.x-mid.x,b.y-mid.y));
}

export function tileProjection(tile,origin){
  const points=[],segments=8;
  for(let y=0;y<=segments;y++)for(let x=0;x<=segments;x++){
    const geo=tileLocation(tile.x+x/segments,tile.y+y/segments,tile.z);
    points.push({...groundPoint(geo.latitude_deg,geo.longitude_deg,origin),u:x/segments,v:y/segments});
  }
  return {points,segments,left:Math.min(...points.map(p=>p.east_m)),right:Math.max(...points.map(p=>p.east_m)),
    bottom:Math.min(...points.map(p=>p.north_m)),top:Math.max(...points.map(p=>p.north_m))};
}

function rasterize(tile){
  const projection=tile.projection,canvas=document.createElement('canvas');
  canvas.width=256;canvas.height=Math.max(1,Math.round(256*(projection.top-projection.bottom)/(projection.right-projection.left)));
  const ctx=canvas.getContext('2d'),{segments,points}=projection;
  const at=p=>({x:(p.east_m-projection.left)/(projection.right-projection.left)*canvas.width,y:(projection.top-p.north_m)/(projection.top-projection.bottom)*canvas.height});
  for(let y=0;y<segments;y++)for(let x=0;x<segments;x++){
    const i=y*(segments+1)+x;
    for(const indices of [[i,i+1,i+segments+1],[i+1,i+segments+2,i+segments+1]]){
      const [a,b,c]=indices.map(index=>points[index]),[p,q,r]=[a,b,c].map(at);
      const ax=a.u*256,ay=a.v*256,bx=(b.u-a.u)*256,by=(b.v-a.v)*256,cx=(c.u-a.u)*256,cy=(c.v-a.v)*256;
      const det=bx*cy-by*cx;
      const m11=((q.x-p.x)*cy-(r.x-p.x)*by)/det,m12=((q.y-p.y)*cy-(r.y-p.y)*by)/det;
      const m21=((r.x-p.x)*bx-(q.x-p.x)*cx)/det,m22=((r.y-p.y)*bx-(q.y-p.y)*cx)/det;
      // Overlap subpixel clip edges so antialiasing does not expose diagonal seams.
      const center={x:(p.x+q.x+r.x)/3,y:(p.y+q.y+r.y)/3};
      const clip=[p,q,r].map(v=>{const dx=v.x-center.x,dy=v.y-center.y,length=Math.hypot(dx,dy);return {x:v.x+dx/length*.75,y:v.y+dy/length*.75};});
      ctx.save();ctx.beginPath();ctx.moveTo(clip[0].x,clip[0].y);ctx.lineTo(clip[1].x,clip[1].y);ctx.lineTo(clip[2].x,clip[2].y);ctx.closePath();ctx.clip();
      ctx.setTransform(m11,m12,m21,m22,p.x-m11*ax-m21*ay,p.y-m12*ax-m22*ay);ctx.drawImage(tile.image,0,0,256,256);ctx.restore();
    }
  }
  return canvas;
}

export class Basemap {
  constructor(imageFactory=()=>new Image()){
    this.imageFactory=imageFactory;this.enabled=false;this.origin=null;this.cache=new Map();this.active=[];this.loading=0;this.revision=0;this.lastPlan=-Infinity;
  }
  get enabled(){return this._enabled;}
  set enabled(value){
    if(value&&!this._enabled){for(const tile of this.cache?.values()||[])if(tile.state==='error')tile.state='waiting';this.lastPlan=-Infinity;}
    this._enabled=value;
  }
  setOrigin(origin){
    const key=origin?JSON.stringify(origin):'';if(key===this.originKey)return;
    this.originKey=key;this.origin=origin;this.active=[];this.lastPlan=-Infinity;this.revision++;
    for(const tile of this.cache.values()){tile.projection=null;tile.raster=null;}
  }
  update(east,north,width,height,pixelWidth,now=performance.now()){
    if(!this.enabled||!this.origin)return;
    if(now-this.lastPlan<250)return;this.lastPlan=now;
    const layout=visibleTiles(this.origin,east,north,Math.min(600000,width),Math.min(600000,height),pixelWidth);
    const key=layout.map(t=>`${t.z}/${t.x}/${t.y}`).join('|');
    if(this.layoutKey!==key){this.layoutKey=key;this.revision++;}
    this.active=layout.map(info=>{
      const id=`${info.z}/${info.x}/${info.y}`;let tile=this.cache.get(id);
      if(!tile){tile={...info,id,state:'waiting'};this.cache.set(id,tile);}
      this.cache.delete(id);this.cache.set(id,tile);return tile;
    });
    const active=new Set(this.active);
    for(const [id,tile] of this.cache){if(this.cache.size<=80)break;if(!active.has(tile)&&tile.state!=='loading')this.cache.delete(id);}
    this.pump();
  }
  pump(){
    if(!this.enabled)return;
    for(const tile of this.active){
      if(this.loading>=4)break;if(tile.state!=='waiting')continue;
      tile.state='loading';this.loading++;
      const img=this.imageFactory();tile.image=img;img.crossOrigin='anonymous';
      let done=false;
      const complete=state=>{if(done)return;done=true;clearTimeout(timeout);tile.state=state;this.loading--;this.revision++;this.pump();};
      const timeout=setTimeout(()=>complete('error'),15000);
      img.onload=()=>complete('ready');img.onerror=()=>complete('error');
      // Default browser HTTP caching and Referer are deliberately preserved.
      img.src=`https://tile.openstreetmap.org/${tile.id}.png`;
    }
  }
  get tiles(){
    return this.active.filter(tile=>tile.state==='ready').map(tile=>{
      if(!tile.projection)tile.projection=tileProjection(tile,this.origin);
      return tile;
    });
  }
  get status(){
    if(!this.enabled)return '';
    if(!this.origin)return 'Harita için kayıt aç';
    if(!this.active.length)return 'Bu bölgede harita yok';
    if(this.active.some(t=>t.state==='error'))return 'Harita kısmen yüklenemedi · internet bağlantısını kontrol et';
    return this.active.some(t=>t.state!=='ready')?'Harita yükleniyor…':'';
  }
  draw2D(ctx,point,scale,dark){
    if(!this.enabled)return;
    ctx.save();if(dark)ctx.filter='brightness(.58) saturate(.65)';
    for(const tile of this.tiles){
      if(!tile.raster)tile.raster=rasterize(tile);
      const p=tile.projection,top=point({east_m:p.left,north_m:p.top});
      ctx.drawImage(tile.raster,top.x,top.y,(p.right-p.left)*scale,(p.top-p.bottom)*scale);
    }
    ctx.restore();
  }
}
