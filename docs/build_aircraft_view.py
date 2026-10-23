"""Wrap the exported Mermaid SVG in a small, offline pan/zoom viewer."""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parent
SVG = ROOT / "mimari/AIRCRAFT_UML.svg"
SOURCE = ROOT / "mimari/AIRCRAFT_UML.mmd"
DESTINATION = ROOT / "AIRCRAFT_UML.html"

CSS = """
:root{font-family:'Segoe UI',Arial,sans-serif;color:#1c304b;background:#f4f7fb;color-scheme:light}*{box-sizing:border-box}body{margin:0;height:100dvh;display:flex;flex-direction:column;overflow:hidden}button,select,a{font:inherit}button,select,.download{border:1px solid #cbd6e5;border-radius:7px;background:white;color:#243e60;padding:7px 11px;font-size:13px}button{cursor:pointer}button:hover,.download:hover{background:#eaf1fc}button:focus-visible,select:focus-visible,a:focus-visible,#stage:focus-visible{outline:3px solid #528cdf;outline-offset:2px}.toolbar{background:#fff;border-bottom:1px solid #dce4ee;padding:14px 20px;display:flex;align-items:center;gap:18px;flex-wrap:wrap;z-index:2}.title{margin-right:auto}.title h1{font-size:18px;line-height:1.2;margin:0 0 4px}.title p{font-size:11px;color:#65778d;margin:0}.controls{display:flex;align-items:center;gap:6px;flex-wrap:wrap}.controls label{font-size:12px;color:#536b87}select{min-width:155px}.download{text-decoration:none;white-space:nowrap}.source{font-size:12px;color:#3e6392;text-underline-offset:3px}.zoom-label{min-width:43px;font-size:12px;text-align:center;font-variant-numeric:tabular-nums;color:#526b88}#stage{flex:1;min-height:0;position:relative;overflow:hidden;cursor:grab;background-color:#fafcfe;background-image:radial-gradient(#cfdae7 .8px,transparent .8px);background-size:20px 20px;touch-action:none;outline-offset:-3px}#stage.dragging{cursor:grabbing}#stage>svg{display:block;width:100%;height:100%;max-width:none!important;user-select:none;touch-action:none}#stage g.node{cursor:pointer}#stage g.node.selected rect{stroke:#235fc3!important;stroke-width:3px!important;fill:#edf4ff!important}#stage path.relation.connected{stroke:#245fae!important;stroke-width:2.6px!important}.statusbar{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;border-top:1px solid #dce4ee;background:white;padding:9px 20px;font-size:11px;color:#65778d}.statusbar p{margin:0}.selection{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.selection strong{color:#244a7b}.selection button{font-size:11px;padding:3px 7px}.selection .relation-hint{color:#7a8ba0;font-size:10px}.legend{font-size:11px;color:#526b88;margin-left:auto}.legend span{margin-left:10px}#stage text{font-synthesis:none}#stage:focus{outline-offset:-3px}
@media(max-width:760px){.toolbar{padding:11px 12px;gap:10px}.title{width:100%;margin:0}.title h1{font-size:16px}.controls{gap:5px}.controls label{display:none}button,select,.download{padding:7px 9px;font-size:12px}select{min-width:140px}.statusbar{padding:8px 12px;gap:6px}.legend{margin-left:0}.source{font-size:11px}.title p{font-size:10px}}
@media print{body{height:auto;overflow:visible;background:white}.toolbar,.statusbar{display:none}#stage{height:auto;overflow:visible;background:white}#stage>svg{width:100%;height:auto}}
"""

