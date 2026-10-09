import test from 'node:test';
import assert from 'node:assert/strict';
import {sampleWindow,SampleBuffer} from '../web/stream.js';

test('Trail lookup includes only the chosen interval and caps drawing density',()=>{
  const samples=Array.from({length:100001},(_,i)=>({sim_time_s:i/10}));
  const points=sampleWindow(samples,5000,300,1500);
  assert.equal(points[0].sim_time_s,4700);assert.equal(points.at(-1).sim_time_s,5000);
  assert.ok(points.length<=1501);
  assert.equal(sampleWindow(samples,-1,300).length,0);
  assert.equal(sampleWindow(samples,10001,0).length,0);
});

test('Aircraft final sample is flagged as ended without inventing a destruction event',()=>{
  const buffer=new SampleBuffer();buffer.consume({session:'one',cursor:2,sequence:2,samples:[
    {id:'a',sequence:1,kind:'aircraft',sim_time_s:0,record_start_s:0,record_end_s:1},
    {id:'a',sequence:2,kind:'aircraft',sim_time_s:1,record_start_s:0,record_end_s:1}
  ]});
  assert.equal(buffer.at('a',2).measurement_ended,true);
  assert.equal(buffer.at('a',1).measurement_ended,false);
});
