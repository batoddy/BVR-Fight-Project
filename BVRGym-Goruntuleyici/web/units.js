export const METERS_PER_NM=1852;
export const METERS_PER_FOOT=.3048;
export const MODEL_LENGTHS={aircraft:15.06,missile:3.66};
// Nesne büyütme: 1× gerçek boy. Füzeler 1× dışında görünürlük için ayrıca 3,75 kat (3 × %125) büyür.
export const visualScale=(kind,scale)=>scale*(kind==='missile'&&scale!==1?3.75:1);
// Ekrandaki model boyu (m): gerçek boy × büyütme. Yardımcı çizgiler bununla orantılı.
export const visualLength=(kind,scale)=>(MODEL_LENGTHS[kind]||MODEL_LENGTHS.aircraft)*visualScale(kind,scale);
// Yardımcı çizgiler modelle orantılı, ama uzaktan kaybolmasın diye
// ekranda en az `pixels` kadar: metersPerPixel o noktadaki ölçek.
export const guideLength=(modelMeters,factor,metersPerPixel,pixels)=>Math.max(modelMeters*factor,metersPerPixel*pixels);
// Common visual scale: 100 m/s produces an 18 px shaft at the reference depth.
export const VELOCITY_PIXELS_PER_MPS=.18;
export function velocityVector(entity){
  const east=entity.velocity_east_mps,north=entity.velocity_north_mps,up=entity.velocity_up_mps;
  if(![east,north,up].every(Number.isFinite)||entity.measurement_ended)return null;
  const speed=Math.hypot(east,north,up);
  return speed>1e-3?{east,north,up,speed}:null;
}
export const toNauticalMiles=meters=>meters/METERS_PER_NM;
export const toFeet=meters=>meters/METERS_PER_FOOT;
export const SPEED_UNITS={mps:{symbol:'m/s',factor:1,digits:1,labelDigits:0},kmh:{symbol:'km/sa',factor:3.6,digits:1,labelDigits:0},kms:{symbol:'km/s',factor:.001,digits:3,labelDigits:2},kts:{symbol:'kt',factor:3600/1852,digits:1,labelDigits:0},mach:{symbol:'Mach',digits:2,labelDigits:2}};
export const ALTITUDE_UNITS={ft:{symbol:'ft',factor:1/METERS_PER_FOOT},m:{symbol:'m',factor:1}};
export const DISTANCE_UNITS={nm:{symbol:'NM',factor:1/METERS_PER_NM},km:{symbol:'km',factor:.001}};
export const altitudeValue=(meters,unit='ft')=>meters*(ALTITUDE_UNITS[unit]||ALTITUDE_UNITS.ft).factor;
export const altitudeLabel=(meters,unit='ft')=>Math.round(altitudeValue(meters,unit)).toLocaleString('tr-TR')+' '+(ALTITUDE_UNITS[unit]||ALTITUDE_UNITS.ft).symbol;
export function altitudeReferenceInfo(reference){
  const ground=' Yer düzlemi çizim için 0 m referansıdır; arazi yüksekliği gösterilmez.';
  if(reference==='WGS84 ellipsoid height')return {label:'İrtifa · WGS84',note:'İrtifa WGS84 elipsoidine göredir. Deniz seviyesine (MSL) dönüşüm uygulanmaz.'+ground};
  if(reference==='MSL'||reference==='mean sea level')return {label:'İrtifa · MSL',note:'Kayıt, irtifa referansını ortalama deniz seviyesi (MSL) olarak bildiriyor.'+ground};
  return {label:'İrtifa',note:(reference?'Kaydın bildirdiği irtifa referansı: '+reference+'. Dönüşüm uygulanmaz.':'Kayıt irtifa referansını belirtmiyor; MSL olduğu varsayılmaz.')+ground};
}
export const distanceLabel=(meters,unit='nm',digits=2)=>{
  const definition=DISTANCE_UNITS[unit]||DISTANCE_UNITS.nm;
  return (meters*definition.factor).toLocaleString('tr-TR',{minimumFractionDigits:digits,maximumFractionDigits:digits})+' '+definition.symbol;
};

export function speedValue(entity,unit='mps'){
  if(unit==='mach')return Number.isFinite(entity.mach)?entity.mach:Number.isFinite(entity.estimated_mach)?entity.estimated_mach:null;
  const speed=Number.isFinite(entity.true_airspeed_mps)?entity.true_airspeed_mps:entity.position_speed_mps;
  return Number.isFinite(speed)?speed*(SPEED_UNITS[unit]||SPEED_UNITS.mps).factor:null;
}

export function speedLabel(entity,unit='mps'){
  const definition=SPEED_UNITS[unit]||SPEED_UNITS.mps,speed=speedValue(entity,unit);
  const value=speed==null?'—':speed.toLocaleString('tr-TR',{minimumFractionDigits:definition.labelDigits,maximumFractionDigits:definition.labelDigits});
  return value+' '+definition.symbol;
}

export function distanceScale(metersPerPixel,maxPixels=140,unit='nm'){
  if(!Number.isFinite(metersPerPixel)||metersPerPixel<=0||maxPixels<=0)return null;
  const definition=DISTANCE_UNITS[unit]||DISTANCE_UNITS.nm;
  const maximum=metersPerPixel*maxPixels*definition.factor;
  const magnitude=10**Math.floor(Math.log10(maximum)),normalized=maximum/magnitude;
  const value=(normalized>=5?5:normalized>=2?2:1)*magnitude,meters=value/definition.factor;
  return {value,symbol:definition.symbol,meters,pixels:meters/metersPerPixel};
}

export function headingOffset(heading,length){
  const angle=heading*Math.PI/180;
  return {east:Math.sin(angle)*length,north:Math.cos(angle)*length};
}

export function munitionState(munition,time,showOutcome=false){
  if(munition.record_start_s==null)return 'carried';
  if(time<munition.record_start_s)return 'carried';
  if(time<=munition.record_end_s)return 'flying';
  // Sonuç yalnızca istenirse gösterilir; kayıtta olay varsa ne olduğunu o söyler.
  if(showOutcome&&munition.outcome&&(munition.outcome_time_s==null||time>=munition.outcome_time_s))return munition.outcome;
  return 'spent';
}

// Otopilot hesabından hedef çizgisi etiketi. Değerler kayıttaki gibi; burada hesaplanmaz.
// Yuvarlanınca -0,0 kalmasın: C++'tan -1e-9 gibi değerler gelir.
export const angleLabel=value=>Number.isFinite(value)?((Math.round(value*10)/10)||0).toLocaleString('tr-TR',{minimumFractionDigits:1,maximumFractionDigits:1})+'°':'—';
// ATA: işaret yerine taraf (+ sağda, C++ ile aynı). Tam 0 ve 180'de taraf yok.
export const sideAngleLabel=value=>{
  if(!Number.isFinite(value))return '—';
  const rounded=Math.round(value*10)/10,side=rounded>0&&rounded<180?' R':rounded<0&&rounded>-180?' L':'';
  return angleLabel(Math.abs(rounded))+side;
};
export function analysisLabel(calc,options,unit='nm'){
  if(!calc)return [];
  const parts=[];
  if(options.ata)parts.push('ATA '+sideAngleLabel(calc.ata_deg));
  if(options.bearing)parts.push('BRG '+angleLabel(calc.bearing_deg));
  if(options.range)parts.push('R '+(Number.isFinite(calc.range_3d_m)?distanceLabel(calc.range_3d_m,unit,1):'—'));
  if(options.aspect)parts.push('AA '+angleLabel(calc.aspect_deg));
  return parts;
}
