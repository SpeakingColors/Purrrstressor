import asyncio, json
from playwright.async_api import async_playwright
src=open('docs/mockups/Purrrstressor.html').read()
open('/tmp/purr_wrapped.html','w').write('<!doctype html><html><head><meta charset="utf-8"></head><body>'+src+'</body></html>')
JS = r"""
() => {
  const st=document.querySelector('#stage').getBoundingClientRect();
  const R=e=>{const r=e.getBoundingClientRect();return [Math.round((r.left-st.left)*100)/100,Math.round((r.top-st.top)*100)/100,Math.round(r.width*100)/100,Math.round(r.height*100)/100]};
  const out={};
  const q=(s)=>document.querySelector(s);
  for(const [k,s] of Object.entries({cheekL:'#cheekL',cheekR:'#cheekR',main:'.main',hdr:'.hdr',body:'.body',left:'.left',top:'#top',disp:'.disp',knobs:'#knobs',meters:'.meters',foot:'.foot',logo:'.brand svg',name:'.brand .name',s87:'.brand .s87',pPrev:'#pPrev',pName:'#pName',pNext:'#pNext',pSave:'#pSave',bBypass:'#bBypass',bGuide:'#bGuide',bSettings:'#bSettings',bAbout:'#bAbout',cvCurve:'#cvCurve',cvFilt:'#cvFilt',cvHist:'#cvHist',cvIn:'#cvIn',cvOut:'#cvOut',pkIn:'#pkIn',pkOut:'#pkOut',winsz:'#winsz'})) out[k]=R(q(s));
  out.caps=[...document.querySelectorAll('.dcell .cap')].map(e=>({t:e.textContent,r:R(e)}));
  out.mcap=[...document.querySelectorAll('.meters .cap span')].map(e=>({t:e.textContent,r:R(e)}));
  out.mbtn=[...document.querySelectorAll('.meters button')].map(e=>({t:e.textContent,r:R(e)}));
  out.kcells=[...document.querySelectorAll('#knobs .kcell')].map(c=>({lab:c.querySelector('.lab').textContent,labR:R(c.querySelector('.lab')),knob:c.querySelector('.knob')?R(c.querySelector('.knob')):null,val:c.querySelector('.kval')?R(c.querySelector('.kval')):null,btn:c.querySelector('button')?R(c.querySelector('button')):null,note:[...c.querySelectorAll('.kval')].map(R)}));
  const top=q('#top');const tr=top.getBoundingClientRect();
  out.top=[...top.children].map(e=>{const r=e.getBoundingClientRect();const cs=getComputedStyle(e);return {c:e.className,t:e.textContent,tip:e.dataset.tip||'',aria:e.getAttribute('aria-label')||'',x:Math.round((r.left-tr.left)*100)/100,y:Math.round((r.top-tr.top)*100)/100,w:Math.round(r.width*100)/100,h:Math.round(r.height*100)/100,color:e.style.color||'',fs:cs.fontSize,ff:cs.fontFamily.indexOf('Barlow')>=0?'ui':'mono',fw:cs.fontWeight,ls:cs.letterSpacing,tt:cs.textTransform,tc:cs.color}});
  out.topAbs=R(top);
  return out;
}
"""
async def main():
    async with async_playwright() as p:
        b=await p.chromium.launch();pg=await b.new_page(viewport={'width':1400,'height':900})
        await pg.goto('file:///tmp/purr_wrapped.html');await pg.wait_for_timeout(1200)
        d=await pg.evaluate(JS)
        json.dump(d,open('docs/mockups/layout.json','w'),indent=1)
        await pg.screenshot(path='docs/mockups/mockup_reference.png',clip={'x':0,'y':0,'width':1400,'height':720})
        print(d['top'][:3],len(d['top']),d['kcells'][0]['knob'],d['cheekL'],d['main'])
        await b.close()
asyncio.run(main())
