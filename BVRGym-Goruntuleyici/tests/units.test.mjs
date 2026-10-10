import test from 'node:test';
import assert from 'node:assert/strict';
import {toFeet,toNauticalMiles,headingOffset,munitionState,MODEL_LENGTHS,speedLabel,speedValue,altitudeValue,altitudeLabel,distanceLabel,distanceScale} from '../web/units.js';

test('Display unit conversions preserve SI input and prioritize recorded Mach',()=>{
  const entity={true_airspeed_mps:1852/3600,mach:.8,estimated_mach:.7};
  assert.equal(speedValue(entity,'kts'),1);
  assert.ok(Math.abs(speedValue(entity,'kmh')-1.852)<1e-12);
  assert.equal(speedValue({true_airspeed_mps:250},'kms'),.25);
  assert.equal(speedLabel({true_airspeed_mps:250},'kms'),'0,25 km/s');
  assert.equal(speedLabel(entity,'mach'),'0,80 Mach');
  assert.equal(speedLabel({estimated_mach:.96},'mach'),'0,96 Mach');
  assert.equal(speedLabel({true_airspeed_mps:250},'mach'),'— Mach');
  assert.equal(altitudeValue(5000,'m'),5000);
  assert.equal(altitudeLabel(5000,'m'),'5.000 m');
  assert.equal(distanceLabel(1852,'nm'),'1,00 NM');
  assert.equal(distanceLabel(1852,'km'),'1,85 km');
  assert.equal(entity.true_airspeed_mps,1852/3600);
});
test('Distance ruler remains physically correct for NM and km across zoom levels',()=>{
  for(const unit of ['nm','km'])for(const metersPerPixel of [1,10,100,1000]){
    const scale=distanceScale(metersPerPixel,140,unit);
    assert.ok(scale.pixels>50&&scale.pixels<=140);
    assert.ok(Math.abs(scale.pixels*metersPerPixel-scale.meters)<1e-9);
    assert.equal(scale.symbol,unit==='nm'?'NM':'km');
  }
});

test('Compact speed labels prefer measured TAS, round estimates and preserve zero',()=>{
  assert.equal(speedLabel({true_airspeed_mps:250,position_speed_mps:300}),'250 m/s');
  assert.equal(speedLabel({true_airspeed_mps:0,position_speed_mps:300}),'0 m/s');
  assert.equal(speedLabel({true_airspeed_mps:null,position_speed_mps:300.15}),'300 m/s');
  assert.equal(speedLabel({true_airspeed_mps:NaN,position_speed_mps:200}),'200 m/s');
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
test('Recorded outcome is shown only when asked, and only after it happened',()=>{
  const hit={record_start_s:10,record_end_s:20,outcome:'hit',outcome_time_s:20.5};
  const lost={record_start_s:10,record_end_s:20,outcome:'lost',outcome_time_s:20.5};
  // Kapali: kayitta olay olsa da sonuc gosterilmez.
  assert.equal(munitionState(hit,21),'spent');
  assert.equal(munitionState(hit,21,false),'spent');
  // Acik: sonuc ancak olay aninda ve sonrasinda gorunur.
  assert.equal(munitionState(hit,20,true),'flying');
  assert.equal(munitionState(hit,20.5,true),'hit');
  assert.equal(munitionState(lost,21,true),'lost');
  // Olaysiz kayit her iki durumda da yalnizca olcumun bittigini soyler.
  assert.equal(munitionState({record_start_s:10,record_end_s:20},21,true),'spent');
});
