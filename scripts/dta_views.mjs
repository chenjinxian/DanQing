// CDP 驱动 DTA：经真实 UI（Standard rotations 面板）逐视图截图——地面真值采集
import { writeFileSync } from 'node:fs';

const CDP = 9223;
const outDir = 'D:/Github/DanQing/build/gui-check';

const list = await (await fetch(`http://127.0.0.1:${CDP}/json`)).json();
const page = list.find(t => t.type === 'page');
if (!page) throw new Error('no page target');
console.log('target:', page.url);

const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });

let msgId = 0;
const pending = new Map();
ws.onmessage = (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
};
function send(method, params = {}) {
  return new Promise((res) => { const id = ++msgId; pending.set(id, res); ws.send(JSON.stringify({ id, method, params })); });
}
async function evalJS(expression, awaitPromise = false) {
  const r = await send('Runtime.evaluate', { expression, awaitPromise, returnByValue: true });
  if (r.result?.exceptionDetails) throw new Error(JSON.stringify(r.result.exceptionDetails).slice(0, 400));
  return r.result?.result?.value;
}

// 打开 Standard rotations 下拉（title 匹配）
const opened = await evalJS(`(() => {
  const b = [...document.querySelectorAll('[title]')].find(e => e.title === 'Standard rotations');
  if (!b) return 'no dropdown; titles=' + [...document.querySelectorAll('[title]')].map(e => e.title).slice(0, 20).join('|');
  { const s = b.querySelector("span"); (s || b).click(); return "opened"; }
})()`);
console.log('dropdown:', opened.slice(0, 300));
await new Promise(r => setTimeout(r, 800));

// 面板内 8 个字形按钮（.toolMenu .simpleicon，DOM 序 = entries 序）
const names = ['Top','Bottom','Left','Right','Front','Back','Iso','RightIso'];
const btnCount = await evalJS(`document.querySelectorAll('.toolMenu .simpleicon').length`);
console.log('panel buttons:', btnCount);
if (btnCount < 8) throw new Error('panel not open');

for (let i = 0; i < 8; ++i) {
  // 每次点击后面板关闭，需重开
  await evalJS(`(() => {
    const b = [...document.querySelectorAll('[title]')].find(e => e.title === 'Standard rotations');
    if (b) { const s = b.querySelector("span"); (s || b).click(); }
  })()`);
  await new Promise(r => setTimeout(r, 500));
  const clicked = await evalJS(`(() => {
    const btns = document.querySelectorAll('.toolMenu .simpleicon');
    if (!btns[${i}]) return 'missing';
    btns[${i}].click(); return 'ok';
  })()`);
  if (clicked !== 'ok') throw new Error(`button ${i}: ${clicked}`);
  await new Promise(r => setTimeout(r, 1500));
  const shot = await send('Page.captureScreenshot', { format: 'png' });
  writeFileSync(`${outDir}/dta-std-${names[i]}.png`, Buffer.from(shot.result.data, 'base64'));
  console.log('saved', names[i]);
}
ws.close();
