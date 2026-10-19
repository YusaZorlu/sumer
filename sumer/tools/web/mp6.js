// Six players in one world (the host and five guests), and a seventh who is turned away. Everyone starts in a
// PvP game, so the starts are spread over the city; a game without PvP puts each at their own door.
const { chromium, info, cmd, waitFor, sleep } = require('./lib.js');
const BASE = 'http://localhost:8123/';
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const ARGS = ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--enable-webgl'];
const NAMES = ['Ann', 'Bob', 'Cem', 'Dee', 'Eve', 'Fay'];

async function open(browser, name, url) {
  const ctx = await browser.newContext({ viewport: { width: 400, height: 225 } });
  const page = await ctx.newPage();
  page.name = name;
  page.on('pageerror', e => { console.log(`[${name}] PAGE ERROR ${e}`); fails++; });
  await page.goto(url);
  return page;
}

(async () => {
  const room = 'six' + Date.now() % 100000;
  const browser = await chromium.launch({ args: ARGS });
  const pages = [];
  pages.push(await open(browser, 'Ann', `${BASE}?auto=host&name=Ann&room=${room}&pvp=0`));
  await waitFor(pages[0], 'Ann hosting', i => i.lobby === 0 && i.role === 1);
  for (let k = 1; k < 6; k++) {
    const p = await open(browser, NAMES[k], `${BASE}?auto=join&name=${NAMES[k]}&room=${room}`);
    pages.push(p);
    await waitFor(p, NAMES[k] + ' joining', i => i.lobby === 0 && i.role === 2, 120000);
  }
  const all = await waitFor(pages[0], 'six players', i => i.players === 6, 60000).catch(() => info(pages[0]));
  check(all.players === 6, `the host sees six players (${all.players})`);
  await sleep(2500);
  const seen = await Promise.all(pages.map(p => info(p)));
  check(seen.every(i => i.players === 6), `everyone sees six players (${seen.map(i => i.players).join(' ')})`);
  // PvP off: each starts at their own door: six different places.
  const spots = seen.map(i => `${i.x.toFixed(0)},${i.z.toFixed(0)}`);
  check(new Set(spots).size === 6, `six different doorsteps: ${spots.join('  ')}`);
  check(seen.every(i => i.peers.length === 6), 'every machine lists all six in its roster');

  // A seventh finds the game full: the relay says so (a seventh page here would only slow the rest down; the
  // game's own refusal text, "The game is full: six players already.", was seen in such a page).
  const answer = await new Promise(done => {
    const ws = new WebSocket(`ws://localhost:8123/ws?role=join&room=${room}`);
    ws.binaryType = 'arraybuffer';
    ws.onmessage = e => { done(new Uint8Array(e.data)[0]); ws.close(); };
    ws.onerror = () => done(-1);
    setTimeout(() => done(-2), 10000);
  });
  check(answer === 4, `a seventh is turned away by the relay (answer ${answer}, 4 means full)`);
  await pages[5].screenshot({ path: __dirname + '/shots/mp6-fay.png' });

  await browser.close();
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(2); });
