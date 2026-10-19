// With the club in hand: a quick tap that takes the mouse back must not swing; quick taps while
// playing must. The browser is kept from handing the mouse back until the tap (headless Chromium
// would otherwise hand it over at once). Test build on port 8123.
const { chromium } = require('../lib.js');
const sleep = ms => new Promise(r => setTimeout(r, ms));
const PORT = process.argv[2] || 8123, W = 800, H = 450;
(async () => {
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const p = await (await browser.newContext({ viewport: { width: W, height: H } })).newPage();
  p.on('pageerror', e => console.log('PAGE ERROR', e.message));
  p.on('console', m => { if (m.text().startsWith('EV')) console.log(m.text()); });
  await p.addInitScript(() => {
    const t0 = performance.now();
    const log = s => console.log(`EV ${(performance.now() - t0).toFixed(0)} ${s}`);
    document.addEventListener('pointerlockchange', () => log('pointerlockchange locked=' + !!document.pointerLockElement));
    window.addEventListener('mousedown', () => log('mousedown'), true);
  });
  await p.goto(`http://localhost:${PORT}/?room=swing2&gfx=classic`);
  await sleep(7000);
  // Play alone. A window under 960x540 gets the UI shrunk to fit (k), the panel at the top.
  const k = Math.min(1, H / 540, W / 960);
  await p.mouse.click((W / k / 2 - 360 + 165) * k, (8 + 233) * k);
  await sleep(6000);
  const info = () => p.evaluate(() => { const i = JSON.parse(Module.ccall('TestInfo', 'string', [], [])); return { panel: i.panel, captured: i.captured, swing: i.cool, equip: i.equip }; });
  // Before each tap: the last swing's cooldown has run out, so any cooldown seen after the tap is a new swing.
  const settle = async () => { const t0 = Date.now(); while (Date.now() - t0 < 8000 && (await info()).swing > 0) await sleep(50); };
  const watch = async ms => { let most = 0; const t0 = Date.now(); while (Date.now() - t0 < ms) { most = Math.max(most, (await info()).swing); await sleep(25); } return most; };
  const fps = await p.evaluate(() => new Promise(r => { let n = 0; const t0 = performance.now(); const f = () => { n++; if (performance.now() - t0 < 1000) requestAnimationFrame(f); else r(n); }; requestAnimationFrame(f); }));
  console.log('frames a second:', fps);
  await p.keyboard.press('2');
  await sleep(800);
  console.log('club in hand:', JSON.stringify(await info()));

  const inPlay = [];
  for (let k = 0; k < 3; k++) {
    await settle();
    await p.mouse.click(W / 2, H / 2);
    inPlay.push(await watch(1500));
  }
  console.log('quick taps while playing, longest swing after each:', inPlay.join(', '));

  await p.evaluate(() => { Module.canvas.requestPointerLock = function () {}; document.exitPointerLock(); });
  await sleep(1000);
  console.log('mouse let go:', JSON.stringify(await info()));
  await p.keyboard.press('Escape');
  await sleep(1000);
  console.log('resumed, waiting for a click:', JSON.stringify(await info()));
  await settle();
  console.log('before the tap:', JSON.stringify(await info()));
  await p.evaluate(() => { delete Module.canvas.requestPointerLock; });
  await p.mouse.click(W / 2, H / 2);
  const onCapture = await watch(1500);
  console.log('after the tap that takes the mouse back:', JSON.stringify(await info()), 'longest swing', onCapture);
  await settle();
  await p.mouse.click(W / 2, H / 2);
  const after = await watch(1500);
  console.log('the next quick tap: longest swing', after);
  const ok = inPlay.every(s => s > 0) && onCapture === 0 && after > 0;
  console.log(ok ? 'PASS' : 'FAIL');
  await browser.close();
})();
