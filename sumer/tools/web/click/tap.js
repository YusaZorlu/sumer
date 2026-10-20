// Instant taps (down and up in the same moment, like a touchpad tap) at 1600x900.
const { chromium } = require('../lib.js');
const sleep = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const W = 1600, H = 900;
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const page = await (await browser.newContext({ viewport: { width: W, height: H } })).newPage();
  page.on('pageerror', e => console.log('PAGE ERROR', e.message));
  await page.goto('http://localhost:8124/?gfx=classic');
  await sleep(6000);
  const px = W / 2 - 360, py = H > 546 ? (H - 530) / 2 : 8;
  await page.mouse.click(px + 720 - 114, py + 25);           // language: instant tap
  await sleep(1500);
  await page.screenshot({ path: __dirname + '/tap-1-language.png' });
  await page.mouse.click(px + 40 + 125, py + 120 + 50 + 40 + 23); // Play alone: instant tap
  await sleep(12000);
  await page.screenshot({ path: __dirname + '/tap-2-playing.png' });
  await browser.close();
})();
