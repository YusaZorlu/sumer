// Players fighting players, on three kinds of server: PvP, Survival (PvP off) and the battle royale, where
// everyone has the best of everything. Ann and Bob join, each in a browser of their own.
const { chromium } = require('./lib.js');
const { BASE, RUN, open, info, cmd, hostCmd, waitFor, sleep } = require('./lib.js');
const ANN = 'Ann' + RUN, BOB = 'Bob' + RUN;
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
  // The server's world runs in real time, so a mark lasts 0.8 s: Ann is looked at every time round, Bob every
  // other time.
  const seen = { warn: false, hit: false, light: [0, 0, 0, 0], bobSawWarn: false, bobFelt: 0, msgs: [] };
  for (let k = 0, t0 = Date.now(); Date.now() - t0 < 9000; k++) {
    const a = await info(ann), b = k % 2 ? await info(bob) : null;
    const m = a && a.peers && a.peers.find(p => p.name === BOB);
    if (m && m.mark === 1) seen.warn = true;
    if (m && m.mark === 3) seen.hit = true;
    const mb = b && b.peers && b.peers.find(p => p.name === BOB);
    if (mb && mb.mark === 1) seen.bobSawWarn = true;
    if (b && b.shake > seen.bobFelt) seen.bobFelt = b.shake;
    if (a && a.msg && !seen.msgs.includes(a.msg)) seen.msgs.push(a.msg);   // a caravan's news can follow at once
    if (a && a.hitLight) seen.light = seen.light.map((v, i) => Math.max(v, a.hitLight[i]));
    if (Date.now() - t0 > 2500 && (seen.hit || (a && a.health < before.health))) break;
    await sleep(20);
  }
  await sleep(800);
  const after = await info(ann);
  if (after.msg && !seen.msgs.includes(after.msg)) seen.msgs.push(after.msg);
  return { before, after, bob: await info(bob), seen };
}

async function game(kind) {
  const room = `${kind}-fight${RUN}`;
  const b1 = await chromium.launch({ args: ARGS }), b2 = await chromium.launch({ args: ARGS });
  const ann = await open(b1, ANN, `${BASE}?auto=join&room=${room}`);
  await waitFor(ann, 'Ann joining', i => i.lobby === 0 && i.role === 2);
  const bob = await open(b2, BOB, `${BASE}?auto=join&room=${room}`);
  await waitFor(bob, 'Bob joining', i => i.lobby === 0 && i.role === 2);
  await hostCmd(room, 22, 900);   // a battle royale's round would start in half a minute: not during these tests
  await sleep(2000);
  return { ann, bob, close: async () => { await b1.close(); await b2.close(); } };
}

