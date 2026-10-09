export const METERS_PER_NM=1852;
export const METERS_PER_FOOT=.3048;
export const MODEL_LENGTHS={aircraft:15.06,missile:3.66};
export const visualScale=(kind,scale)=>scale*(kind==='missile'&&scale!==1?1.25:1);
export const toNauticalMiles=meters=>meters/METERS_PER_NM;
export const toFeet=meters=>meters/METERS_PER_FOOT;
export const altitudeLabel=meters=>Math.round(toFeet(meters)).toLocaleString('tr-TR')+' ft';

export function speedLabel(entity){
  const recorded=Number.isFinite(entity.true_airspeed_mps);
  const speed=recorded?entity.true_airspeed_mps:entity.estimated_tas_mps;
  if(!Number.isFinite(speed))return '— m/s';
  return Math.round(speed).toLocaleString('tr-TR')+' m/s';
}

export function distanceScale(metersPerPixel,maxPixels=140){
  if(!Number.isFinite(metersPerPixel)||metersPerPixel<=0||maxPixels<=0)return null;
  const maxNM=toNauticalMiles(metersPerPixel*maxPixels);
  const magnitude=10**Math.floor(Math.log10(maxNM));
  const normalized=maxNM/magnitude;
  const nauticalMiles=(normalized>=5?5:normalized>=2?2:1)*magnitude;
  return {nauticalMiles,pixels:nauticalMiles*METERS_PER_NM/metersPerPixel};
}

export function headingOffset(heading,length){
  const angle=heading*Math.PI/180;
  return {east:Math.sin(angle)*length,north:Math.cos(angle)*length};
}

export function munitionState(munition,time){
  if(munition.record_start_s==null)return 'carried';
  if(time<munition.record_start_s)return 'carried';
  if(time<=munition.record_end_s)return 'flying';
  return 'spent';
}
