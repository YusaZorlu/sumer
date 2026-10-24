// Three players in a Survival server (PvP off), each in a browser profile of their own; the world is the room's
// server program's, read through the test server (hostInfo). Being at their doors, the shared world, a plot
// both want, a snake, chat, a night everyone sleeps through, and the server stopping and starting again
// with the same world and everyone's things.
const { BASE, RUN, launch, open, info, cmd, hostInfo, hostCmd, stopRoom, waitFor, waitHost, sleep } = require('./lib.js');
const SHOTS = __dirname + '/shots/';
const ROOM = 'survival-mp' + RUN;
const ANN = 'Ann' + RUN, BOB = 'Bob' + RUN, CEM = 'Cem' + RUN;
const URL = `${BASE}?auto=join&room=${ROOM}`;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const near = (a, b, r) => Math.hypot(a[0] - b[0], a[1] - b[1]) < r;
const yawTo = (from, to) => Math.atan2(-(to[0] - from[0]), -(to[1] - from[1]));

(async () => {
  const browser = await launch();
  const ann = await open(browser, ANN, URL);
  let a = await waitFor(ann, 'Ann joining', i => i.lobby === 0 && i.role === 2);
  check(a.self === 1 && a.account === ANN, `Ann joins the server in slot ${a.self}, under her account's name`);
  const bob = await open(browser, BOB, URL);
  let b = await waitFor(bob, 'Bob joining', i => i.lobby === 0 && i.role === 2);
  const cem = await open(browser, CEM, URL);
  let c = await waitFor(cem, 'Cem joining', i => i.lobby === 0 && i.role === 2);
  check(b.self === 2 && c.self === 3, `Bob is in slot ${b.self}, Cem in slot ${c.self}`);
  let h = await waitHost(ROOM, 'three players at the server', i => i.players === 3);
  b = await waitFor(bob, 'three players at Bob', i => i.players === 3 && i.peers.some(p => p.name === CEM));
  check(h.players === 3 && b.peers.map(p => p.name).join() === [ANN, BOB, CEM].join(), `everyone sees ${b.peers.map(p => p.name).join(', ')}`);
  a = await info(ann);
  check(near([a.x, a.z], [14.2, 22], 2) && near([b.x, b.z], [14, 44.2], 2) && near([c.x, c.z], [28, 44.2], 2),
        `each starts at their own door (${a.x.toFixed(1)},${a.z.toFixed(1)}  ${b.x.toFixed(1)},${b.z.toFixed(1)}  ${c.x.toFixed(1)},${c.z.toFixed(1)})`);

  // Bob walks off: the server and Cem see him where he is.
  await cmd(bob, 1, 40, 10);
  h = await waitHost(ROOM, 'Bob moved at the server', i => i.peers.some(p => p.name === BOB && near([p.x, p.z], [40, 10], 1.5)), 30000).catch(e => null);
  check(!!h, 'the server sees Bob where he went');
  c = await waitFor(cem, 'Bob moved at Cem', i => i.peers.some(p => p.name === BOB && near([p.x, p.z], [40, 10], 1.5)), 30000).catch(e => null);
  check(!!c, 'Cem sees him there too');

  // The world is the server's: animals and townsfolk are where it has them.
  h = await hostInfo(ROOM);
  b = await info(bob);
  let same = 0;
  for (let i = 0; i < h.animals.length; i++) if (near([h.animals[i][3], h.animals[i][4]], [b.animals[i][3], b.animals[i][4]], 6)) same++;
  check(same > h.animals.length * 0.9, `${same} of ${h.animals.length} animals stand where the server has them`);
  check(near(h.villager0, b.villager0, 3), `the first villager walks the same lane (${h.villager0} / ${b.villager0})`);
  check(Math.abs(h.hour - b.hour) < 0.2 && h.day === b.day, `one clock: server ${h.hour.toFixed(2)}, Bob ${b.hour.toFixed(2)}`);

  // Both sow the same plot at once: the server gives it to one of them.
  await cmd(bob, 7, 3, 4);
  await cmd(cem, 7, 3, 4);
  await cmd(bob, 1, 29.6, 18.6);
  await cmd(cem, 1, 29.4, 18.6);
  await sleep(1500);
  b = await info(bob);
  c = await info(cem);
  check(b.target === 4 && b.targetIndex === 0 && c.target === 4, `both stand at plot 1: "${b.promptE}"`);
  await Promise.all([cmd(bob, 2), cmd(cem, 2)]);
  h = await waitHost(ROOM, 'plot 1 sown', i => i.plots[0] === 1, 20000).catch(e => null);
  check(!!h, 'the server has plot 1 sown');
  b = await waitFor(bob, 'Bob sees plot 1 sown', i => i.plots[0] === 1, 30000).catch(e => null) || await info(bob);
  c = await waitFor(cem, 'Cem sees plot 1 sown', i => i.plots[0] === 1, 30000).catch(e => null) || await info(cem);
  const seeds = s => s.bag.filter(x => x[0] === 3).reduce((n, x) => n + x[1], 0);
  console.log(`  Bob: "${b.msg}" seeds ${seeds(b)}; Cem: "${c.msg}" seeds ${seeds(c)}`);
  check(seeds(b) + seeds(c) === 7 && b.plots[0] === 1 && c.plots[0] === 1, 'exactly one of them paid a seed, and both see the wheat');

  // A snake by Bob's feet: the server's snake strikes him on his machine; his blows kill it there.
  h = await hostInfo(ROOM);
  const snake = h.animals.findIndex(x => x[0] === 5 && x[1] === 1 && x[2] === 0);
  if (snake < 0) {
    check(false, 'a calm snake to test with');
  } else {
    const xp0 = (await info(bob)).xp;
    const sp = [h.animals[snake][3], h.animals[snake][4]];
    await cmd(bob, 1, sp[0] + 1.6, sp[1]);
    b = await waitFor(bob, 'the snake biting Bob', i => i.health < 100, 40000).catch(e => null);
    check(!!b, `the snake bites Bob (health ${b ? b.health : '?'}, "${b ? b.msg : ''}")`);
    a = await info(ann);
    check(a.health === 100, 'Ann, far away, is unhurt');
    for (let k = 0; k < 6; k++) {
      b = await info(bob);
      const s = b.animals[snake];
      if (s[2] === 3 || !s[1]) break;
      await cmd(bob, 4, yawTo([b.x, b.z], [s[3], s[4]]), 0.3);
      await cmd(bob, 6);
      await sleep(1500);
    }
    h = await waitHost(ROOM, 'the snake dead', i => i.animals[snake][2] === 3, 20000).catch(e => null);
    check(!!h, `Bob's blows kill the server's snake (state ${h ? h.animals[snake][2] : '?'})`);
    b = await waitFor(bob, 'experience for the snake', i => i.xp === xp0 + 1, 20000).catch(e => null);
    check(!!b, `the kill gives Bob 1 experience (${xp0} -> ${b ? b.xp : (await info(bob)).xp})`);
  }

  // Everyone in one place, Ann looking at the others.
  await cmd(ann, 1, 21, 36);
  await cmd(bob, 1, 18, 41.5);
  await cmd(cem, 1, 23.5, 42);
  await cmd(ann, 4, Math.PI, 0.12);
  await cmd(cem, 4, -Math.PI / 2 - 0.4, 0.15);
  await sleep(5000);
  await ann.screenshot({ path: SHOTS + 'mp-ann.png' });
  await cem.screenshot({ path: SHOTS + 'mp-cem.png' });

  // Chat: T opens a line (the T is not typed into it), Enter sends it, and the server passes it on to everyone.
  await bob.keyboard.press('t');
  b = await waitFor(bob, 'the chat line open', i => i.panel === 9, 10000).catch(e => null);
  check(!!b, 'T opens the chat line');
  await bob.keyboard.type('Hello Ann', { delay: 150 });
  await bob.keyboard.press('Enter');
  const line = `${BOB}: Hello Ann`;
  a = await waitFor(ann, 'Ann hears Bob', i => i.chat === line, 15000).catch(e => null);
  c = await waitFor(cem, 'Cem hears Bob', i => i.chat === line, 15000).catch(e => null);
  b = await waitFor(bob, 'Bob sees his own line', i => i.chat === line && i.panel === 0, 15000).catch(e => null);
  check(!!a && !!c && !!b, `Bob's line reaches everyone: "${(a || await info(ann)).chat}"`);
  await ann.screenshot({ path: SHOTS + 'mp-chat.png' });

  // Night: it passes only when all three lie in bed.
  const beds = [[23, 19], [17, 53], [31, 53]];
  await hostCmd(ROOM, 9, 21.5);
  await sleep(1000);
  const before = (await hostInfo(ROOM)).day;
  const pages = [ann, bob, cem];
  for (let s = 0; s < 3; s++) await cmd(pages[s], 1, beds[s][0] - (s ? 0 : 1.0), beds[s][1] - (s ? 1.0 : 0));
  await sleep(1500);
  for (let s = 0; s < 2; s++) await cmd(pages[s], 2);
  await sleep(3000);
  h = await hostInfo(ROOM);
  a = await info(ann);
  check(h.day === before && a.waitBed === 1, `two of three in bed: still night (day ${h.day}, "${a.msg}")`);
  await ann.screenshot({ path: SHOTS + 'mp-bed.png' });
  await cmd(cem, 2);
  h = await waitHost(ROOM, 'dawn', i => i.day === before + 1, 30000).catch(e => null);
  check(!!h && Math.abs(h.hour - 6) < 0.5, `all three in bed: dawn of day ${h ? h.day : '?'}`);
  b = await waitFor(bob, 'Bob awake', i => !i.waitBed && i.day === before + 1, 20000).catch(e => null);
  check(!!b, `Bob wakes on day ${b ? b.day : '?'}: "${b ? b.msg : ''}"`);

  // Cem leaves; the others see it.
  await cem.context().close();
  h = await waitHost(ROOM, 'Cem gone at the server', i => i.players === 2, 20000).catch(e => null);
  b = await waitFor(bob, 'Cem gone at Bob', i => i.players === 2, 20000).catch(e => null);
  check(!!h && !!b, `Cem left: ${b ? b.msg : ''}`);
  await press(ann, 'Escape');
  await ann.screenshot({ path: SHOTS + 'mp-pause.png' });

  // The server stops the room (as when it stands empty a while): everyone goes back to the title.
  await stopRoom(ROOM);
  b = await waitFor(bob, 'Bob back at the title', i => i.lobby === 1, 30000).catch(e => null);
  check(!!b, 'when the server stops the game, Bob is back at the title screen');
  await sleep(1000);
  await bob.screenshot({ path: SHOTS + 'mp-stopped.png' });

  // Back in: the room starts again with the same world, and Bob comes back to his things.
  await bob.goto(URL);
  b = await waitFor(bob, 'Bob again', i => i.lobby === 0 && i.role === 2);
  h = await hostInfo(ROOM);
  check(h.day === before + 1, `the server's world goes on: day ${h.day}`);
  check(b.self === 2 && /Welcome back/.test(b.msg), `Bob is back in his own house with his things: "${b.msg}"`);

  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  await browser.close();
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(2); });

// Headless frames are slow: a key has to stay down across a frame for the game to see it.
async function press(page, key) { await page.keyboard.down(key); await sleep(2500); await page.keyboard.up(key); await sleep(1500); }
