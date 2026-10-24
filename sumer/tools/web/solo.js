const { launch, open, info, cmd, waitFor, sleep } = require('./lib.js');
const SHOTS = __dirname + '/shots/';
(async () => {
  const browser = await launch();
  const page = await open(browser, 'solo', 'http://localhost:8123/', true);
  await sleep(3000);
  await page.screenshot({ path: SHOTS + 'lobby.png' });
  const t0 = Date.now();
  await page.goto('http://localhost:8123/?auto=solo&name=Ann&dev=1');
  const s = await waitFor(page, 'solo game', i => i.lobby === 0 && i.role === 0);
  console.log('solo started in', Date.now() - t0, 'ms', JSON.stringify({ x: s.x, z: s.z, day: s.day, hour: s.hour, msg: s.msg }));
  await sleep(4000);
  const fps = await page.evaluate(async () => { let n = 0; const t = performance.now(); await new Promise(r => { function f() { n++; if (performance.now() - t < 3000) requestAnimationFrame(f); else r(); } requestAnimationFrame(f); }); return n / 3; });
  console.log('frames per second (headless software GL):', fps.toFixed(1));
  await page.screenshot({ path: SHOTS + 'solo.png' });

  // The developer tools are gone: the keys that opened the dev console do nothing.
  let failed = 0;
  const check = (ok, what) => { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) failed++; };
  await page.mouse.click(640, 360);
  await sleep(300);
  await page.keyboard.press('Backquote');
  await page.keyboard.press('F2');
  await sleep(600);
  check((await info(page)).panel === 0, 'no dev console: ` and F2 open nothing');
  console.log(failed ? failed + ' FAILED' : 'ALL PASSED');
  await browser.close();
  if (failed) process.exit(1);
})().catch(e => { console.error(e); process.exit(1); });
