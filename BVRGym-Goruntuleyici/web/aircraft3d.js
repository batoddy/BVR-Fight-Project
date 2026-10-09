import * as THREE from 'three';
import {MODEL_LENGTHS} from './units.js';

function normalizeLength(model,kind,material){
  const bounds=new THREE.Box3().setFromObject(model);
  const length=bounds.max.z-bounds.min.z;
  model.scale.z=MODEL_LENGTHS[kind]/length;
  const root=new THREE.Group();root.add(model);root.userData.bodyMaterial=material;
  return root;
}

function plate(points, thickness, material) {
  const shape=new THREE.Shape();
  points.forEach(([x,z],i)=>i?shape.lineTo(x,-z):shape.moveTo(x,-z));
  shape.closePath();
  const geometry=new THREE.ExtrudeGeometry(shape,{depth:thickness,bevelEnabled:false});
  geometry.rotateX(-Math.PI/2);geometry.translate(0,-thickness/2,0);
  return new THREE.Mesh(geometry,material);
}

export function createF16(color=0x6b9bbc) {
  // Approximate F-16 silhouette: 15.06 m long, 9.96 m wingspan.
  // Model axes: X right wing, Y up, nose toward -Z.
  const aircraft=new THREE.Group();
  const body=new THREE.MeshStandardMaterial({color,roughness:.48,metalness:.45});
  const edge=new THREE.MeshStandardMaterial({color:0x314454,roughness:.65,metalness:.3});
  const glass=new THREE.MeshStandardMaterial({color:0x162e42,roughness:.13,metalness:.7});
  const noseMaterial=new THREE.MeshStandardMaterial({color:0x48545f,roughness:.7});
  const nozzleMaterial=new THREE.MeshStandardMaterial({color:0x222d36,metalness:.7,roughness:.45});
  const section=[
    [-8.0,0.015,.015],[-6.3,.24,.22],[-4.6,.43,.42],[-2.7,.58,.54],
    [-.6,.69,.55],[1.8,.71,.56],[4.7,.48,.43],[6.4,.40,.37],[7.06,.38,.35]
  ];
  const positions=[],indices=[],rings=16;
  section.forEach(([z,width,height])=>{
    for(let i=0;i<rings;i++){const a=i/rings*Math.PI*2;positions.push(Math.cos(a)*width,Math.sin(a)*height,z);}
  });
  for(let row=0;row<section.length-1;row++)for(let i=0;i<rings;i++){
    const a=row*rings+i,b=row*rings+(i+1)%rings,c=b+rings,d=a+rings;
    indices.push(a,b,d,b,c,d);
  }
  const skin=new THREE.BufferGeometry();skin.setAttribute('position',new THREE.Float32BufferAttribute(positions,3));skin.setIndex(indices);skin.computeVertexNormals();
  const fuselage=new THREE.Mesh(skin,body);aircraft.add(fuselage);
  const wings=plate([[-.6,-1.6],[-4.98,2.45],[-4.98,3.85],[-.7,2.55],
    [.7,2.55],[4.98,3.85],[4.98,2.45],[.6,-1.6]],.16,body);
  aircraft.add(wings);
  // Leading-edge root extensions.
  aircraft.add(plate([[-.35,-4.8],[-1.15,-.7],[-.7,1.7],[.7,1.7],[1.15,-.7],[.35,-4.8]],.1,body));
  for(const side of [-1,1]){
    const stab=plate([[side*.45,3.55],[side*2.6,5.4],[side*2.45,6.75],[side*.42,5.65]],.12,body);
    aircraft.add(stab);
    const rail=new THREE.Mesh(new THREE.CylinderGeometry(.035,.035,1.7,6),edge);
    rail.rotation.x=Math.PI/2;rail.position.set(side*4.95,.05,3.05);aircraft.add(rail);
  }
  // Single vertical stabilizer.
  const finGeometry=new THREE.BufferGeometry();
  finGeometry.setAttribute('position',new THREE.Float32BufferAttribute([
    0,.35,1.75, 0,2.65,4.35, 0,2.65,5.1, 0,.35,6.55
  ],3));finGeometry.setIndex([0,1,2,0,2,3]);finGeometry.computeVertexNormals();
  const finMaterial=body.clone();finMaterial.side=THREE.DoubleSide;
  aircraft.add(new THREE.Mesh(finGeometry,finMaterial));
  const canopy=new THREE.Mesh(new THREE.SphereGeometry(1,20,12),glass);
  canopy.scale.set(.39,.46,1.18);canopy.position.set(0,.49,-3.45);aircraft.add(canopy);
  const nose=new THREE.Mesh(new THREE.ConeGeometry(.24,1.7,16),noseMaterial);
  nose.rotation.x=-Math.PI/2;nose.position.set(0,0,-7.15);aircraft.add(nose);
  const intake=new THREE.Mesh(new THREE.SphereGeometry(1,16,8),edge);
  intake.scale.set(.42,.35,.74);intake.position.set(0,-.48,-1.75);aircraft.add(intake);
  const exhaust=new THREE.Mesh(new THREE.CylinderGeometry(.38,.41,.65,16,1,true),nozzleMaterial);
  exhaust.rotation.x=Math.PI/2;exhaust.position.set(0,0,6.75);aircraft.add(exhaust);
  aircraft.userData.bodyMaterial=body;
  return normalizeLength(aircraft,'aircraft',body);
}

export function createMissile(color=0xecc274) {
  // AIM-120 silhouette, normalized to 3.66 m; nose uses the same -Z axis as aircraft.
  const missile=new THREE.Group();
  const body=new THREE.MeshStandardMaterial({color,metalness:.35,roughness:.5});
  const cylinder=new THREE.Mesh(new THREE.CylinderGeometry(.09,.09,2.95,12),body);
  cylinder.rotation.x=Math.PI/2;missile.add(cylinder);
  const nose=new THREE.Mesh(new THREE.ConeGeometry(.09,.7,12),body);
  nose.rotation.x=-Math.PI/2;nose.position.z=-1.825;missile.add(nose);
  for(let i=0;i<4;i++){
    const fin=plate([[0,.5],[.4,1.25],[.4,1.5],[0,1.25]],.025,body);
    fin.rotation.z=i*Math.PI/2;missile.add(fin);
  }
  const nozzle=new THREE.Mesh(new THREE.CylinderGeometry(.07,.07,.12,10),new THREE.MeshStandardMaterial({color:0x3e454a,metalness:.6,roughness:.6}));
  nozzle.rotation.x=Math.PI/2;nozzle.position.z=1.5;missile.add(nozzle);
  return normalizeLength(missile,'missile',body);
}

export function createGeneric(kind,color){
  const group=new THREE.Group();
  const body=new THREE.MeshStandardMaterial({color,roughness:.7});
  const symbol=new THREE.Mesh(new THREE.OctahedronGeometry(1,0),body);
  symbol.scale.set(.45,.35,1);group.add(symbol);
  return normalizeLength(group,kind,body);
}

export function disposeObject(object) {
  const geometries=new Set(),materials=new Set();
  object.traverse(node=>{if(node.geometry)geometries.add(node.geometry);if(node.material)for(const material of [].concat(node.material))materials.add(material);});
  geometries.forEach(g=>g.dispose());materials.forEach(m=>m.dispose());
}
