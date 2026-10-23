// A phone held sideways (844 x 390, three device pixels to each): the lobby and the game must fit the
// screen, a tap must land on the button under it, and the graphics must pick the phone setting.
// Screenshots go to tools/web/shots/phone-*.png.
const { chromium } = require(process.env.PLAYWRIGHT || 'playwright');
const { info, cmd, waitFor, sleep } = require('./lib.js');
const SHOTS = __dirname + '/shots/';
const BASE = process.env.BASE || 'http://localhost:8123/';
(async () => {
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--enable-webgl'] });
  const ctx = await browser.newContext({
    viewport: { width: 844, height: 390 }, deviceScaleFactor: 3, isMobile: true, hasTouch: true,
    userAgent: 'Mozilla/5.0 (Linux; Android 14; Pixel 8) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Mobile Safari/537.36',
  });
  const page = await ctx.newPage();
  page.on('pageerror', e => console.log('PAGE ERROR', String(e)));
  await page.goto(BASE + '?name=Ann&gfx=auto');
  await sleep(4000);
  await page.screenshot({ path: SHOTS + 'phone-lobby.png', timeout: 180000 });
  // A tap on the name field opens the browser's text box (and with it the phone's keyboard).
  let asked = '';
  page.on('dialog', d => { asked = d.message(); d.accept('Zed'); });
  const uiW = 844 * 540 / 390, k = 390 / 540;
  await page.touchscreen.tap((uiW / 2 - 4) * k, 147 * k);
  await sleep(3000);
  console.log((asked ? 'PASS' : 'FAIL') + `: a tap on the name field asks for the name ("${asked}")`);
  // "Sumer Survival", then "Singleplayer" in its place, sit at (390, 240) in the UI's units, which are shrunk to
  // 390/540 of a screen pixel.
  await page.touchscreen.tap(390 * k, 240 * k);
  await sleep(3000);
  await page.screenshot({ path: SHOTS + 'phone-survival.png', timeout: 180000 });
  await page.touchscreen.tap(390 * k, 240 * k);
  const s = await waitFor(page, 'solo game after a tap', i => i.lobby === 0 && i.role === 0, 60000);
  console.log('tap on Play alone started the game:', JSON.stringify({ x: s.x, z: s.z, gfx: s.gfx }));
  await sleep(5000);
  await page.screenshot({ path: SHOTS + 'phone-game.png', timeout: 180000 });

  // Touch controls, with real touch events (several fingers at once through the DevTools protocol).
  let fails = 0;
  const check = (ok, what) => { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; };
  const cdp = await ctx.newCDPSession(page);
  const touch = (type, pts) => cdp.send('Input.dispatchTouchEvent', { type, touchPoints: pts.map(([x, y, id]) => ({ x, y, id })) });
  const tap = async (b) => { const i = await info(page); await page.touchscreen.tap(i.buttons[b][0], i.buttons[b][1]); };
  const ATTACK = 0, PARRY = 1, USE = 2, CROUCH = 3, VIEW = 4, BAG = 5, MENU = 6, JUMP = 7, PREV = 8, NEXT = 9;
  if (!asked) fails++;
  check((await info(page)).peers.some(p => p.name === 'Zed'), 'the name typed in the box is the player\'s');
  // A new game opens "Your look" first: Close puts it away.
  await tap(MENU);
  let i0 = await waitFor(page, 'the look panel closed', i => i.panel === 0, 30000).catch(e => null) || await info(page);
  await cmd(page, 1, 40, 10);   // open ground, nothing in the way
  await sleep(1500);
  i0 = await info(page);
  check(i0.touch === 1 && i0.buttons.every(b => b[0] > 0), `touch controls shown (${JSON.stringify(i0.buttons)})`);

  await touch('touchStart', [[120, 300, 1]]);
  await touch('touchMove', [[120, 350, 1]]);   // pulled back: walks backward, away from the wall north of (40, 10)
  let i1 = await waitFor(page, 'walking', i => Math.hypot(i.x - i0.x, i.z - i0.z) > 2, 60000).catch(e => null);
  // A second finger looks around while the thumb still walks.
  const yaw0 = (await info(page)).yaw;
  await touch('touchMove', [[120, 350, 1], [560, 120, 2]]);
  await touch('touchMove', [[120, 350, 1], [480, 120, 2]]);
  await sleep(1500);
  const i2 = await info(page);
  await touch('touchEnd', []);
  check(!!i1, 'the stick walks');
  check(i2.panel === 0 && i2.captured === 0, `a phone never takes the pointer, so touching never pauses (panel ${i2.panel}, captured ${i2.captured})`);
  check(Math.abs(i2.yaw - yaw0) > 0.2, `a drag on the right turns the view while walking (yaw ${yaw0.toFixed(2)} -> ${i2.yaw.toFixed(2)})`);

  const fp = (await info(page)).fp;
  await tap(VIEW);
  check(!!(await waitFor(page, 'view switched', i => i.fp !== fp, 20000).catch(e => null)), 'View switches first person / behind you');
  await tap(VIEW);
  await tap(CROUCH);
  check(!!(await waitFor(page, 'crouching', i => i.crouch === 1, 20000).catch(e => null)), 'Crouch crouches');
  await tap(CROUCH);
  await waitFor(page, 'standing', i => i.crouch === 0, 20000).catch(e => null);

  const y0 = (await info(page)).y;
  await tap(JUMP);
  check(!!(await waitFor(page, 'a jump', i => i.y > y0 + 0.2, 20000).catch(e => null)), 'Jump jumps');

  await cmd(page, 14, 1);
  await sleep(500);
  await tap(NEXT);
  check(!!(await waitFor(page, 'the next weapon', i => i.equip === 2, 20000).catch(e => null)), 'the next weapon after the bow is the melee weapon');
  await tap(PREV);
  check(!!(await waitFor(page, 'the previous weapon', i => i.equip === 1, 20000).catch(e => null)), 'and the previous one the bow again');

  await cmd(page, 14, 2);
  await sleep(1000);
  await page.touchscreen.tap(560, 150);   // a tap where there is no button: looking, never a blow
  await sleep(2000);
  const stray = await info(page);
  check(stray.swing <= 0 && stray.cool <= 0, `a tap off the buttons does not strike (swing ${stray.swing}, cool ${stray.cool})`);
  await tap(ATTACK);
  check(!!(await waitFor(page, 'a blow', i => i.swing > 0 || i.cool > 0, 20000).catch(e => null)), 'Attack strikes with the melee weapon');
  await waitFor(page, 'the blow over', i => i.swing <= 0, 20000).catch(e => null);
  await tap(PARRY);
  check(!!(await waitFor(page, 'a parry', i => i.parry > 0, 20000).catch(e => null)), 'Parry raises the guard');

  // The bow: held to draw, let go to shoot.
  await cmd(page, 14, 1);
  await sleep(1500);
  const q0 = (await info(page)).quiver;
  const a = (await info(page)).buttons[ATTACK];
  await touch('touchStart', [[a[0], a[1], 3]]);
  await waitFor(page, 'the bow drawn', i => i.aim === 1, 20000).catch(e => null);
  const aimYaw = (await info(page)).yaw;
  await touch('touchMove', [[a[0] - 40, a[1], 3]]);
  await touch('touchMove', [[a[0] - 80, a[1], 3]]);
  const aimed = await waitFor(page, 'aiming', i => Math.abs(i.yaw - aimYaw) > 0.1, 20000).catch(e => null);
  check(!!aimed && aimed.aim === 1, `sliding the finger on Attack aims the drawn bow (yaw ${aimYaw.toFixed(2)} -> ${aimed ? aimed.yaw.toFixed(2) : '?'})`);
  await sleep(1500);
  await touch('touchEnd', []);
  check(!!(await waitFor(page, 'a shot', i => i.quiver === q0 - 1, 20000).catch(e => null)), `holding Attack draws the bow and letting go shoots (quiver ${q0})`);

  // Use: sow the first plot.
  await cmd(page, 7, 3, 4);
  await cmd(page, 1, 29.6, 18.6);
  await waitFor(page, 'at the plot', i => i.target === 4, 20000).catch(e => null);
  await tap(USE);
  check(!!(await waitFor(page, 'sown', i => i.plots[0] === 1, 20000).catch(e => null)), 'Use sows the plot under you');

  await tap(MENU);
  let m = await waitFor(page, 'the menu', i => i.panel === 4, 20000).catch(e => null);
  check(!!m && m.buttons[ATTACK][0] < 0 && m.buttons[MENU][0] > 0, 'Menu opens the pause menu, and only its Close button stays');
  await page.screenshot({ path: SHOTS + 'phone-touch-menu.png', timeout: 180000 });
  await tap(MENU);
  check(!!(await waitFor(page, 'closed', i => i.panel === 0, 20000).catch(e => null)), 'Close closes it');
  await tap(BAG);
  check(!!(await waitFor(page, 'the baskets', i => i.panel === 1, 20000).catch(e => null)), 'Bag opens the baskets');
  await tap(MENU);
  await waitFor(page, 'closed', i => i.panel === 0, 20000).catch(e => null);
  check((await info(page)).fpsLimit === 60, 'frames are limited to 60 a second unless the settings say otherwise');

  // Left-handed: the thumb's buttons on the left, the stick on the right half.
  await cmd(page, 19, 1);
  await sleep(1000);
  let lh = await info(page);
  check(lh.buttons[ATTACK][0] < 422 && lh.buttons[JUMP][0] < 422 && lh.buttons[BAG][0] > 300, `left-handed, the buttons change sides (Attack at x ${lh.buttons[ATTACK][0]})`);
  await touch('touchStart', [[720, 300, 5]]);
  await touch('touchMove', [[720, 350, 5]]);
  const lw = await waitFor(page, 'walking left-handed', i => Math.hypot(i.x - lh.x, i.z - lh.z) > 1, 60000).catch(e => null);
  await touch('touchEnd', []);
  check(!!lw, 'and the stick works on the right half');
  await page.screenshot({ path: SHOTS + 'phone-touch-left.png', timeout: 180000 });
  await cmd(page, 19, 0);
  await sleep(1000);

  await touch('touchStart', [[120, 300, 4]]);
  await touch('touchMove', [[150, 270, 4]]);
  await sleep(1500);
  await page.screenshot({ path: SHOTS + 'phone-touch.png', timeout: 180000 });
  await touch('touchEnd', []);
  await cmd(page, 15, 4);
  await sleep(2500);
  await page.screenshot({ path: SHOTS + 'phone-pause.png', timeout: 180000 });
  await cmd(page, 15, 5);
  await sleep(2500);
  await page.screenshot({ path: SHOTS + 'phone-help.png', timeout: 180000 });
  await cmd(page, 15, 1);
  await sleep(2500);
  await page.screenshot({ path: SHOTS + 'phone-bag.png', timeout: 180000 });
  await browser.close();
  console.log(fails ? fails + ' FAILED' : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
