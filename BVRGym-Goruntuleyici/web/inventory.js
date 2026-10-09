import {munitionState} from './units.js';
export const stateNames={carried:'Uçakta',flying:'Havada',spent:'Atıldı'};

export function inventoryPanel(munitions,time,selected,onSelect){
  const panel=document.createElement('div');panel.className='inventory';
  const states=munitions.map(munition=>munitionState(munition,time));
  const summary=document.createElement('p');summary.className='inventory-summary';
  const count=state=>states.filter(value=>value===state).length;
  const models=[...new Set(munitions.map(munition=>munition.model.split(' ')[0]))].join(', ');
  summary.textContent=(models?models+' · ':'')+count('carried')+' uçakta · '+count('flying')+' havada · '+count('spent')+' atıldı';
  panel.append(summary);
  const grid=document.createElement('div');grid.className='inventory-grid';
  for(const [index,munition] of munitions.entries()){
    const state=states[index],item=document.createElement('button');
    item.className='munition '+state+(selected===munition.id?' selected':'');
    item.dataset.id=munition.id;item.dataset.state=state;item.dataset.kind='missile';item.dataset.team=munition.team;
    item.setAttribute('aria-pressed',String(selected===munition.id));
    item.disabled=!munition.has_track;
    item.title=munition.model+' · '+munition.name+' · '+stateNames[state]+(!munition.has_track?' · Atış ölçümü bulunmuyor':'');
    const name=document.createElement('strong');name.textContent=munition.name;
    const label=document.createElement('span');label.textContent=stateNames[state];
    item.append(name,label);item.onclick=()=>onSelect(munition.id);grid.append(item);
  }
  panel.append(grid);
  if(!munitions.length){summary.textContent='Mühimmat bilgisi yok.';}
  return panel;
}
