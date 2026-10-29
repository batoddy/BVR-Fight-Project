// Playback always uses the common simulation time stored by TacviewLogger.
export class ReplayClock {
  constructor(speed=1) { this.speed=speed; this.reset(); }
  reset() { this.time=null; this.wall=null; }
  seek(time,nowSeconds) { this.time=time; this.wall=nowSeconds; }
  tick(buffer,nowSeconds,playing=true) {
    const bounds=buffer.bounds();
    if(!bounds){this.wall=nowSeconds;return null;}
    if(this.time===null)this.time=bounds.first;
    if(this.wall!==null&&playing)this.time+=Math.max(0,nowSeconds-this.wall)*this.speed;
    this.wall=nowSeconds;
    this.time=Math.min(bounds.last,Math.max(bounds.first,this.time));
    return this.time;
  }
}

export function interpolate(a,b,time) {
  if(!b||b.sim_time_s<=a.sim_time_s)return {...a};
  const f=Math.max(0,Math.min(1,(time-a.sim_time_s)/(b.sim_time_s-a.sim_time_s)));
  const result={...a};
  for(const key of ['east_m','north_m','altitude_m','velocity_east_mps','velocity_north_mps','velocity_up_mps','true_airspeed_mps','mach','position_speed_mps','estimated_mach','load_factor_g','pitch_deg','latitude_deg','longitude_deg','path_m'])
    result[key]=Number.isFinite(a[key])&&Number.isFinite(b[key])?a[key]+(b[key]-a[key])*f:null;
  for(const key of ['heading_deg','roll_deg']) {
    const delta=((b[key]-a[key]+540)%360)-180;
    result[key]=a[key]+delta*f;
  }
  result.heading_deg=((result.heading_deg%360)+360)%360;
  result.sim_time_s=a.sim_time_s+(b.sim_time_s-a.sim_time_s)*f;
  return result;
}

export function sampleWindow(samples,time,duration=-1,maxPoints=Infinity){
  const bound=(value,inclusive)=>{let lo=0,hi=samples.length;while(lo<hi){const mid=(lo+hi)>>1;if(samples[mid].sim_time_s<value||(inclusive&&samples[mid].sim_time_s===value))lo=mid+1;else hi=mid;}return lo;};
  const first=duration<0?0:bound(time-duration,false),last=bound(time,true);
  const stride=Math.max(1,Math.ceil((last-first)/maxPoints)),result=[];
  for(let i=first;i<last;i+=stride)result.push(samples[i]);
  if(last>first&&result.at(-1)!==samples[last-1])result.push(samples[last-1]);
  return result;
}

export class SampleBuffer {
  constructor(){this.session=null;this.cursor=0;this.tracks=new Map();this.packet={};}
  consume(packet) {
    const reset=packet.session!==this.session;
    if(reset){this.tracks.clear();this.cursor=0;}
    this.session=packet.session;
    for(const item of packet.samples) {
      if(item.sequence<=this.cursor)continue;
      let track=this.tracks.get(item.id);
      if(!track){track={samples:[]};this.tracks.set(item.id,track);}
      const last=track.samples.at(-1);
      if(!last||item.sim_time_s>last.sim_time_s)track.samples.push(item);
      else if(item.sim_time_s===last.sim_time_s)track.samples[track.samples.length-1]=item;
      else{this.cursor=item.sequence;continue;}
      // İlk örnek zamanı (füzede atış anı) ve o andan beri gidilen yol, m:
      // yol = önceki yol + √(Δdoğu² + Δkuzey² + Δirtifa²)
      const samples=track.samples,before=samples.at(-2);
      item.start_s=samples[0].sim_time_s;
      item.path_m=before?before.path_m+Math.hypot(item.east_m-before.east_m,item.north_m-before.north_m,item.altitude_m-before.altitude_m):0;
      this.cursor=item.sequence;
    }
    this.cursor=Math.max(this.cursor,packet.cursor);
    this.packet=packet;
    return reset;
  }
  get complete(){return this.cursor>=this.packet.sequence;}
  bounds(){
    const tracks=[...this.tracks.values()].filter(track=>track.samples.length);
    if(!tracks.length)return null;
    return {first:Math.min(...tracks.map(track=>track.samples[0].sim_time_s)),
            last:Math.max(...tracks.map(track=>track.samples.at(-1).sim_time_s))};
  }
  at(id,time) {
    const samples=this.tracks.get(id)?.samples;
    if(!samples?.length||time==null)return null;
    // A CSV has no impact/kill events. Show only the recorded missile interval.
    const first=samples[0],last=samples.at(-1);
    if(time<first.record_start_s||first.kind==='missile'&&time>first.record_end_s)return null;
    if(time<=first.sim_time_s)return {...first};
    if(time>=last.sim_time_s)return {...last,measurement_ended:time>first.record_end_s};
    let lo=0,hi=samples.length-1;
    while(lo+1<hi){const mid=(lo+hi)>>1;if(samples[mid].sim_time_s<=time)lo=mid;else hi=mid;}
    return interpolate(samples[lo],samples[hi],time);
  }
  entities(time){return [...this.tracks.keys()].map(id=>this.at(id,time)).filter(Boolean);}
}
