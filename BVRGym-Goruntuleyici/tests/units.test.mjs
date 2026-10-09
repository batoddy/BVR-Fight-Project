import test from 'node:test';
import assert from 'node:assert/strict';
import {toFeet,toNauticalMiles,headingOffset,munitionState,MODEL_LENGTHS,speedLabel} from '../web/units.js';

test('Compact speed labels prefer measured TAS, round estimates and preserve zero',()=>{
  assert.equal(speedLabel({true_airspeed_mps:250,estimated_tas_mps:300}),'250 m/s');
  assert.equal(speedLabel({true_airspeed_mps:0,estimated_tas_mps:300}),'0 m/s');
  assert.equal(speedLabel({true_airspeed_mps:null,estimated_tas_mps:300.15}),'300 m/s');
  assert.equal(speedLabel({true_airspeed_mps:NaN,estimated_tas_mps:200}),'200 m/s');
  assert.equal(speedLabel({}),'— m/s');
});
test('NM and feet convert display values without changing the physical data',()=>{
  assert.equal(toNauticalMiles(1852),1);
  assert.equal(toFeet(.3048),1);
  assert.ok(MODEL_LENGTHS.aircraft/MODEL_LENGTHS.missile>4&&MODEL_LENGTHS.aircraft/MODEL_LENGTHS.missile<4.2);
});
test('Heading uses true north and keeps the chosen world-space length',()=>{
  for(const [heading,east,north] of [[0,0,100],[90,100,0],[180,0,-100],[270,-100,0]]){
    const offset=headingOffset(heading,100);
    assert.ok(Math.abs(offset.east-east)<1e-8&&Math.abs(offset.north-north)<1e-8);
  }
});
test('Inventory distinguishes carried, flying and spent at the replay time',()=>{
  const recorded={record_start_s:10,record_end_s:20};
  assert.equal(munitionState(recorded,9),'carried');
  assert.equal(munitionState(recorded,10),'flying');
  assert.equal(munitionState(recorded,20),'flying');
  assert.equal(munitionState(recorded,21),'spent');
  assert.equal(munitionState({record_start_s:null},100),'carried');
});
