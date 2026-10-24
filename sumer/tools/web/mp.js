// A host and two guests in separate browser profiles, through server.py.
// These suites host with PvP off (&pvp=0): they check the world, doors, sleep and saves, where a PvP game would start everyone at
// random places in the city. PvP has its own suite (pvp.js).
const { launch, open, info, cmd, waitFor, sleep } = require('./lib.js');
const SHOTS = __dirname + '/shots/';
const BASE = 'http://localhost:8123/';
const ROOM = 'room' + Date.now() % 100000;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const near = (a, b, r) => Math.hypot(a[0] - b[0], a[1] - b[1]) < r;
const yawTo = (from, to) => Math.atan2(-(to[0] - from[0]), -(to[1] - from[1]));
// Headless frames are slow: a key has to stay down across a frame for the game to see it.
async function press(page, key) { await page.keyboard.down(key); await sleep(2500); await page.keyboard.up(key); await sleep(1500); }

(async () => {
  const browser = await launch();
  const host = await open(browser, 'host', `${BASE}?auto=host&name=Ann&room=${ROOM}&pvp=0`);
  let h = await waitFor(host, 'the host game', i => i.lobby === 0 && i.role === 1);
  check(h.self === 0 && h.players === 1, `Ann hosts room ${ROOM} (slot ${h.self})`);

  const bob = await open(browser, 'bob', `${BASE}?auto=join&name=Bob&room=${ROOM}`);
  let b = await waitFor(bob, 'Bob joining', i => i.lobby === 0 && i.role === 2);
  const cem = await open(browser, 'cem', `${BASE}?auto=join&name=Cem&room=${ROOM}`);
  let c = await waitFor(cem, 'Cem joining', i => i.lobby === 0 && i.role === 2);
  check(b.self === 1 && c.self === 2, `Bob is player ${b.self + 1}, Cem player ${c.self + 1}`);
  h = await waitFor(host, 'three players at the host', i => i.players === 3);
  b = await waitFor(bob, 'three players at Bob', i => i.players === 3 && i.peers.some(p => p.name === 'Cem'));
  check(h.players === 3 && b.peers.map(p => p.name).join() === 'Ann,Bob,Cem', `everyone sees ${b.peers.map(p => p.name).join(', ')}`);
  check(near([b.x, b.z], [14, 44.2], 2) && near([c.x, c.z], [28, 44.2], 2), `guests start at their own doors (${b.x.toFixed(1)},${b.z.toFixed(1)} and ${c.x.toFixed(1)},${c.z.toFixed(1)})`);

  // Bob walks off: the host and Cem see him where he is.
  await cmd(bob, 1, 40, 10);
  h = await waitFor(host, 'Bob moved at the host', i => near([i.peers[1].x, i.peers[1].z], [40, 10], 1.5), 30000).catch(e => null);
  check(!!h, 'the host sees Bob where he went');
  c = await waitFor(cem, 'Bob moved at Cem', i => i.peers.some(p => p.s === 1 && near([p.x, p.z], [40, 10], 1.5)), 30000).catch(e => null);
  check(!!c, 'Cem sees him there too');

  // The world is the host's: animals and townsfolk are where the host has them.
  h = await info(host);
  b = await info(bob);
  let same = 0;
  for (let i = 0; i < h.animals.length; i++) if (near([h.animals[i][3], h.animals[i][4]], [b.animals[i][3], b.animals[i][4]], 6)) same++;
  check(same > h.animals.length * 0.9, `${same} of ${h.animals.length} animals stand where the host has them`);
  check(near(h.villager0, b.villager0, 3), `the first villager walks the same lane (${h.villager0} / ${b.villager0})`);
  check(Math.abs(h.hour - b.hour) < 0.2 && h.day === b.day, `one clock: host ${h.hour.toFixed(2)}, Bob ${b.hour.toFixed(2)}`);

  // Both guests sow the same plot at once: the host gives it to one of them.
  await cmd(bob, 7, 3, 4);
  await cmd(cem, 7, 3, 4);
  await cmd(bob, 1, 29.6, 18.6);
  await cmd(cem, 1, 29.4, 18.6);
  await sleep(1500);
  b = await info(bob);
  c = await info(cem);
  check(b.target === 4 && b.targetIndex === 0 && c.target === 4, `both stand at plot 1: "${b.promptE}"`);
  await Promise.all([cmd(bob, 2), cmd(cem, 2)]);
  h = await waitFor(host, 'plot 1 sown', i => i.plots[0] === 1, 20000).catch(e => null);
  check(!!h, 'the host has plot 1 sown');
  b = await waitFor(bob, 'Bob sees plot 1 sown', i => i.plots[0] === 1, 30000).catch(e => null) || await info(bob);
  c = await waitFor(cem, 'Cem sees plot 1 sown', i => i.plots[0] === 1, 30000).catch(e => null) || await info(cem);
  const seeds = s => s.bag.filter(x => x[0] === 3).reduce((n, x) => n + x[1], 0);
  console.log(`  Bob: "${b.msg}" seeds ${seeds(b)}; Cem: "${c.msg}" seeds ${seeds(c)}`);
  check(seeds(b) + seeds(c) === 7 && b.plots[0] === 1 && c.plots[0] === 1, 'exactly one of them paid a seed, and both see the wheat');

  // A snake by Bob's feet: the host's snake strikes him on his machine.
  h = await info(host);
  const snake = h.animals.findIndex(a => a[0] === 5 && a[1] === 1 && a[2] === 0);
  if (snake < 0) {
    check(false, 'a calm snake to test with');
  } else {
    const sp = [h.animals[snake][3], h.animals[snake][4]];
    await cmd(bob, 1, sp[0] + 1.6, sp[1]);
    b = await waitFor(bob, 'the snake biting Bob', i => i.health < 100, 40000).catch(e => null);
    check(!!b, `the snake bites Bob (health ${b ? b.health : '?'}, "${b ? b.msg : ''}")`);
    h = await info(host);
    check(h.health === 100, 'the host, far away, is unhurt');
    // Bob strikes back with his club.
    for (let k = 0; k < 4; k++) {
      b = await info(bob);
      const s = b.animals[snake];
      if (s[2] === 3 || !s[1]) break;
      await cmd(bob, 4, yawTo([b.x, b.z], [s[3], s[4]]), 0.3);
      await cmd(bob, 6);
      await sleep(1500);
    }
    h = await waitFor(host, 'the snake dead', i => i.animals[snake][2] === 3 || i.animals[snake][5] < 16, 20000).catch(e => null);
    check(!!h, `Bob's blows land at the host (snake state ${h ? h.animals[snake][2] : '?'}, hp ${h ? h.animals[snake][5] : '?'})`);
  }

  // Everyone in one place, the host looking at the guests.
  await cmd(host, 1, 21, 36);
  await cmd(bob, 1, 18, 41.5);
  await cmd(cem, 1, 23.5, 42);
  await cmd(host, 4, Math.PI, 0.12);
  await cmd(cem, 4, -Math.PI / 2 - 0.4, 0.15);
  await sleep(5000);
  await host.screenshot({ path: SHOTS + 'mp-host.png' });
  await cem.screenshot({ path: SHOTS + 'mp-cem.png' });

  // Chat: T opens a line (the T is not typed into it), Enter sends it, and the host passes it on to everyone.
  await bob.keyboard.press('t');
  b = await waitFor(bob, 'the chat line open', i => i.panel === 9, 10000).catch(e => null);
  check(!!b, 'T opens the chat line');
  await bob.keyboard.type('Hello Ann', { delay: 150 });
  await bob.keyboard.press('Enter');
  h = await waitFor(host, 'the host hears Bob', i => i.chat === 'Bob: Hello Ann', 15000).catch(e => null);
  c = await waitFor(cem, 'Cem hears Bob', i => i.chat === 'Bob: Hello Ann', 15000).catch(e => null);
  b = await waitFor(bob, 'Bob sees his own line', i => i.chat === 'Bob: Hello Ann' && i.panel === 0, 15000).catch(e => null);
  check(!!h && !!c && !!b, `Bob's line reaches everyone: "${(h || await info(host)).chat}"`);
  await host.keyboard.press('t');
  await waitFor(host, 'the host chat line open', i => i.panel === 9, 10000).catch(e => null);
  await host.keyboard.type('Hi Bob', { delay: 150 });
  await host.keyboard.press('Enter');
  b = await waitFor(bob, 'Bob hears Ann', i => i.chat === 'Ann: Hi Bob', 15000).catch(e => null);
  check(!!b, `the host's line reaches Bob: "${(b || await info(bob)).chat}"`);
  await host.screenshot({ path: SHOTS + 'mp-chat.png' });

  // Night: it passes only when all three lie in bed.
  const beds = [[23, 19], [17, 53], [31, 53]];
  await cmd(host, 9, 21.5);
  await sleep(1000);
  const before = (await info(host)).day;
  const pages = [host, bob, cem];
  for (let s = 0; s < 3; s++) {
    await cmd(pages[s], 1, beds[s][0] - (s ? 0 : 1.0), beds[s][1] - (s ? 1.0 : 0));
  }
  await sleep(1500);
  for (let s = 0; s < 2; s++) await cmd(pages[s], 2);
  await sleep(3000);
  h = await info(host);
  check(h.day === before && h.waitBed === 1, `two of three in bed: still night (day ${h.day}, "${h.msg}")`);
  await host.screenshot({ path: SHOTS + 'mp-bed.png' });
  await cmd(cem, 2);
  h = await waitFor(host, 'dawn', i => i.day === before + 1, 30000).catch(e => null);
  check(!!h && Math.abs(h.hour - 6) < 0.5, `all three in bed: dawn of day ${h ? h.day : '?'}`);
  b = await waitFor(bob, 'Bob awake', i => !i.waitBed && i.day === before + 1, 20000).catch(e => null);
  check(!!b, `Bob wakes on day ${b ? b.day : '?'}: "${b ? b.msg : ''}"`);

  // Cem leaves; the others see it.
  await cem.context().close();
  h = await waitFor(host, 'Cem gone at the host', i => i.players === 2, 20000).catch(e => null);
  b = await waitFor(bob, 'Cem gone at Bob', i => i.players === 2, 20000).catch(e => null);
  check(!!h && !!b, `Cem left: ${b ? b.msg : ''}`);

  // The host ends the game (saving it); Bob is sent back to the title screen.
  await press(host, 'Escape');
  h = await info(host);
  console.log(`  host panel after Esc: ${h.panel}`);
  await host.screenshot({ path: SHOTS + 'mp-pause.png' });
  await press(host, 'KeyQ');
  b = await waitFor(bob, 'Bob back at the title', i => i.lobby === 1, 30000).catch(e => null);
  check(!!b, 'when the host leaves, Bob is back at the title screen');
  await sleep(1000);
  await bob.screenshot({ path: SHOTS + 'mp-hostleft.png' });

  // The host opens the game again: same day, Bob comes back to his things.
  await host.goto(`${BASE}?auto=host&name=Ann&room=${ROOM}&pvp=0`);
  h = await waitFor(host, 'host again', i => i.lobby === 0 && i.role === 1);
  check(h.day === before + 1, `the hosted game goes on: day ${h.day}`);
  await bob.goto(`${BASE}?auto=join&name=Bob&room=${ROOM}`);
  b = await waitFor(bob, 'Bob again', i => i.lobby === 0 && i.role === 2);
  check(b.self === 1 && b.health > 99 && /Welcome back/.test(b.msg), `Bob is back in the red house with his things: "${b.msg}"`);

  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  await browser.close();
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(2); });
