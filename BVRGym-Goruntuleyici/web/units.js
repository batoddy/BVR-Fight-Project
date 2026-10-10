export const METERS_PER_NM=1852;
export const METERS_PER_FOOT=.3048;
export const MODEL_LENGTHS={aircraft:15.06,missile:3.66};
export const visualScale=(kind,scale)=>scale*(kind==='missile'&&scale!==1?1.25:1);
export const toNauticalMiles=meters=>meters/METERS_PER_NM;
export const toFeet=meters=>meters/METERS_PER_FOOT;
export const SPEED_UNITS={mps:{symbol:'m/s',factor:1,digits:1,labelDigits:0},kmh:{symbol:'km/sa',factor:3.6,digits:1,labelDigits:0},kms:{symbol:'km/s',factor:.001,digits:3,labelDigits:2},kts:{symbol:'kt',factor:3600/1852,digits:1,labelDigits:0},mach:{symbol:'Mach',digits:2,labelDigits:2}};
export const ALTITUDE_UNITS={ft:{symbol:'ft',factor:1/METERS_PER_FOOT},m:{symbol:'m',factor:1}};
export const DISTANCE_UNITS={nm:{symbol:'NM',factor:1/METERS_PER_NM},km:{symbol:'km',factor:.001}};
export const altitudeValue=(meters,unit='ft')=>meters*(ALTITUDE_UNITS[unit]||ALTITUDE_UNITS.ft).factor;
export const altitudeLabel=(meters,unit='ft')=>Math.round(altitudeValue(meters,unit)).toLocaleString('tr-TR')+' '+(ALTITUDE_UNITS[unit]||ALTITUDE_UNITS.ft).symbol;
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
