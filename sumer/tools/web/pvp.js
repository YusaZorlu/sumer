// Players fighting players in the browser, in three hosted games: PvP on (the default), PvP off,
// and combat mode. Ann hosts, Bob joins, each in a browser of their own.
const { chromium } = require('./lib.js');
const { open, info, cmd, waitFor, sleep } = require('./lib.js');
const BASE = 'http://localhost:8123/';
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const ARGS = ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--enable-webgl'];

// Ann at (38, 20) in the city's quiet east quarter, Bob 1.5 m south of her facing her, club in hand; Bob swings.
async function bobSwings(ann, bob) {
  await cmd(ann, 1, 38, 20);
  await cmd(bob, 1, 38, 21.5);
  await cmd(bob, 4, 0, 0.3);
  await cmd(bob, 14, 2);
  await sleep(1500);
  const before = await info(ann);
  await cmd(bob, 6);
  // Watch for a while what each of them sees: the marks over Bob and the red lights on Ann. This browser
  // runs about 5 frames a second and the game clamps each frame to 0.05 s, so the blow's moment (0.3 s of
  // game time) takes a second or more here: wait until it has landed or for 9 s.
  const seen = { warn: false, hit: false, light: [0, 0, 0, 0], bobSawWarn: false };
  for (const t0 = Date.now(); Date.now() - t0 < 9000;) {
    const a = await info(ann), b = await info(bob);
    const m = a && a.peers && a.peers.find(p => p.name === 'Bob');
    if (m && m.mark === 1) seen.warn = true;
    if (m && m.mark === 3) seen.hit = true;
    const mb = b && b.peers && b.peers.find(p => p.name === 'Bob');
    if (mb && mb.mark === 1) seen.bobSawWarn = true;
    if (a && a.hitLight) seen.light = seen.light.map((v, i) => Math.max(v, a.hitLight[i]));
    if (Date.now() - t0 > 2500 && (seen.hit || (a && a.health < before.health))) break;
    await sleep(60);
  }
  await sleep(800);
  return { before, after: await info(ann), bob: await info(bob), seen };
}

async function game(rules, name) {
  const room = name + Date.now() % 100000;
  const b1 = await chromium.launch({ args: ARGS }), b2 = await chromium.launch({ args: ARGS });
  const ann = await open(b1, 'ann', `${BASE}?auto=host&name=Ann&room=${room}${rules}`);
  await waitFor(ann, 'Ann hosting', i => i.lobby === 0 && i.role === 1);
  const bob = await open(b2, 'bob', `${BASE}?auto=join&name=Bob&room=${room}`);
  await waitFor(bob, 'Bob joining', i => i.lobby === 0 && i.role === 2);
  await sleep(2000);
  return { ann, bob, close: async () => { await b1.close(); await b2.close(); } };
}

