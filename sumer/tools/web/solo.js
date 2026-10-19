const { launch, open, info, cmd, waitFor, sleep } = require('./lib.js');
const SHOTS = __dirname + '/shots/';
(async () => {
  const browser = await launch();
  const page = await open(browser, 'solo', 'http://localhost:8123/', true);
  await sleep(3000);
  await page.screenshot({ path: SHOTS + 'lobby.png' });
  const t0 = Date.now();
  await page.goto('http://localhost:8123/?auto=solo&name=Ann');
  const s = await waitFor(page, 'solo game', i => i.lobby === 0 && i.role === 0);
  console.log('solo started in', Date.now() - t0, 'ms', JSON.stringify({ x: s.x, z: s.z, day: s.day, hour: s.hour, msg: s.msg }));
  await sleep(4000);
  const fps = await page.evaluate(async () => { let n = 0; const t = performance.now(); await new Promise(r => { function f() { n++; if (performance.now() - t < 3000) requestAnimationFrame(f); else r(); } requestAnimationFrame(f); }); return n / 3; });
  console.log('frames per second (headless software GL):', fps.toFixed(1));
  await page.screenshot({ path: SHOTS + 'solo.png' });
  await browser.close();
})().catch(e => { console.error(e); process.exit(1); });
