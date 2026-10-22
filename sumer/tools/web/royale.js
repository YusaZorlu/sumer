// A battle royale on the server's battle royale port: three players, a round started by the host, a
// statue's blessing, the sandstorm, players out and watching, the winner, and a late joiner who watches.
const { launch, open, info, cmd, waitFor, sleep } = require('./lib.js');
const BASE = 'http://localhost:8133/';
const ROOM = 'br' + Date.now() % 100000;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const away = (a, b) => Math.hypot(a.x - b.x, a.z - b.z);

(async () => {
  const browser = await launch();
  const ann = await open(browser, 'ann', `${BASE}?auto=host&name=Ann&room=${ROOM}`);
  let a = await waitFor(ann, 'Ann hosting', i => i.lobby === 0 && i.role === 1);
  check(/mode=br/.test(ann.url()) && a.royale.on === 1 && a.pvp === 1 && a.combat === 1,
        `the battle royale port opens the game in its battle royale mode, combat on (${ann.url()})`);
  check(a.guards.length === 0 && a.royale.statues.length === 15 && a.royale.phase === 1,
        `no guards (${a.guards.length}), 15 statues (${a.royale.statues.length}), waiting for a round (${a.royale.phase})`);
  const bob = await open(browser, 'bob', `${BASE}?auto=join&name=Bob&room=${ROOM}`);
  await waitFor(bob, 'Bob joining', i => i.lobby === 0 && i.role === 2);
  const cem = await open(browser, 'cem', `${BASE}?auto=join&name=Cem&room=${ROOM}`);
  await waitFor(cem, 'Cem joining', i => i.lobby === 0 && i.role === 2);
  await sleep(1500);

  // Chat before the round: the host's Enter sends the line and does not start the round.
  await ann.keyboard.press('t');
  await waitFor(ann, 'Ann chatting', i => i.panel === 10, 10000).catch(e => null);
  await ann.keyboard.type('Ready?', { delay: 150 });
  await ann.keyboard.press('Enter');
  let b0 = await waitFor(bob, 'Bob hears Ann', i => i.chat === 'Ann: Ready?', 30000).catch(e => null);
  a = await info(ann);
  check(!!b0 && a.royale.phase === 1, `Ann's line reaches Bob and the round waits (phase ${a.royale.phase}, "${(b0 || await info(bob)).chat}")`);

  // The round: everyone set down far out and far apart.
  await cmd(ann, 20);
  let b = await waitFor(bob, 'the round at Bob', i => i.royale.phase === 2, 60000).catch(e => null);
  let c = await waitFor(cem, 'the round at Cem', i => i.royale.phase === 2, 60000).catch(e => null);
  a = await info(ann);
  const center = { x: 0, z: -6 };
  check(!!b && !!c && away(a, b) > 60 && away(a, c) > 60 && away(b, c) > 60 && away(a, center) > 200 && away(b, center) > 200 && away(c, center) > 200,
        `the round begins with everyone far out and apart (Ann ${a.x.toFixed(0)},${a.z.toFixed(0)}  Bob ${b ? b.x.toFixed(0) : '?'},${b ? b.z.toFixed(0) : '?'}  Cem ${c ? c.x.toFixed(0) : '?'},${c ? c.z.toFixed(0) : '?'})`);
  check(!!b && /round has begun/.test(b.msg), `Bob hears the round begin: "${b ? b.msg : ''}"`);

  // Bob prays at a statue of Ninurta: +5% attack speed, and the statue's blessing is gone for everyone.
  const st = a.royale.statues[0];
  await cmd(bob, 1, st[0] + 1.5, st[1] + 1.5);
  // The host judges the prayer from where it sees Bob, so wait until it sees him at the statue.
  await waitFor(ann, 'Bob at the statue', i => { const p = i.peers.find(q => q.name === 'Bob'); return p && Math.hypot(p.x - st[0], p.z - st[1]) < 3; }, 60000).catch(e => null);
  await sleep(1000);
  await cmd(bob, 2);
  b = await waitFor(bob, 'the blessing', i => i.royale.attack > 1.01, 60000).catch(e => null);
  check(!!b && /blesses you/.test(b.msg), `Bob's prayer is answered: attack x${b ? b.royale.attack : '?'} "${b ? b.msg : ''}"`);
  c = await waitFor(cem, 'the statue taken', i => i.royale.statues[0][2] === 1, 60000).catch(e => null);
  check(!!c, 'Cem sees the statue\'s blessing taken');

  // The sandstorm: late in the round it reaches Ann out in the land and hurts her.
  await cmd(ann, 21, 800);
  await cmd(ann, 1, 300, 300);
  const h0 = (await info(ann)).health;
  a = await waitFor(ann, 'the storm', i => i.royale.storm === 1 && i.health < h0 - 5, 60000).catch(e => null);
  check(!!a, `the storm hurts Ann where she stands (${h0} -> ${a ? a.health : '?'})`);
  await cmd(ann, 21, 0);
  await cmd(ann, 1, 0, 0);
  await sleep(1500);

  // Cem falls: he is out and watches; the others see him out.
  await cmd(cem, 13, 500);
  c = await waitFor(cem, 'Cem out', i => i.royale.out === 1, 60000).catch(e => null);
  check(!!c && /watch the others/.test(c.msg), `brought down, Cem is out and watches: "${c ? c.msg : ''}"`);
  a = await waitFor(ann, 'Cem out at the host', i => { const p = i.peers.find(q => q.name === 'Cem'); return p && (p.flags & 512); }, 60000).catch(e => null);
  check(!!a && a.royale.alive === 2, `the host counts two still standing (${a ? a.royale.alive : '?'})`);
  await cem.keyboard.press('t');
  await waitFor(cem, 'Cem chatting', i => i.panel === 10, 10000).catch(e => null);
  await cem.keyboard.type('gg', { delay: 150 });
  await cem.keyboard.press('Enter');
  b = await waitFor(bob, 'Bob hears Cem', i => i.chat === 'Cem: gg', 30000).catch(e => null);
  check(!!b, 'Cem, out and watching, still chats');

  // A latecomer watches until the next round.
  const dee = await open(browser, 'dee', `${BASE}?auto=join&name=Dee&room=${ROOM}`);
  const d = await waitFor(dee, 'Dee joining', i => i.lobby === 0 && i.role === 2, 180000).catch(e => null);
  check(!!d && d.royale.out === 1, `Dee, who joins mid-round, watches (${d ? d.royale.out : '?'})`);

  // Bob falls: Ann is the last one standing.
  await cmd(bob, 13, 500);
  a = await waitFor(ann, 'the round over', i => i.royale.phase === 3, 60000).catch(e => null);
  check(!!a && a.royale.winner === 0, `Ann wins the round (winner ${a ? a.royale.winner : '?'})`);
  c = await waitFor(cem, 'the winner at Cem', i => i.royale.phase === 3 && i.royale.winner === 0, 60000).catch(e => null);
  check(!!c, 'Cem sees Ann win');

  // A new round brings everyone back in, the watchers too.
  await cmd(ann, 20);
  c = await waitFor(cem, 'Cem back in', i => i.royale.phase === 2 && i.royale.out === 0, 60000).catch(e => null);
  const d2 = await waitFor(dee, 'Dee in', i => i.royale.phase === 2 && i.royale.out === 0, 60000).catch(e => null);
  check(!!c && !!d2, 'the next round brings Cem and Dee back in');
  await ann.screenshot({ path: __dirname + '/shots/royale.png' });

  await browser.close();
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('ERROR ' + e.message); process.exit(1); });
