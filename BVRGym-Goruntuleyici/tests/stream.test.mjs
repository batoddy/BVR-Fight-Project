import test from 'node:test';
import assert from 'node:assert/strict';
import {interpolate,SampleBuffer,ReplayClock} from '../web/stream.js';

test('Velocity components interpolate through stops and reversals without inventing missing data',()=>{
  const first={sim_time_s:0,heading_deg:0,roll_deg:0,velocity_east_mps:300,velocity_north_mps:100,velocity_up_mps:20};
  const last={...first,sim_time_s:2,velocity_east_mps:-300,velocity_north_mps:300,velocity_up_mps:-20};
  const middle=interpolate(first,last,1);
  assert.equal(middle.velocity_east_mps,0);assert.equal(middle.velocity_north_mps,200);assert.equal(middle.velocity_up_mps,0);
  assert.equal(interpolate(first,{...last,velocity_up_mps:null},1).velocity_up_mps,null);
});

const item=(sequence,time,heading=0,id='blue',kind='aircraft')=>({sequence,id,kind,sim_time_s:time,
  record_start_s:0,record_end_s:200,east_m:time*100,north_m:0,altitude_m:5000,
  true_airspeed_mps:null,mach:null,heading_deg:heading,roll_deg:0,pitch_deg:0,latitude_deg:59,longitude_deg:18});
const packet=(samples,session='flight',sequence=samples.length,cursor=samples.length)=>({samples,session,sequence,cursor});
const store=()=>{const buffer=new SampleBuffer();buffer.consume(packet([item(1,0),item(2,200)]));return buffer;};

test('Position and heading interpolate smoothly across north; absent speed stays unknown',()=>{
  const mid=interpolate(item(1,0,359),item(2,1,1),.5);
  assert.equal(mid.heading_deg,0);assert.equal(mid.east_m,50);
  assert.equal(mid.mach,null);assert.equal(mid.true_airspeed_mps,null);
});
test('Missiles use the aircraft clock and appear only within the recorded interval',()=>{
  const buffer=new SampleBuffer();const missile=time=>({...item(0,time,0,'missile','missile'),record_start_s:10,record_end_s:15});
  buffer.consume(packet([item(1,0),{...missile(10),sequence:2},{...missile(15),sequence:3},item(4,20)]));
  assert.equal(buffer.entities(9).length,1);assert.equal(buffer.entities(12).length,2);
  assert.equal(buffer.entities(15).length,2);assert.equal(buffer.entities(15.001).length,1);
  assert.equal(buffer.at('missile',15.001),null);
  assert.equal(buffer.entities(12).length,2); // Seeking backward restores the missile.
});
test('Aircraft are absent before their first sample and retain the final measured state',()=>{
  const buffer=new SampleBuffer();buffer.consume(packet([{...item(1,10),record_start_s:10},item(2,20)]));
  assert.equal(buffer.entities(9).length,0);assert.equal(buffer.at('blue',50).east_m,2000);
});
test('All tracks survive paginated loading without pruning objects absent from a page',()=>{
  const buffer=new SampleBuffer();buffer.consume(packet([item(1,0)],'flight',2,1));
  assert.equal(buffer.complete,false);
  buffer.consume(packet([item(2,1,90,'red')],'flight',2,2));
  assert.equal(buffer.complete,true);assert.equal(buffer.tracks.size,2);
});
test('New file snapshot replaces the whole previous flight, including its timeline',()=>{
  const buffer=store();buffer.consume(packet([item(1,0,270,'new')],'next'));
  assert.equal(buffer.tracks.size,1);assert.equal(buffer.at('blue',0),null);
  assert.equal(buffer.at('new',0).heading_deg,270);
});
test('Repeated HTTP responses do not duplicate samples',()=>{
  const buffer=new SampleBuffer(),page=packet([item(1,0),item(2,20)]);
  buffer.consume(page);buffer.consume(page);assert.equal(buffer.tracks.get('blue').samples.length,2);
});
test('Long recordings retain their beginning beyond the former sample limit',()=>{
  const buffer=new SampleBuffer(),samples=Array.from({length:25001},(_,index)=>item(index+1,index));
  for(let index=0;index<samples.length;index+=2000)
    buffer.consume(packet(samples.slice(index,index+2000),'long',samples.length,Math.min(samples.length,index+2000)));
  assert.equal(buffer.tracks.get('blue').samples.length,25001);
  assert.deepEqual(buffer.bounds(),{first:0,last:25000});assert.equal(buffer.at('blue',0).east_m,0);
});
for(const speed of [1,2,3,4,5,10])test(`Replay ${speed}x uses simulation seconds independently of export timing`,()=>{
  const clock=new ReplayClock(speed),buffer=store();clock.seek(10,0);
  assert.equal(clock.tick(buffer,2),10+2*speed);assert.equal(clock.tick(buffer,1000),200);
});
test('Pause and lost HTTP connection freeze the timeline without a jump on resume',()=>{
  const clock=new ReplayClock(2),buffer=store();clock.seek(10,0);
  assert.equal(clock.tick(buffer,1),12);assert.equal(clock.tick(buffer,100,false),12);
  assert.equal(clock.tick(buffer,101),14);
});
test('Seeking backward and forward remains exact while paused',()=>{
  const clock=new ReplayClock(5),buffer=store();clock.seek(90,1);assert.equal(clock.tick(buffer,2,false),90);
  clock.seek(20,2);assert.equal(clock.tick(buffer,3,false),20);
  clock.seek(120,3);assert.equal(clock.tick(buffer,4,false),120);
});
