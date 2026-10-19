// Everything by real mouse clicks and typing, on the production build, in windows that are not 1280x720:
// Ann hosts (1600x900), Bob joins (1366x768), Bob opens the pause menu and clicks Resume; Cem plays
// alone (1920x1080), then quits to the title screen from the pause menu.
const { chromium } = require('../lib.js');
const sleep = ms => new Promise(r => setTimeout(r, ms));
const PORT = process.argv[2] || 8124, ROOM = 'clickflow' + Date.now() % 10000;
const dir = __dirname + '/flow';
require('fs').mkdirSync(dir, { recursive: true });

// Each player in a browser of their own, as on separate computers (one mouse to lock per browser).
async function page(unused, W, H, name) {
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  browsers.push(browser);
  const p = await (await browser.newContext({ viewport: { width: W, height: H } })).newPage();
  p.on('pageerror', e => console.log(`[${name}] PAGE ERROR ${e.message}`));
  p.W = W; p.H = H; p.name = name;
  await p.goto(`http://localhost:${PORT}/?room=${ROOM}`);
  return p;
}
// The title screen's panel is 720x490 in the middle of the window.
const lobby = (p, dx, dy) => ({ x: p.W / 2 - 360 + dx, y: (p.H > 546 ? (p.H - 530) / 2 : 8) + dy });
// The pause menu is 620x470 in the middle.
const pause = (p, dx, dy) => ({ x: (p.W - 620) / 2 + dx, y: (p.H - 470) / 2 + dy });
async function click(p, at, what) {
  await p.mouse.move(at.x, at.y, { steps: 3 });
  await sleep(120);
  await p.mouse.down();
  await sleep(90);
  await p.mouse.up();
  console.log(`${p.name}: clicked ${what} at ${Math.round(at.x)},${Math.round(at.y)}`);
}
const browsers = [];
const shot = (p, n) => p.screenshot({ path: `${dir}/${n}.png`, timeout: 90000 });

(async () => {
  const ann = await page(null, 1600, 900, 'Ann');
  const bob = await page(null, 1366, 768, 'Bob');
  await sleep(7000);

  await click(ann, lobby(ann, 40 + 150 + 165, 120 + 19), 'the name field');
  await ann.keyboard.type('Ann', { delay: 60 });
  await sleep(300);
  await shot(ann, '1-ann-named');
  await click(ann, lobby(ann, 40 + 125, 272 + 23), 'Host a game');
  await sleep(9000);
  await shot(ann, '2-ann-hosting');

  await click(bob, lobby(bob, 40 + 150 + 165, 120 + 19), 'the name field');
  await bob.keyboard.type('Bob', { delay: 60 });
  await sleep(300);
  await click(bob, lobby(bob, 40 + 125, 374 + 23), "Join a game");
  await sleep(10000);
  await shot(bob, '3-bob-joined');
  await shot(ann, '4-ann-sees-bob');

  await bob.keyboard.press('Escape');
  await sleep(1200);
  await shot(bob, '5-bob-paused');
  await click(bob, pause(bob, 50 + 120, 470 - 130 + 20), 'Resume');
  await sleep(1500);
  await shot(bob, '6-bob-resumed');

  for (const b of browsers.splice(0)) await b.close();
  const cem = await page(null, 1920, 1080, 'Cem');
  await sleep(7000);
  await click(cem, lobby(cem, 40 + 125, 210 + 23), 'Play alone');
  await sleep(10000);
  await shot(cem, '7-cem-alone');
  await cem.keyboard.press('Escape');
  await sleep(1200);
  await shot(cem, '8-cem-paused');
  await click(cem, pause(cem, 50 + 240 + 40 + 120, 470 - 130 + 54 + 20), 'Save & quit');
  await sleep(2500);
  await shot(cem, '9-cem-title');
  for (const b of browsers) await b.close();
  console.log('done');
})();
