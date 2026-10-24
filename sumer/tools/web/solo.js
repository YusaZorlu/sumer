// Singleplayer: it starts once logged in; a beast brought down is worth experience, which goes to the account;
// a save goes to the account too, so the same account in another browser plays on from it. The developer
// tools are gone.
const { BASE, RUN, launch, open, info, cmd, waitFor, sleep } = require('./lib.js');
const SHOTS = __dirname + '/shots/';
const NAME = 'Solo' + RUN;
let failed = 0;
const check = (ok, what) => { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) failed++; };
const me = async page => (await page.evaluate(() => fetch('/api/me').then(r => r.text()))).trim().split('\t');

(async () => {
  const browser = await launch();
  const page = await open(browser, NAME, BASE, true);
  await sleep(3000);
  await page.screenshot({ path: SHOTS + 'lobby.png' });
  const t0 = Date.now();
  await page.goto(BASE + '?auto=solo');
  let s = await waitFor(page, 'solo game', i => i.lobby === 0 && i.role === 0 && i.day > 0);
  console.log('solo started in', Date.now() - t0, 'ms', JSON.stringify({ x: s.x, z: s.z, day: s.day, hour: s.hour, msg: s.msg }));
  await sleep(4000);
  const fps = await page.evaluate(async () => { let n = 0; const t = performance.now(); await new Promise(r => { function f() { n++; if (performance.now() - t < 3000) requestAnimationFrame(f); else r(); } requestAnimationFrame(f); }); return n / 3; });
  console.log('frames per second (headless software GL):', fps.toFixed(1));
  await page.screenshot({ path: SHOTS + 'solo.png' });

  // The developer tools are gone: the keys that opened the dev console do nothing.
  await page.mouse.click(640, 360);
  await sleep(300);
  await page.keyboard.press('Backquote');
  await page.keyboard.press('F2');
  await sleep(600);
  check((await info(page)).panel === 0, 'no dev console: ` and F2 open nothing');

  // A partridge or a goose brought down with the club: 1 experience, shown at once and sent to the account.
  s = await info(page);
  const prey = s.animals.findIndex(a => (a[0] === 6 || a[0] === 3) && a[1] === 1 && a[2] === 0);
  const xp0 = s.xp;
  await cmd(page, 14, 2);
  for (let k = 0; k < 12 && prey >= 0; k++) {
    s = await info(page);
    const a = s.animals[prey];
    if (a[2] === 3 || !a[1]) break;
    await cmd(page, 1, a[3] + 1.2, a[4]);
    await cmd(page, 4, Math.atan2(-(a[3] - (a[3] + 1.2)), -(a[4] - a[4])), 0.4);
    await cmd(page, 6);
    await sleep(700);
  }
  s = await waitFor(page, 'experience for the kill', i => i.xp === xp0 + 1, 20000).catch(e => null);
  check(!!s, `a beast brought down alone is worth 1 experience (${xp0} -> ${s ? s.xp : (await info(page)).xp})`);
  await page.screenshot({ path: SHOTS + 'solo-xp.png' });
  await sleep(12000);
  const kept = await me(page);
  check(+kept[1] === xp0 + 1, `the experience reaches the account within a few seconds (${kept.join(' ')})`);

  // F5 saves; the save goes to the account, and another browser logged in to it plays on from there.
  await cmd(page, 9, 15.5);
  await page.keyboard.press('F5');
  await sleep(4000);
  const saves = await page.evaluate(() => fetch('/api/saves').then(r => r.arrayBuffer()).then(b => b.byteLength));
  check(saves > 200, `the save is kept with the account (${saves} bytes for the three slots)`);
  const day = (await info(page)).day;
  const other = await open(browser, NAME, BASE + '?auto=solo');
  s = await waitFor(other, 'the other browser playing', i => i.lobby === 0 && i.role === 0 && i.day > 0, 120000).catch(e => null);
  check(!!s && s.day === day && /Welcome back/.test(s.msg) && Math.abs(s.hour - 15.5) < 0.5, `another browser plays on from the account's save: day ${s ? s.day : '?'}, ${s ? s.hour.toFixed(1) : '?'} h: "${s ? s.msg : ''}"`);

  console.log(failed ? failed + ' FAILED' : 'ALL PASSED');
  await browser.close();
  if (failed) process.exit(1);
})().catch(e => { console.error(e); process.exit(1); });
