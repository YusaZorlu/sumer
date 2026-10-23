// The title menu by real clicks: Sumer Royale > Host and Join make one battle royale, and Sumer Survival >
// Multiplayer hosted under the same room name is a game apart.
const { launch, open, info, waitFor, sleep } = require('./lib.js');
const room = 'mf' + Date.now() % 100000;
const lobby = (dx, dy) => ({ x: 480 - 360 + dx, y: 8 + dy });
async function click(p, dx, dy) { const a = lobby(dx, dy); await p.mouse.click(a.x, a.y); await sleep(1500); }
let fails = 0; const check = (ok, w) => { console.log((ok ? 'PASS: ' : 'FAIL: ') + w); if (!ok) fails++; };
(async () => {
  const browser = await launch();
  const ann = await open(browser, 'ann', `http://localhost:8123/?name=Ann&room=${room}`);
  const bob = await open(browser, 'bob', `http://localhost:8123/?name=Bob&room=${room}`);
  const cem = await open(browser, 'cem', `http://localhost:8123/?name=Cem&room=${room}`);
  await sleep(6000);
  await click(ann, 165, 290); await click(ann, 165, 232);           // Sumer Royale, Host a battle royale
  let a = await waitFor(ann, 'Ann hosting', i => i.lobby === 0 && i.role === 1, 90000);
  check(a.royale.on === 1 && a.guards.length === 0, `Sumer Royale > Host starts a battle royale (royale ${a.royale.on}, guards ${a.guards.length})`);
  await click(cem, 165, 232); await click(cem, 165, 286);           // Sumer Survival, Multiplayer (host)
  let c = await waitFor(cem, 'Cem hosting', i => i.lobby === 0 && i.role === 1, 90000).catch(e => null);
  check(!!c && c.royale.on === 0 && c.guards.length > 0 && c.pvp === 0, `Sumer Survival > Multiplayer hosts the usual game in the same room name, apart (royale ${c && c.royale.on}, pvp ${c && c.pvp})`);
  await click(bob, 165, 290); await click(bob, 165, 290);           // Sumer Royale, Join the battle royale
  let b = await waitFor(bob, 'Bob joined', i => i.lobby === 0 && i.role === 2, 90000).catch(e => null);
  check(!!b && b.royale.on === 1 && b.peers.some(p => p.name === 'Ann') && !b.peers.some(p => p.name === 'Cem'), `Sumer Royale > Join joins Ann's battle royale, not Cem's game (${b && b.peers.map(p => p.name)})`);
  await browser.close();
  console.log(fails ? fails + ' FAILED' : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})();