(async () => {
  // 1. The default: PvP on, combat mode off.
  let g = await game('', 'pvp');
  let a = await info(g.ann), b = await info(g.bob);
  check(a.pvp === 1 && a.combat === 0 && b.pvp === 1 && b.combat === 0, `by default PvP is on and combat mode off, for host and guest ("${b.msg}")`);
  check(/ziggurat/.test(b.msg) && /boost/.test(b.msg), 'the welcome in a PvP game says the top of the ziggurat gives a boost');
  const people = { pvp: a.villagers };
  check(a.villagers === b.villagers && a.villagers > 20, `host and guest have the same people in a PvP game (${a.villagers} and ${b.villagers})`);
  await g.bob.screenshot({ path: __dirname + '/shots/pvp-welcome.png' });
  const inCity = i => i.x > -105 && i.x < 112 && i.z > -98 && i.z < 84;
  const away = (i, x, z) => Math.hypot(i.x - x, i.z - z);
  check(inCity(a) && inCity(b) && away(a, 14.2, 22) > 4 && away(b, 14, 44.2) > 4 && away(a, b.x, b.z) > 8,
        `in PvP everyone starts somewhere in the city, not at their door (Ann ${a.x.toFixed(0)},${a.z.toFixed(0)}  Bob ${b.x.toFixed(0)},${b.z.toFixed(0)})`);
  let r = await bobSwings(g.ann, g.bob);
  check(r.seen.warn && r.seen.hit, `Ann sees yellow over Bob while he swings, then red when it lands (warn ${r.seen.warn}, hit ${r.seen.hit})`);
  check(r.seen.light[1] > 0.5 && r.seen.light[0] < 0.2, `Bob struck her from behind: the bottom light, not the top (${r.seen.light.join(' ')})`);
  check(r.after.health < r.before.health && /Bob attacks you/.test(r.after.msg), `Bob's blow hurts Ann (${r.before.health} -> ${r.after.health}): "${r.after.msg}"`);
  const bobAtAnn = r.after.peers.find(p => p.name === 'Bob');
  check(bobAtAnn && bobAtAnn.wanted > 0, `Bob started a fight in the city: the guards want him (${bobAtAnn ? bobAtAnn.wanted : '?'} s)`);
  // Ann hits back: no crime.
  await cmd(g.ann, 4, Math.PI, 0.3);
  await cmd(g.ann, 14, 2);
  await sleep(800);
  const bobBefore = (await info(g.bob)).health;
  await cmd(g.ann, 6);
  let bobTop = 0, bobAfter = bobBefore;
  for (const t0 = Date.now(); Date.now() - t0 < 9000;) {
    const i = await info(g.bob);
    bobTop = Math.max(bobTop, i.hitLight[0]);
    bobAfter = i.health;
    if (Date.now() - t0 > 2500 && bobAfter < bobBefore) break;
    await sleep(60);
  }
  await sleep(500);
  check(bobTop > 0.5, `Ann struck Bob from ahead: the top light shows on his screen over the network (${bobTop.toFixed(2)})`);
  a = await info(g.ann);
  const annSelf = a.peers.find(p => p.name === 'Ann');
  check(bobAfter < bobBefore && annSelf.wanted === 0, `Ann hits back (${bobBefore} -> ${bobAfter}) and is not wanted`);
  // Ann brings Bob down: a kill for Ann, a death for Bob, and Bob hears who did it.
  await cmd(g.bob, 13, Math.max(0, (await info(g.bob)).health - 3));
  await sleep(800);
  await cmd(g.ann, 6);
  const down = await waitFor(g.bob, 'Bob brought down', i => /beat you down/.test(i.msg), 20000).catch(e => null);
  check(!!down && /Ann beat you down/.test(down.msg), `Bob is brought down by Ann: "${down ? down.msg : ''}"`);
  a = await waitFor(g.ann, 'the kill counted', i => { const b = i.peers.find(p => p.name === 'Bob'); return b && b.deaths === 1; }, 20000).catch(e => null);
  const annK = a && a.peers.find(p => p.name === 'Ann'), bobD = a && a.peers.find(p => p.name === 'Bob');
  check(!!a && annK.kills === 1 && bobD.deaths === 1 && /Ann brought down Bob/.test(a.msg), `Ann has the kill, Bob the death: Ann ${annK ? annK.kills : '?'}/${annK ? annK.deaths : '?'}, Bob ${bobD ? bobD.kills : '?'}/${bobD ? bobD.deaths : '?'} ("${a ? a.msg : ''}")`);
  b = await waitFor(g.bob, 'the score at Bob', i => { const x = i.peers.find(p => p.name === 'Ann'); return x && x.kills === 1; }, 15000).catch(e => null);
  check(!!b, 'Bob sees the same score');
  await sleep(1500);
  await g.ann.screenshot({ path: __dirname + '/shots/pvp-on.png' });
  await g.close();

  // 2. PvP off.
  g = await game('&pvp=0', 'nopvp');
  b = await info(g.bob);
  const a0 = await info(g.ann);
  check(b.pvp === 0, `PvP off reaches the guest: "${b.msg}"`);
  check(a0.villagers === b.villagers && b.villagers === people.pvp + 3, `without PvP the three ziggurat guards stand (${b.villagers} people, ${people.pvp} with PvP)`);
  check(!/ziggurat/.test(b.msg) && Math.hypot(b.x - 14, b.z - 44.2) < 1.5, `PvP off: no ziggurat tip, and Bob starts at his own door (${b.x.toFixed(1)},${b.z.toFixed(1)})`);
  r = await bobSwings(g.ann, g.bob);
  check(r.after.health === r.before.health, `PvP off: Bob's blow does Ann no harm (${r.before.health} -> ${r.after.health})`);
  await g.close();

  // 3. Combat mode.
  g = await game('&combat=1', 'combat');
  a = await info(g.ann);
  b = await info(g.bob);
  check(a.combat === 1 && b.combat === 1, `combat mode for both: "${b.msg}"`);
  r = await bobSwings(g.ann, g.bob);
  const bobSeen = r.after.peers.find(p => p.name === 'Bob');
  check(r.after.health < r.before.health && bobSeen.wanted === 0, `combat mode: the blow lands (${r.before.health} -> ${r.after.health}) and the guards stay out of it`);
  // Keys: 3 the war mace (everyone has one in combat mode), 4 hands free.
  await g.bob.keyboard.press('3');
  await sleep(800);
  const k3 = (await info(g.bob)).equip;
  await g.bob.keyboard.press('4');
  await sleep(800);
  const k4 = (await info(g.bob)).equip;
  check(k3 === 3 && k4 === 0, `key 3 takes up the war mace (${k3}), key 4 leaves the hands free (${k4})`);
  // A drawn bow holds the archer still.
  await g.bob.keyboard.press('1');
  await sleep(800);
  await cmd(g.bob, 1, 40, 27);
  await sleep(800);
  const box = await g.bob.evaluate(() => { const r = Module.canvas.getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; });
  await g.bob.mouse.move(box.x, box.y);
  await g.bob.mouse.down();
  await sleep(600);
  const before = await info(g.bob);
  await g.bob.keyboard.down('w');
  await sleep(3000);
  const held = await info(g.bob);
  await g.bob.keyboard.up('w');
  await g.bob.mouse.up();
  await sleep(800);
  await g.bob.keyboard.down('w');
  await sleep(3000);
  const walked = await info(g.bob);
  await g.bob.keyboard.up('w');
  const still = Math.hypot(held.x - before.x, held.z - before.z), moved = Math.hypot(walked.x - held.x, walked.z - held.z);
  check(still < 0.05 && moved > 0.5, `with the bow drawn W does not move Bob (${still.toFixed(2)} m); once it is loosed he walks (${moved.toFixed(1)} m)`);
  await g.bob.screenshot({ path: __dirname + '/shots/combat.png' });
  await g.close();

  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(2); });
