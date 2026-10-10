import {munitionState} from './units.js';
export const stateNames={carried:'Uçakta',flying:'Havada',spent:'Atıldı',hit:'İsabet',lost:'Kayıp'};
const summaryLabels=[['carried','uçakta'],['flying','havada'],['hit','isabet'],['lost','kayıp'],['spent','atıldı']];

export function inventoryPanel(munitions,time,selected,onSelect,showOutcome=false){
  const panel=document.createElement('div');panel.className='inventory';
  const states=munitions.map(munition=>munitionState(munition,time,showOutcome));
  const summary=document.createElement('p');summary.className='inventory-summary';
  const count=state=>states.filter(value=>value===state).length;
  const models=[...new Set(munitions.map(munition=>munition.model.split(' ')[0]))].join(', ');
  const parts=summaryLabels.filter(([state])=>count(state)).map(([state,label])=>count(state)+' '+label);
  summary.textContent=(models?models+' · ':'')+(parts.length?parts.join(' · '):'mühimmat yok');
  panel.append(summary);
  const grid=document.createElement('div');grid.className='inventory-grid';
  for(const [index,munition] of munitions.entries()){
    const state=states[index],item=document.createElement('button');
    item.className='munition '+state+(selected===munition.id?' selected':'');
    item.dataset.id=munition.id;item.dataset.state=state;item.dataset.kind='missile';item.dataset.team=munition.team;
    item.setAttribute('aria-pressed',String(selected===munition.id));
    item.disabled=!munition.has_track;
    item.title=munition.model+' · '+munition.name+' · '+stateNames[state]+(!munition.has_track?' · Atış ölçümü bulunmuyor':'');
    item.setAttribute('aria-label',munition.model+' · '+munition.name+' · '+stateNames[state]);
    const icon=document.createElementNS('http://www.w3.org/2000/svg','svg');
    icon.setAttribute('viewBox','0 0 24 80');icon.setAttribute('aria-hidden','true');icon.classList.add('munition-icon');
    const shape=document.createElementNS('http://www.w3.org/2000/svg','path');
    shape.setAttribute('d','M12 2 L15 12 L15 33 L21 44 L21 49 L15 44 L15 62 L20 71 L20 75 L14 71 L14 77 L10 77 L10 71 L4 75 L4 71 L9 62 L9 44 L3 49 L3 44 L9 33 L9 12 Z');
    const spine=document.createElementNS('http://www.w3.org/2000/svg','path');spine.setAttribute('d','M12 14V61 M10 69H14');spine.classList.add('munition-spine');
    icon.append(shape,spine);
    const name=document.createElement('span');name.className='munition-name';name.textContent=munition.name;
    item.append(icon,name);item.onclick=()=>onSelect(munition.id);grid.append(item);
  }
  panel.append(grid);
  if(!munitions.length){summary.textContent='Mühimmat bilgisi yok.';}
  return panel;
}
