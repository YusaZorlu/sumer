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
  await sleep(2500);
  return { before, after: await info(ann), bob: await info(bob) };
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
  let r = await bobSwings(g.ann, g.bob);
  check(r.after.health < r.before.health && /Bob attacks you/.test(r.after.msg), `Bob's blow hurts Ann (${r.before.health} -> ${r.after.health}): "${r.after.msg}"`);
  const bobAtAnn = r.after.peers.find(p => p.name === 'Bob');
  check(bobAtAnn && bobAtAnn.wanted > 0, `Bob started a fight in the city: the guards want him (${bobAtAnn ? bobAtAnn.wanted : '?'} s)`);
  // Ann hits back: no crime.
  await cmd(g.ann, 4, Math.PI, 0.3);
  await cmd(g.ann, 14, 2);
  await sleep(800);
  const bobBefore = (await info(g.bob)).health;
  await cmd(g.ann, 6);
  await sleep(2500);
  const bobAfter = (await info(g.bob)).health;
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
  check(b.pvp === 0, `PvP off reaches the guest: "${b.msg}"`);
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
