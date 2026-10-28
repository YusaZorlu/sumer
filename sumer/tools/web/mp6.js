// Six players in one Survival server, and a seventh who is turned away. Each starts at their own door.
const { BASE, RUN, chromium, login, info, waitFor, waitHost, sleep } = require('./lib.js');
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const ARGS = ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--enable-webgl'];
const NAMES = ['Ann', 'Bob', 'Cem', 'Dee', 'Eve', 'Fay'].map(n => n + RUN);
const ROOM = 'survival-six' + RUN;

async function open(browser, name, url) {
  const ctx = await browser.newContext({ viewport: { width: 400, height: 225 } });
  await login(ctx, name);
  const page = await ctx.newPage();
  page.name = name;
  page.on('pageerror', e => { console.log(`[${name}] PAGE ERROR ${e}`); fails++; });
  await page.goto(url);
  return page;
}

(async () => {
  const browser = await chromium.launch({ args: ARGS });
  const pages = [];
  for (let k = 0; k < 6; k++) {
    const p = await open(browser, NAMES[k], `${BASE}?auto=join&room=${ROOM}`);
    pages.push(p);
    // Every page already open slows the next one down on the test machine: the sixth takes minutes.
    await waitFor(p, NAMES[k] + ' joining', i => i.lobby === 0 && i.role === 2, 300000);
  }
  const all = await waitHost(ROOM, 'six players', i => i.players === 6, 60000).catch(() => null);
  check(!!all, `the server has six players (${all ? all.players : '?'})`);
  // Each page hears of the last one in on its next frames: a slow page needs a while.
  const seen = await Promise.all(pages.map(p => waitFor(p, 'six players', i => i.players === 6 && i.peers.length === 6, 60000).catch(() => info(p))));
  check(seen.every(i => i.players === 6), `everyone sees six players (${seen.map(i => i.players).join(' ')})`);
  const spots = seen.map(i => `${i.x.toFixed(0)},${i.z.toFixed(0)}`);
  check(new Set(spots).size === 6, `six different doorsteps: ${spots.join('  ')}`);
  check(seen.every(i => i.peers.length === 6), 'every machine lists all six in its roster');

  // A seventh finds the game full: the server says so before the game would (S_FULL, 4).
  const ctx = await browser.newContext();
  await login(ctx, 'Gus' + RUN);
  const seventh = await ctx.newPage();
  await seventh.goto(BASE + 'api/me');
  const answer = await seventh.evaluate(room => new Promise(done => {
    const ws = new WebSocket(`ws://${location.host}/ws?room=${room}`);
    ws.binaryType = 'arraybuffer';
    ws.onmessage = e => { done(new Uint8Array(e.data)[0]); ws.close(); };
    ws.onerror = () => done(-1);
    setTimeout(() => done(-2), 10000);
  }), ROOM);
  check(answer === 4, `a seventh is turned away (answer ${answer}, 4 means full)`);
  await pages[5].screenshot({ path: __dirname + '/shots/mp6-fay.png' });

  await browser.close();
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(2); });