SCRIPT = r"""
(()=>{
 'use strict';
 const stage=document.getElementById('stage'),svg=stage.querySelector('svg'),picker=document.getElementById('class-picker');
 const nodes=[...svg.querySelectorAll('g[id^="classId-"]')],edges=[...svg.querySelectorAll('path.relation')];
 const base=svg.getAttribute('viewBox').split(/[ ,]+/).map(Number);
 let view={x:base[0],y:base[1],w:base[2],h:base[3]},selected='',mode='focus';
 const byName=new Map(),pointers=new Map();let gesture=null,moved=false,candidate='';
 nodes.forEach(node=>{const name=node.id.replace(/^classId-/,'').replace(/-\d+$/,'');node.dataset.name=name;byName.set(name,node);const option=document.createElement('option');option.value=name;option.textContent=name;picker.append(option);});
 const relations=__RELATIONS__;
 function apply(){svg.setAttribute('viewBox',[view.x,view.y,view.w,view.h].join(' '));document.getElementById('zoom-label').textContent=Math.round(base[2]/view.w*100)+'%';}
 function fitRect(rect,padding=40){const aspect=stage.clientWidth/stage.clientHeight;let w=rect.w+padding*2,h=rect.h+padding*2;if(w/h<aspect)w=h*aspect;else h=w/aspect;view={x:rect.x+rect.w/2-w/2,y:rect.y+rect.h/2-h/2,w,h};apply();}
 function fit(){mode='all';fitRect({x:base[0],y:base[1],w:base[2],h:base[3]},25);}
 function point(x,y){const p=svg.createSVGPoint();p.x=x;p.y=y;return p.matrixTransform(svg.getScreenCTM().inverse());}
 function center(){const r=stage.getBoundingClientRect();return {x:r.left+r.width/2,y:r.top+r.height/2};}
 function zoom(factor,x,y){if(x===undefined){const c=center();x=c.x;y=c.y;}const anchor=point(x,y),w=Math.max(base[2]/18,Math.min(base[2]*5,view.w/factor)),ratio=w/view.w;view.x=anchor.x-(anchor.x-view.x)*ratio;view.y=anchor.y-(anchor.y-view.y)*ratio;view.w=w;view.h*=ratio;mode='manual';apply();}
 function pan(dx,dy){const m=svg.getScreenCTM();view.x-=dx/m.a;view.y-=dy/m.d;mode='manual';apply();}
 function bounds(node){const b=node.getBBox(),m=svg.getScreenCTM().inverse().multiply(node.getScreenCTM());const p=svg.createSVGPoint();p.x=b.x;p.y=b.y;const first=p.matrixTransform(m);p.x=b.x+b.width;p.y=b.y+b.height;const last=p.matrixTransform(m);return {x:first.x,y:first.y,w:last.x-first.x,h:last.y-first.y};}
 function select(name){
  selected=name;picker.value=name;nodes.forEach(n=>n.classList.toggle('selected',n.dataset.name===name));
  edges.forEach(edge=>edge.classList.toggle('connected',relations.some((r,index)=>edge.id.endsWith('_'+(index+1))&&(r.from===name||r.to===name))));
  const panel=document.getElementById('selection');panel.replaceChildren();
  if(!name)return;
  const title=document.createElement('strong');title.textContent=name;panel.append(title);
  const related=relations.filter(r=>r.from===name||r.to===name),seen=new Set();
  for(const r of related){const other=r.from===name?r.to:r.from;if(seen.has(other))continue;seen.add(other);const button=document.createElement('button');button.type='button';button.textContent=other;button.title=r.label;button.onclick=()=>focus(other);panel.append(button);}
 }
 function focus(name){if(!byName.has(name)){select('');fit();return;}select(name);const b=bounds(byName.get(name));mode='focus';fitRect({x:b.x-100,y:b.y-70,w:b.w+200,h:b.h+140},60);}
 picker.onchange=()=>focus(picker.value);
 document.getElementById('fit').onclick=()=>{select('');fit();};
 document.getElementById('zoom-in').onclick=()=>zoom(1.3);document.getElementById('zoom-out').onclick=()=>zoom(1/1.3);
 document.getElementById('reset').onclick=()=>focus('Aircraft');
 document.getElementById('fullscreen').onclick=async()=>{try{if(document.fullscreenElement)await document.exitFullscreen();else await document.documentElement.requestFullscreen();}catch{document.getElementById('help').textContent='Tarayıcı tam ekranı için F11 tuşunu kullanabilirsin.';}};
 stage.addEventListener('wheel',e=>{e.preventDefault();zoom(Math.exp(-Math.max(-150,Math.min(150,e.deltaY))*.003),e.clientX,e.clientY);},{passive:false});
 function snapshot(){const points=[...pointers.values()];return points.length>1?{x:(points[0].x+points[1].x)/2,y:(points[0].y+points[1].y)/2,d:Math.hypot(points[0].x-points[1].x,points[0].y-points[1].y)}:points[0]||null;}
 stage.addEventListener('pointerdown',e=>{if(e.button!==0&&e.pointerType==='mouse')return;e.preventDefault();if(!pointers.size){moved=false;candidate=e.target.closest('g[id^="classId-"]')?.dataset.name||'';}else moved=true;pointers.set(e.pointerId,{x:e.clientX,y:e.clientY});gesture=snapshot();stage.setPointerCapture(e.pointerId);stage.classList.add('dragging');stage.focus({preventScroll:true});});
 stage.addEventListener('pointermove',e=>{if(!pointers.has(e.pointerId))return;pointers.set(e.pointerId,{x:e.clientX,y:e.clientY});const next=snapshot();if(gesture){const dx=next.x-gesture.x,dy=next.y-gesture.y;if(Math.abs(dx)+Math.abs(dy)>1)moved=true;if(next.d&&gesture.d&&gesture.d>0)zoom(next.d/gesture.d,gesture.x,gesture.y);pan(dx,dy);}gesture=next;});
 function release(e){if(!pointers.has(e.pointerId))return;const click=pointers.size===1&&!moved&&candidate&&e.type==='pointerup';pointers.delete(e.pointerId);gesture=snapshot();if(!pointers.size)stage.classList.remove('dragging');if(click)select(candidate);}
 stage.addEventListener('pointerup',release);stage.addEventListener('pointercancel',release);stage.addEventListener('lostpointercapture',release);
 stage.addEventListener('dblclick',e=>{const name=e.target.closest('g[id^="classId-"]')?.dataset.name;if(name)focus(name);else zoom(1.5,e.clientX,e.clientY);});
 stage.addEventListener('keydown',e=>{const step=45;if(e.key==='+'||e.key==='=')zoom(1.3);else if(e.key==='-')zoom(1/1.3);else if(e.key==='0')fit();else if(e.key==='ArrowLeft')pan(step,0);else if(e.key==='ArrowRight')pan(-step,0);else if(e.key==='ArrowUp')pan(0,step);else if(e.key==='ArrowDown')pan(0,-step);else if(e.key==='Escape')select('');else return;e.preventDefault();});
 let lastWidth=stage.clientWidth,lastHeight=stage.clientHeight;
 new ResizeObserver(()=>{if(stage.clientWidth===lastWidth&&stage.clientHeight===lastHeight)return;lastWidth=stage.clientWidth;lastHeight=stage.clientHeight;if(mode==='all')fit();else if(mode==='focus')focus(selected||'Aircraft');else{const cy=view.y+view.h/2;view.h=view.w*stage.clientHeight/stage.clientWidth;view.y=cy-view.h/2;apply();}}).observe(stage);
 svg.removeAttribute('style');svg.setAttribute('preserveAspectRatio','xMidYMid meet');focus('Aircraft');
})();
"""


