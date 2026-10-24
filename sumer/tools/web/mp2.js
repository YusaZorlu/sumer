// Arrows, carcasses, a hen bought into the shared coop, a player brought down, experience, and messages in
// Turkish, in a Survival server (PvP off, where being brought down means a shepherd carries you home; see
// pvp.js for PvP).
const { BASE, RUN, launch, open, info, cmd, hostInfo, waitFor, waitHost, sleep } = require('./lib.js');
const SHOTS = __dirname + '/shots/';
const ROOM = 'survival-two' + RUN;
const URL = `${BASE}?auto=join&room=${ROOM}`;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const count = (s, item) => s.bag.filter(x => x[0] === item).reduce((n, x) => n + x[1], 0);

(async () => {
  const browser = await launch();
  const ann = await open(browser, 'Ann' + RUN, URL);
  await waitFor(ann, 'Ann joining', i => i.lobby === 0 && i.role === 2);
  const bob = await open(browser, 'Bob' + RUN, URL);
  let b = await waitFor(bob, 'Bob joining', i => i.lobby === 0 && i.role === 2);

  // Bob reads Turkish: news from the server comes in his language.
  await cmd(bob, 11, 1);
  const cem = await open(browser, 'Cem' + RUN, URL);
  await waitFor(cem, 'Cem joining', i => i.lobby === 0 && i.role === 2);
  b = await waitFor(bob, 'Turkish news', i => /oyuna katıldı/.test(i.msg), 30000).catch(e => null);
  check(!!b, `Bob, playing in Turkish, reads "${b ? b.msg : ''}"`);
  const a = await info(ann);
  check(/joined the game/.test(a.msg), `Ann, in English, reads "${a.msg}"`);
  await cmd(bob, 11, 0);

  // Bob shoots a calm gazelle: the server's arrow kills it, Bob takes the carcass and his arrow back.
  let h = await hostInfo(ROOM);
  let gi = -1;
  for (let i = 0; i < h.animals.length && gi < 0; i++) {
    const x = h.animals[i];
    if (x[0] === 0 && x[1] === 1 && x[2] === 0) gi = i;
  }
  // The server's world runs in real time: shoot from a way off at once, where the gazelle is now, and
  // again if it moved out of the way.
  b = await info(bob);
  const xp0 = b.xp;
  let q0 = b.quiver;
  for (let shot = 0; shot < 4 && !(h && h.animals[gi][5] < 20); shot++) {
    let gp = (await hostInfo(ROOM)).animals[gi];
    await cmd(bob, 1, gp[3] + 14, gp[4]);
    await sleep(400);
    gp = (await hostInfo(ROOM)).animals[gi];
    q0 = (await info(bob)).quiver;
    await cmd(bob, 10, gp[3], gp[4]);
    h = await waitHost(ROOM, 'the gazelle hit', i => i.animals[gi][5] < 20, 8000).catch(e => null);
  }
  check(!!h, `Bob's arrow strikes the server's gazelle (hp ${h ? h.animals[gi][5] : '?'}, state ${h ? h.animals[gi][2] : '?'})`);
  b = await waitFor(bob, 'the kill reported', i => /down|wounded/.test(i.msg), 20000).catch(e => null);
  check(!!b, `Bob hears of it: "${b ? b.msg : ''}"`);
  h = await hostInfo(ROOM);
  if (h.animals[gi][2] === 3) {
    b = await waitFor(bob, 'experience for the gazelle', i => i.xp === xp0 + 1, 20000).catch(e => null);
    check(!!b, `a beast brought down is worth 1 experience (${xp0} -> ${b ? b.xp : '?'})`);
    const at = h.animals[gi];
    await cmd(bob, 1, at[3] + 0.8, at[4]);
    await sleep(3000);
    b = await info(bob);
    check(b.target === 1, `standing at the carcass: "${b.promptE}"`);
    await cmd(bob, 2);
    b = await waitFor(bob, 'the carcass in a basket', i => count(i, 12) === 1, 20000).catch(e => null);
    check(!!b && b.quiver === q0, `Bob carries the gazelle, his arrow back in the quiver (${b ? b.quiver : '?'}/${q0}): "${b ? b.msg : ''}"`);
    h = await waitHost(ROOM, 'the carcass gone at the server', i => !i.animals[gi][1], 20000).catch(e => null);
    check(!!h, 'the carcass is gone from the server\'s world');

    // The gazelle buys a hen for the shared coop.
    const hens = h.henCount;
    await cmd(bob, 12, 1, 3);
    h = await waitHost(ROOM, 'a hen in the coop', i => i.henCount === hens + 1, 20000).catch(e => null);
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
  const day = (await hostInfo(ROOM)).day;
  await cmd(bob, 13, 500);
  b = await waitFor(bob, 'Bob at home', i => Math.hypot(i.x - 17, i.z - 53) < 4, 20000).catch(e => null);
  check(!!b && b.health > 25 && b.health < 35, `Bob wakes in his own house with ${b ? b.health : '?'} health: "${b ? b.msg : ''}"`);
  h = await hostInfo(ROOM);
  check(h.day === day, `the world's day is still ${h.day}`);

  await bob.screenshot({ path: SHOTS + 'mp2-bob.png' });
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  await browser.close();
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(2); });
