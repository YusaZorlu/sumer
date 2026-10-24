// A battle royale server: no round for one player alone, a countdown once two are in, everyone set down far
// apart, a statue's blessing, the sandstorm, players out and watching, the winner and their experience, a
// late joiner who watches, and the next round.
const { BASE, RUN, launch, open, info, cmd, hostInfo, hostCmd, waitFor, waitHost, sleep } = require('./lib.js');
const ROOM = 'royale-br' + RUN;
const URL = `${BASE}?auto=join&room=${ROOM}`;
const ANN = 'Ann' + RUN, BOB = 'Bob' + RUN, CEM = 'Cem' + RUN;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const away = (a, b) => Math.hypot(a.x - b.x, a.z - b.z);

(async () => {
  const browser = await launch();
  const ann = await open(browser, ANN, URL);
  let a = await waitFor(ann, 'Ann joining', i => i.lobby === 0 && i.role === 2);
  let h = await hostInfo(ROOM);
  check(a.royale.on === 1 && a.pvp === 1 && a.combat === 1 && h.guards === 0 && h.royale.statues.length === 15,
        `a battle royale: PvP and the best gear, no guards (${h.guards}), 15 statues (${h.royale.statues.length})`);
  // Alone, no round starts, not even when asked to.
  await hostCmd(ROOM, 20);
  await sleep(3000);
  h = await hostInfo(ROOM);
  a = await info(ann);
  check(h.royale.phase === 1 && h.royale.countdown < 0 && a.royale.countdown < 0, `one player alone: no round and no countdown (phase ${h.royale.phase}, countdown ${h.royale.countdown})`);
  await ann.screenshot({ path: __dirname + '/shots/royale-alone.png' });

  const bob = await open(browser, BOB, URL);
  await waitFor(bob, 'Bob joining', i => i.lobby === 0 && i.role === 2);
  h = await waitHost(ROOM, 'the countdown', i => i.royale.countdown > 0, 20000).catch(e => null);
  let b = await waitFor(bob, 'the countdown at Bob', i => i.royale.countdown > 0, 20000).catch(e => null);
  check(!!h && !!b, `with two in, a round starts in half a minute (server ${h ? h.royale.countdown : '?'} s, Bob sees ${b ? b.royale.countdown : '?'} s)`);
  await hostCmd(ROOM, 22, 600);   // a page here takes half a minute to load: Cem first, and the chat
  const cem = await open(browser, CEM, URL);
  await waitFor(cem, 'Cem joining', i => i.lobby === 0 && i.role === 2);

  // Chat before the round.
  await ann.keyboard.press('t');
  await waitFor(ann, 'Ann chatting', i => i.panel === 9, 10000).catch(e => null);
  await ann.keyboard.type('Ready?', { delay: 150 });
  await ann.keyboard.press('Enter');
  b = await waitFor(bob, 'Bob hears Ann', i => i.chat === `${ANN}: Ready?`, 30000).catch(e => null);
  check(!!b && b.royale.phase === 1, `Ann's line reaches Bob before the round (phase ${b ? b.royale.phase : '?'})`);

  // The round: everyone set down far out and far apart; everyone's count says three began.
  await hostCmd(ROOM, 22, 1);
  b = await waitFor(bob, 'the round at Bob', i => i.royale.phase === 2, 60000).catch(e => null);
  let c = await waitFor(cem, 'the round at Cem', i => i.royale.phase === 2, 60000).catch(e => null);
  a = await waitFor(ann, 'the round at Ann', i => i.royale.phase === 2, 60000).catch(e => null) || await info(ann);
  const center = { x: 0, z: -6 };
  check(!!b && !!c && away(a, b) > 60 && away(a, c) > 60 && away(b, c) > 60 && away(a, center) > 200 && away(b, center) > 200 && away(c, center) > 200,
        `the round begins with everyone far out and apart (Ann ${a.x.toFixed(0)},${a.z.toFixed(0)}  Bob ${b ? b.x.toFixed(0) : '?'},${b ? b.z.toFixed(0) : '?'}  Cem ${c ? c.x.toFixed(0) : '?'},${c ? c.z.toFixed(0) : '?'})`);
  check(!!b && /round has begun/.test(b.msg), `Bob hears the round begin: "${b ? b.msg : ''}"`);
  b = await waitFor(bob, 'the count at Bob', i => i.royale.started === 3 && i.royale.alive === 3, 20000).catch(e => null);
  check(!!b, `Bob's game counts 3 of 3 standing, not 3 of 0 (${b ? b.royale.alive : '?'} of ${b ? b.royale.started : '?'})`);

  // Bob prays at a statue of Ninurta: +5% attack speed, and the statue's blessing is gone for everyone.
  const st = h.royale.statues[0];
  await cmd(bob, 1, st[0] + 1.5, st[1] + 1.5);
  await waitHost(ROOM, 'Bob at the statue', i => i.peers.some(p => p.name === BOB && Math.hypot(p.x - st[0], p.z - st[1]) < 3), 60000).catch(e => null);
  await sleep(500);
  await cmd(bob, 2);
  b = await waitFor(bob, 'the blessing', i => i.royale.attack > 1.01, 60000).catch(e => null);
  check(!!b && /blesses you/.test(b.msg), `Bob's prayer is answered: attack x${b ? b.royale.attack : '?'} "${b ? b.msg : ''}"`);
  c = await waitFor(cem, 'the statue taken', i => i.royale.statues[0][2] === 1, 60000).catch(e => null);
  check(!!c, 'Cem sees the statue\'s blessing taken');

  // The sandstorm: late in the round it reaches Ann out in the land and hurts her.
  await hostCmd(ROOM, 21, 800);
  await cmd(ann, 1, 300, 300);
  const h0 = (await info(ann)).health;
  a = await waitFor(ann, 'the storm', i => i.royale.storm === 1 && i.health < h0 - 5, 60000).catch(e => null);
  check(!!a, `the storm hurts Ann where she stands (${h0} -> ${a ? a.health : '?'})`);
  await hostCmd(ROOM, 21, 0);
  await cmd(ann, 1, 0, 0);
  await sleep(1500);

  // Cem falls: he is out and watches; the others see him out.
  await cmd(cem, 13, 500);
  c = await waitFor(cem, 'Cem out', i => i.royale.out === 1, 60000).catch(e => null);
  check(!!c && /watch the others/.test(c.msg), `brought down, Cem is out and watches: "${c ? c.msg : ''}"`);
  h = await waitHost(ROOM, 'Cem out at the server', i => i.royale.alive === 2, 60000).catch(e => null);
  check(!!h, `the server counts two still standing (${h ? h.royale.alive : '?'})`);
  await cem.keyboard.press('t');
  await waitFor(cem, 'Cem chatting', i => i.panel === 9, 10000).catch(e => null);
  await cem.keyboard.type('gg', { delay: 150 });
  await cem.keyboard.press('Enter');
  b = await waitFor(bob, 'Bob hears Cem', i => i.chat === `${CEM}: gg`, 30000).catch(e => null);
  check(!!b, 'Cem, out and watching, still chats');

  // A latecomer watches until the next round.
  const dee = await open(browser, 'Dee' + RUN, URL);
  const d = await waitFor(dee, 'Dee joining', i => i.lobby === 0 && i.role === 2, 180000).catch(e => null);
  check(!!d && d.royale.out === 1, `Dee, who joins mid-round, watches (${d ? d.royale.out : '?'})`);

  // Bob falls: Ann is the last one standing, congratulated, and gains ten experience for each of the three.
  const xp0 = (await info(ann)).xp;
  await cmd(bob, 13, 500);
  h = await waitHost(ROOM, 'the round over', i => i.royale.phase === 3, 60000).catch(e => null);
  check(!!h && h.royale.winner === 1, `Ann wins the round (winner slot ${h ? h.royale.winner : '?'})`);
  a = await waitFor(ann, 'the winner\'s experience', i => i.xp >= xp0 + 30, 20000).catch(e => null);
  check(!!a && /Congratulations/.test(a.msg), `Ann is congratulated and gains ${a ? a.xp - xp0 : '?'} experience (3 players x 10): "${a ? a.msg : ''}"`);
  c = await waitFor(cem, 'the winner at Cem', i => i.royale.phase === 3 && i.royale.winner === 1, 60000).catch(e => null);
  check(!!c, 'Cem sees Ann win');
  await ann.screenshot({ path: __dirname + '/shots/royale-won.png' });

  // The next round comes by itself and brings everyone back in, the watchers too.
  h = await waitHost(ROOM, 'the next countdown', i => i.royale.countdown > 0, 20000).catch(e => null);
  check(!!h, `after the round the next one counts down (${h ? h.royale.countdown : '?'} s)`);
  await hostCmd(ROOM, 22, 1);
  c = await waitFor(cem, 'Cem back in', i => i.royale.phase === 2 && i.royale.out === 0, 60000).catch(e => null);
  const d2 = await waitFor(dee, 'Dee in', i => i.royale.phase === 2 && i.royale.out === 0, 60000).catch(e => null);
  check(!!c && !!d2, 'the next round brings Cem and Dee back in');
  await ann.screenshot({ path: __dirname + '/shots/royale.png' });

  await browser.close();
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('ERROR ' + e.message); process.exit(1); });
