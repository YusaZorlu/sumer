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
  // "Play alone" sits at (390, 241) in the UI's units, which are shrunk to 390/540 of a screen pixel.
  const k = 390 / 540;
  await page.touchscreen.tap(390 * k, 241 * k);
  const s = await waitFor(page, 'solo game after a tap', i => i.lobby === 0 && i.role === 0, 60000);
  console.log('tap on Play alone started the game:', JSON.stringify({ x: s.x, z: s.z, gfx: s.gfx }));
  await sleep(5000);
  await page.screenshot({ path: SHOTS + 'phone-game.png', timeout: 180000 });
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
})().catch(e => { console.error(e); process.exit(1); });
