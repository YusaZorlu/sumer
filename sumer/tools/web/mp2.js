// Arrows, carcasses, a hen bought into the shared coop, a guest brought down, and messages in Turkish.
// Hosted with PvP off (&pvp=0), where being brought down means a shepherd carries you home; see pvp.js for the PvP game.
const { launch, open, info, cmd, waitFor, sleep } = require('./lib.js');
const SHOTS = __dirname + '/shots/';
const BASE = 'http://localhost:8123/';
const ROOM = 'two' + Date.now() % 100000;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const count = (s, item) => s.bag.filter(x => x[0] === item).reduce((n, x) => n + x[1], 0);

(async () => {
  const browser = await launch();
  const host = await open(browser, 'host', `${BASE}?auto=host&name=Ann&room=${ROOM}&pvp=0`);
  await waitFor(host, 'the host game', i => i.lobby === 0 && i.role === 1);
  const bob = await open(browser, 'bob', `${BASE}?auto=join&name=Bob&room=${ROOM}`);
  let b = await waitFor(bob, 'Bob joining', i => i.lobby === 0 && i.role === 2);

  // Bob reads Turkish: news from the host's game comes in his language.
  await cmd(bob, 11, 1);
  const cem = await open(browser, 'cem', `${BASE}?auto=join&name=Cem&room=${ROOM}`);
  await waitFor(cem, 'Cem joining', i => i.lobby === 0 && i.role === 2);
  b = await waitFor(bob, 'Turkish news', i => /oyuna katıldı/.test(i.msg), 30000).catch(e => null);
  check(!!b, `Bob, playing in Turkish, reads "${b ? b.msg : ''}"`);
  await cmd(bob, 11, 0);

  // Bob shoots a calm gazelle: the host's arrow kills it, Bob takes the carcass and his arrow back.
  let h = await info(host);
  let gi = -1;
  for (let i = 0; i < h.animals.length && gi < 0; i++) {
    const a = h.animals[i];
    if (a[0] === 0 && a[1] === 1 && a[2] === 0) gi = i;
  }
  const g = h.animals[gi];
  await cmd(bob, 1, g[3] + 7, g[4]);
  await sleep(3000);
  b = await info(bob);
  const q0 = b.quiver;
  h = await info(host);
  const gp = h.animals[gi];
  await cmd(bob, 10, gp[3], gp[4]);
  h = await waitFor(host, 'the gazelle hit', i => i.animals[gi][5] < 20, 30000).catch(e => null);
  check(!!h, `Bob's arrow strikes the host's gazelle (hp ${h ? h.animals[gi][5] : '?'}, state ${h ? h.animals[gi][2] : '?'})`);
  b = await waitFor(bob, 'the kill reported', i => /down|wounded/.test(i.msg), 20000).catch(e => null);
  check(!!b, `Bob hears of it: "${b ? b.msg : ''}"`);
  h = await info(host);
  if (h.animals[gi][2] === 3) {
    const at = h.animals[gi];
    await cmd(bob, 1, at[3] + 0.8, at[4]);
    await sleep(3000);
    b = await info(bob);
    check(b.target === 1, `standing at the carcass: "${b.promptE}"`);
    await cmd(bob, 2);
    b = await waitFor(bob, 'the carcass in a basket', i => count(i, 12) === 1, 20000).catch(e => null);
    check(!!b && b.quiver === q0, `Bob carries the gazelle, his arrow back in the quiver (${b ? b.quiver : '?'}/${q0}): "${b ? b.msg : ''}"`);
    h = await waitFor(host, 'the carcass gone at the host', i => !i.animals[gi][1], 20000).catch(e => null);
    check(!!h, 'the carcass is gone from the host\'s world');

    // The gazelle buys a hen for the shared coop.
    const hens = h.henCount;
    await cmd(bob, 12, 1, 3);
    h = await waitFor(host, 'a hen in the coop', i => i.henCount === hens + 1, 20000).catch(e => null);
    b = await waitFor(bob, 'paid with the gazelle', i => count(i, 12) === 0, 20000).catch(e => null);
    check(!!h && !!b, `the farmer's hen goes into the shared coop (${h ? h.henCount : '?'} hens), paid with Bob's gazelle: "${b ? b.msg : ''}"`);
    const c = await waitFor(cem, 'Cem sees the hen', i => i.henCount === hens + 1, 20000).catch(e => null);
    check(!!c, 'Cem sees the new hen too');
  } else {
    check(false, 'the gazelle died of the arrow');
  }

  // Bob is brought down far out: he comes to at home at once, the night does not pass.
  await cmd(bob, 1, 300, 150);
  await sleep(2000);
  const day = (await info(host)).day;
  await cmd(bob, 13, 500);
  b = await waitFor(bob, 'Bob at home', i => Math.hypot(i.x - 17, i.z - 53) < 4, 20000).catch(e => null);
  check(!!b && b.health > 25 && b.health < 35, `Bob wakes in his own house with ${b ? b.health : '?'} health: "${b ? b.msg : ''}"`);
  h = await info(host);
  check(h.day === day, `the world's day is still ${h.day}`);

  await bob.screenshot({ path: SHOTS + 'mp2-bob.png' });
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  await browser.close();
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(2); });
