// Directory selection changes the library, never the currently playing flight.
const el=id=>document.getElementById(id);
const storageKey='bvrgym-recording-directory';
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
        const button=document.createElement('button');button.className='library-item';
        const content=document.createElement('div'),title=document.createElement('strong'),subtitle=document.createElement('span');
        title.textContent=entry.name;subtitle.textContent=entry.files+' CSV · Klasör kaydı';
        button.dataset.path=entry.path;button.title=entry.path;content.append(title,subtitle);button.append(content);
        button.onclick=async()=>{
          if(busy)return;busy=true;button.disabled=true;error('');
          try{await api('/api/open',{id:entry.id});el('record-options').open=false;}
          catch(exc){error(exc.message);}finally{busy=false;button.disabled=false;}
        };
        library.append(button);
      }
      if(!entries.length)library.textContent='Bu dizinde CSV kaydı bulunamadı. Kayıtlar alt klasörlere kaydedildiğinde Yenile ile listeleyebilirsin.';
      filter();
    }catch(exc){error(exc.message);}
  }
  function filter(){const query=el('record-search').value.toLocaleLowerCase('tr').trim();for(const item of el('recording-library').querySelectorAll('.library-item'))item.hidden=Boolean(query&&!item.textContent.toLocaleLowerCase('tr').includes(query));}
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