(async () => {
  // 1. A PvP server.
  let g = await game('pvp');
  let a = await info(g.ann), b = await info(g.bob);
  check(a.pvp === 1 && a.combat === 1 && b.pvp === 1 && b.combat === 1, `on a PvP server PvP is on for both, with the best gear ("${b.msg}")`);
  check(/ziggurat/.test(b.msg) && /boost/.test(b.msg), 'the welcome in a PvP game says the top of the ziggurat gives a boost');
  const people = { pvp: a.villagers };
  check(a.villagers === b.villagers && a.villagers > 20, `both have the same people in a PvP game (${a.villagers} and ${b.villagers})`);
  await g.bob.screenshot({ path: __dirname + '/shots/pvp-welcome.png' });
  const inCity = i => i.x > -105 && i.x < 112 && i.z > -98 && i.z < 84;
  const away = (i, x, z) => Math.hypot(i.x - x, i.z - z);
  check(inCity(a) && inCity(b) && away(a, 14.2, 22) > 4 && away(b, 14, 44.2) > 4 && away(a, b.x, b.z) > 8,
        `in PvP everyone starts somewhere in the city, not at their door (Ann ${a.x.toFixed(0)},${a.z.toFixed(0)}  Bob ${b.x.toFixed(0)},${b.z.toFixed(0)})`);
  let r = await bobSwings(g.ann, g.bob);
  // The yellow shows for the blow's 0.3 s, the red 0.8 s, in real time: a page this slow catches one or both.
  check(r.seen.warn || r.seen.hit, `Ann sees the mark over Bob: yellow while he swings, red when it lands (warn ${r.seen.warn}, hit ${r.seen.hit})`);
  check(r.seen.light[1] > 0.5 && r.seen.light[0] < 0.2, `Bob struck her from behind: the bottom light, not the top (${r.seen.light.join(' ')})`);
  check(r.after.health < r.before.health && r.seen.msgs.some(m => /attacks you/.test(m)), `Bob's blow hurts Ann (${r.before.health} -> ${r.after.health}): "${r.seen.msgs.join('" "')}"`);
  check(r.seen.bobFelt > 0, `Bob feels his blow land on Ann over the network: his view kicks (${r.seen.bobFelt.toFixed(2)})`);
  const bobAtAnn = r.after.peers.find(p => p.name === BOB);
  check(bobAtAnn && bobAtAnn.wanted === 0, `Bob started a fight in the city, and the guards stay out of it (wanted ${bobAtAnn ? bobAtAnn.wanted : '?'})`);
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
  const annSelf = a.peers.find(p => p.name === ANN);
  check(bobAfter < bobBefore && annSelf.wanted === 0, `Ann hits back (${bobBefore} -> ${bobAfter}) and is not wanted`);
  // Ann brings Bob down: a kill for Ann, a death for Bob, and Bob hears who did it. The kill is worth 10
  // experience, and both accounts keep it in their K/D.
  // Out in the open land, where no guard comes after Bob for the fight he started.
  const xpBefore = (await info(g.ann)).xp;
  await cmd(g.ann, 1, 250, 60);
  await cmd(g.bob, 1, 250, 61.5);
  await cmd(g.ann, 4, Math.PI, 0.3);
  await sleep(1500);
  // Bob nearly down first (his armor softens every hurt, so again until it is so).
  for (let k = 0; k < 8 && (await info(g.bob)).health > 5; k++) { await cmd(g.bob, 13, Math.max(1, (await info(g.bob)).health - 3)); await sleep(300); }
  await sleep(300);
  await cmd(g.ann, 6);
  const down = await waitFor(g.bob, 'Bob brought down', i => /beat you down/.test(i.msg), 20000).catch(e => null);
  check(!!down && new RegExp(ANN + ' beat you down').test(down.msg), `Bob is brought down by Ann: "${down ? down.msg : ''}"`);
  a = await waitFor(g.ann, 'the kill counted', i => { const b = i.peers.find(p => p.name === BOB); return b && b.deaths === 1; }, 20000).catch(e => null);
  const annK = a && a.peers.find(p => p.name === ANN), bobD = a && a.peers.find(p => p.name === BOB);
  check(!!a && annK.kills === 1 && bobD.deaths === 1 && new RegExp(ANN + ' brought down ' + BOB).test(a.msg), `Ann has the kill, Bob the death: Ann ${annK ? annK.kills : '?'}/${annK ? annK.deaths : '?'}, Bob ${bobD ? bobD.kills : '?'}/${bobD ? bobD.deaths : '?'} ("${a ? a.msg : ''}")`);
  b = await waitFor(g.bob, 'the score at Bob', i => { const x = i.peers.find(p => p.name === ANN); return x && x.kills === 1; }, 15000).catch(e => null);
  check(!!b, 'Bob sees the same score');
  a = await waitFor(g.ann, 'experience for the kill', i => i.xp >= xpBefore + 10, 15000).catch(e => null);
  check(!!a, `a player brought down is worth 10 experience (${xpBefore} -> ${a ? a.xp : (await info(g.ann)).xp})`);
  const me = async page => (await page.evaluate(() => fetch('/api/me').then(r => r.text()))).trim().split('\t');
  const annMe = await me(g.ann), bobMe = await me(g.bob);
  check(annMe[2] === '1' && annMe[3] === '0' && bobMe[2] === '0' && bobMe[3] === '1', `the accounts keep the K/D: Ann ${annMe.slice(1).join('/')}, Bob ${bobMe.slice(1).join('/')}`);
  await sleep(1500);
  await g.ann.screenshot({ path: __dirname + '/shots/pvp-on.png' });
  await g.close();

  // 2. A Survival server: PvP off.
  g = await game('survival');
  b = await info(g.bob);
  const a0 = await info(g.ann);
  check(b.pvp === 0, `PvP off reaches the guest: "${b.msg}"`);
  check(a0.villagers === b.villagers && b.villagers === people.pvp + 3, `without PvP the three ziggurat guards stand (${b.villagers} people, ${people.pvp} with PvP)`);
  check(!/ziggurat/.test(b.msg) && Math.hypot(b.x - 14, b.z - 44.2) < 1.5, `PvP off: no ziggurat tip, and Bob starts at his own door (${b.x.toFixed(1)},${b.z.toFixed(1)})`);
  r = await bobSwings(g.ann, g.bob);
  check(r.after.health === r.before.health, `PvP off: Bob's blow does Ann no harm (${r.before.health} -> ${r.after.health})`);
  await g.close();

  // 3. The battle royale: everyone has the best of everything (the PvP servers have no such mode).
  g = await game('royale');
  a = await info(g.ann);
  b = await info(g.bob);
  check(a.combat === 1 && b.combat === 1, `the battle royale gives both the best gear: "${b.msg}"`);
  r = await bobSwings(g.ann, g.bob);
  const bobSeen = r.after.peers.find(p => p.name === BOB);
  check(r.after.health < r.before.health && bobSeen.wanted === 0, `the blow lands (${r.before.health} -> ${r.after.health}), and no guard wants Bob for it`);
  // Keys: 3 the war mace (everyone has one in a battle royale), 4 hands free.
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
