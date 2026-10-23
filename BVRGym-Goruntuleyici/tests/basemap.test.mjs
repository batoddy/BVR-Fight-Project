import test from 'node:test';
import assert from 'node:assert/strict';
import {Basemap,groundPoint,groundCoordinates,tileCoordinates,tileLocation,tileProjection,visibleTiles} from '../web/basemap.js';

const origin={latitude_deg:59,longitude_deg:18,altitude_m:7000};

test('Ground map shares the aircraft ENU origin regardless of its altitude',()=>{
  const p=groundPoint(59,18,origin);
  assert.ok(Math.abs(p.east_m)<1e-8&&Math.abs(p.north_m)<1e-8);
  const east=groundPoint(59,18.01,origin),north=groundPoint(59.01,18,origin);
  assert.ok(east.east_m>500&&north.north_m>1100);
});

test('Geographic/local round trips remain aligned across latitude and the date line',()=>{
  for(const lat of [-70,0,59,80])for(const lon of [18,179.95,-179.95]){
    const reference={latitude_deg:lat,longitude_deg:lon,altitude_m:12000};
    for(const [east,north] of [[0,0],[35000,-18000],[-90000,60000]]){
      const geo=groundCoordinates(east,north,reference),point=groundPoint(geo.latitude_deg,geo.longitude_deg,reference);
      assert.ok(Math.abs(point.east_m-east)<.001&&Math.abs(point.north_m-north)<.001);
    }
  }
});

test('Tile coordinates preserve north, east and geographic boundaries',()=>{
  for(const zoom of [2,8,16]){
    const tile=tileCoordinates(59,18,zoom),geo=tileLocation(tile.x,tile.y,zoom);
    assert.ok(Math.abs(geo.latitude_deg-59)<1e-9&&Math.abs(geo.longitude_deg-18)<1e-9);
    assert.ok(tileCoordinates(60,18,zoom).y<tile.y);
    assert.ok(tileCoordinates(59,19,zoom).x>tile.x);
  }
  const tile=tileCoordinates(59,18,10),projection=tileProjection({x:Math.floor(tile.x),y:Math.floor(tile.y),z:10},origin);
  assert.equal(projection.points.length,81);
  assert.ok(projection.points[0].north_m>projection.points.at(-1).north_m);
  assert.ok(projection.points[0].east_m<projection.points[8].east_m);
});

test('Viewport tile plans stay bounded and wrap correctly at the date line',()=>{
  for(const longitude of [18,179.99,-179.99]){
    const tiles=visibleTiles({...origin,longitude_deg:longitude},0,0,500000,300000,1500);
    assert.ok(tiles.length>0&&tiles.length<=36);
    assert.ok(tiles.every(t=>t.x>=0&&t.x<2**t.z&&t.y>=0&&t.y<2**t.z));
    assert.equal(new Set(tiles.map(t=>`${t.z}/${t.x}/${t.y}`)).size,tiles.length);
  }
  assert.deepEqual(visibleTiles({...origin,latitude_deg:89},0,0,1000,1000,1500),[]);
});

test('Map downloads start only when enabled, limit concurrency and reuse loaded images',()=>{
  const images=[],map=new Basemap(()=>{const image={};images.push(image);return image;});
  map.setOrigin(origin);map.update(0,0,50000,30000,1000,0);assert.equal(images.length,0);
  map.enabled=true;map.update(0,0,50000,30000,1000,300);assert.ok(images.length>0&&images.length<=4);
  for(let i=0;i<images.length;i++)images[i].onload();
  assert.equal(map.loading,0);assert.ok(map.tiles.length>0);assert.equal(map.status,'');
  const count=images.length;map.enabled=false;map.enabled=true;map.update(0,0,50000,30000,1000,600);
  assert.equal(images.length,count);
  const firstProjection=map.tiles[0].projection;
  map.setOrigin({...origin,longitude_deg:18.01});map.update(0,0,50000,30000,1000,900);
  for(let i=count;i<images.length;i++)images[i].onload();
  assert.notEqual(map.tiles[0].projection,firstProjection);
});

test('Failed tile requests stop when disabled and can be retried explicitly',()=>{
  const images=[],map=new Basemap(()=>{const image={};images.push(image);return image;});
  map.enabled=true;map.setOrigin(origin);map.update(0,0,10000,10000,1000,0);
  map.enabled=false;for(const image of [...images])image.onerror();
  const count=images.length;assert.equal(map.loading,0);assert.equal(map.status,'');
  map.enabled=true;map.update(0,0,10000,10000,1000,300);assert.ok(images.length>count);
  for(let i=count;i<images.length;i++)images[i].onload();
  assert.equal(map.status,'');
});