def build():
    svg = SVG.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    relations = []
    for line in source.splitlines():
        match = re.match(r'^\s*(\w+)(?:\s+"[^"]+")?\s+(\*--|\.\.>|-->)(?:\s+"[^"]+")?\s+(\w+)\s*:\s*(.+)$', line)
        if match:
            own, kind, target, label = match.groups()
            relations.append(dict(from_=own, to=target, label=label, kind=kind))
    relations = [{"from": r.pop("from_"), **r} for r in relations]
    if len(relations) != len(re.findall(r'class="[^"]*\brelation\b', svg)):
        raise ValueError("Mermaid source and SVG relations are out of sync")
    script = SCRIPT.replace("__RELATIONS__", json.dumps(relations, ensure_ascii=False))
    page = ('<!doctype html><html lang="tr"><head><meta charset="utf-8">'
            '<meta name="viewport" content="width=device-width,initial-scale=1">'
            '<title>Aircraft UML · Gezilebilir diyagram</title><style>'+CSS+'</style></head><body>'
            '<header class="toolbar"><div class="title"><h1>Uçak, füze ve geometri</h1>'
            '<p>Mermaid UML · World → Aircraft · 14 sınıf · çevrimdışı</p></div>'
            '<div class="controls"><label for="class-picker">Sınıfa git</label>'
            '<select id="class-picker"><option value="">Sınıf seç…</option></select>'
            '<button id="zoom-out" aria-label="Küçült">−</button><span class="zoom-label" id="zoom-label"></span>'
            '<button id="zoom-in" aria-label="Büyüt">+</button><button id="fit">Tümünü gör</button>'
            '<button id="reset">Aircraft</button><button id="fullscreen">Tam ekran</button>'
            '<a class="download" href="mimari/AIRCRAFT_UML.svg" download>SVG indir ↓</a>'
            '<a class="source" href="mimari/AIRCRAFT_UML.mmd" download>Mermaid kaynağı</a></div></header>'
            '<main id="stage" tabindex="0" aria-label="UML diyagramı. Sürükleyerek gez; tekerlek veya artı eksi tuşlarıyla yakınlaş; sıfır tuşuyla tamamını gör.">'+svg+'</main>'
            '<footer class="statusbar"><div class="selection" id="selection" aria-live="polite"></div>'
            '<p id="help">Sürükle: gezin · Tekerlek / iki parmak: yakınlaş · Çift tık: sınıfa odaklan</p>'
            '<div class="legend"><span>◆ sahiplik</span><span>⇢ kullanım</span><span>→ hedef ilişkisi</span></div>'
            '</footer><script>'+script+'</script></body></html>')
    DESTINATION.write_text(page, encoding="utf-8")
    print(f"Built {DESTINATION.relative_to(ROOT.parent)}: {len(page.encode('utf-8')):,} bytes, {len(relations)} relations")


if __name__ == "__main__":
    build()
