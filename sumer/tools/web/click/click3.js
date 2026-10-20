// A browser window that is not 1280x720: hover the language button and hold a click on it.
const { chromium } = require('../lib.js');
const sleep = ms => new Promise(r => setTimeout(r, ms));
const W = +process.argv[2] || 1600, H = +process.argv[3] || 900, PORT = process.argv[4] || 8124;
(async () => {
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const page = await (await browser.newContext({ viewport: { width: W, height: H } })).newPage();
  page.on('pageerror', e => console.log('PAGE ERROR', e.message));
  await page.goto(`http://localhost:${PORT}/?gfx=classic`);
  await sleep(6000);
  // The panel is 720x490, centered; the language button sits 214 px in from its right edge, 12 down.
  const px = W / 2 - 360, py = H > 546 ? (H - 530) / 2 : 8;
  const bx = px + 720 - 214 + 100, by = py + 12 + 13;
  await page.mouse.move(bx, by, { steps: 4 });
  await sleep(500);
  await page.screenshot({ path: `${__dirname}/hover-${W}x${H}.png` });
  await page.mouse.down();
  await sleep(150);
  await page.mouse.up();
  await sleep(1500);
  await page.screenshot({ path: `${__dirname}/click-${W}x${H}.png` });
  console.log(`clicked at ${bx},${by} in a ${W}x${H} window`);
  await browser.close();
})();
