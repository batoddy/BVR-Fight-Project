// Directory selection changes the library, never the currently playing flight.
const el=id=>document.getElementById(id);
const storageKey='bvrgym-recording-directory';
const dateFormat=new Intl.DateTimeFormat('tr-TR',{day:'2-digit',month:'short',year:'numeric',hour:'2-digit',minute:'2-digit'});
export function setupRecordingMenu({open}){
  let busy=false;
  const error=message=>{el('record-error').hidden=!message;el('record-error').textContent=message;};
  async function api(path,payload){
    const response=await fetch(path,payload?{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)}:{});
    const result=await response.json().catch(()=>({}));
    if(!response.ok)throw new Error(result.error||'Dizin açılamadı');
    return result;
  }
  async function refresh(){
    try{
      const entries=await api('/api/recordings');
      const library=el('recording-library');library.replaceChildren();
      for(const entry of entries){
        const row=document.createElement('div');row.className='library-row';
        const button=document.createElement('button');button.className='library-item';
        const content=document.createElement('div'),title=document.createElement('strong'),subtitle=document.createElement('span');
        title.textContent=entry.name;subtitle.textContent=entry.files+' CSV · Klasör kaydı';
        button.dataset.path=entry.path;button.title=entry.path;content.append(title,subtitle);button.append(content);
        if(entry.modified){
          const stamp=document.createElement('time'),date=new Date(entry.modified*1000);
          stamp.className='library-date';stamp.dateTime=date.toISOString();stamp.textContent=dateFormat.format(date);
          button.append(stamp);
        }
        button.onclick=async()=>{
          if(busy)return;busy=true;button.disabled=true;error('');
          try{await api('/api/open',{id:entry.id});el('record-options').open=false;}
          catch(exc){error(exc.message);}finally{busy=false;button.disabled=false;}
        };
        const remove=document.createElement('button');remove.className='library-delete';
        remove.textContent='×';remove.title='Kaydı sil';remove.setAttribute('aria-label',entry.name+' kaydını sil');
        remove.onclick=async()=>{
          if(busy||!confirm(`"${entry.name}" kaydı diskten silinsin mi?\nBu işlem geri alınamaz.`))return;
          busy=true;remove.disabled=true;error('');
          try{await api('/api/delete',{id:entry.id});await refresh();}
          catch(exc){error(exc.message);}finally{busy=false;remove.disabled=false;}
        };
        row.append(button,remove);library.append(row);
      }
      if(!entries.length)library.textContent='Bu dizinde CSV kaydı bulunamadı. Kayıtlar alt klasörlere kaydedildiğinde Yenile ile listeleyebilirsin.';
      filter();
    }catch(exc){error(exc.message);}
  }
  function filter(){const query=el('record-search').value.toLocaleLowerCase('tr').trim();for(const item of el('recording-library').querySelectorAll('.library-row'))item.hidden=Boolean(query&&!item.textContent.toLocaleLowerCase('tr').includes(query));}
  el('change-directory').onclick=async()=>{
    if(busy)return;busy=true;const button=el('change-directory');button.disabled=true;error('');
    try{
      const result=await api('/api/choose-directory',{});
      if(!result.cancelled){
        try{localStorage.setItem(storageKey,result.path);}catch{}
        el('record-source').textContent=result.path;el('record-search').value='';await refresh();
      }
    }catch(exc){error(exc.message);}finally{busy=false;button.disabled=false;}
  };
  el('close-record-panel').onclick=()=>el('record-options').open=false;
  el('refresh-library').onclick=refresh;el('record-search').oninput=filter;
  try{indexedDB.deleteDatabase('bvrgym-recordings');}catch{}
  // Restore only the directory path. CSV contents are never copied into browser storage.
  (async()=>{try{const path=localStorage.getItem(storageKey);if(path)await api('/api/directory',{path});}catch{}await refresh();})();
  return {refresh,error,open};
}
