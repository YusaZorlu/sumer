// Hide and seek in a Sumer Party: floor is lava voted out so hide and seek is played; one seeker, the
// others hidden as townsfolk (no armor, nothing in hand, no running) among the same bigger crowd on every
// screen; the seeker's blow sends a hider to the farthest village, and with everyone found the game ends.
const { BASE, RUN, launch, open, info, hostInfo, hostCmd, waitFor, waitHost, sleep } = require('./lib.js');
const ROOM = 'party-h' + RUN;
const URL = `${BASE}?auto=join&room=${ROOM}`;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const PG_LAVA = 0, PG_HIDE = 4, PP_GAME = 3, PP_BREAK = 4;

async function key(page, k, ms = 800) {
  await page.keyboard.down(k);
  await sleep(ms);
  await page.keyboard.up(k);
  await sleep(800);
}

async function voteOutLava(page) {
  await key(page, 'Escape');
  await waitFor(page, 'the pause menu', i => i.panel === 4, 20000);
  await key(page, 'v');
  await waitFor(page, 'the party panel', i => i.panel === 10, 20000);
  await page.mouse.click(130 + 590 + 55, 35 + 70 + 66 * PG_LAVA + 15);
  await waitFor(page, 'the vote', i => i.party.vote === PG_LAVA, 20000).catch(e => null);
  await key(page, 'Escape');
}

(async () => {
  const browser = await launch();
  const pages = [];
  for (const name of ['Ann', 'Bob', 'Cem']) {
    const p = await open(browser, name + RUN, URL);
    await waitFor(p, name + ' joining', i => i.lobby === 0 && i.role === 2);
    pages.push(p);
  }
  await voteOutLava(pages[0]);
  await voteOutLava(pages[1]);
  for (const p of pages) if (!(await info(p)).party.ready) await key(p, 'Enter');
  let h = await waitHost(ROOM, 'the first game', i => i.party.phase === PP_GAME, 40000).catch(e => null);
  check(!!h, 'everyone ready: the party begins');
  for (let k = 0; k < 4 && h && h.party.game !== PG_HIDE; k++) {
    await hostCmd(ROOM, 30, 239.5);
    await waitHost(ROOM, 'a break', i => i.party.phase === PP_BREAK, 30000).catch(e => null);
    await hostCmd(ROOM, 30, 0.2);
    h = await waitHost(ROOM, 'the next game', i => i.party.phase === PP_GAME, 30000).catch(e => null);
  }
  check(!!h && h.party.game === PG_HIDE, 'hide and seek comes round');
  await sleep(3000);
  h = await hostInfo(ROOM);
  const seen = [];
  for (const p of pages) seen.push(await info(p));
  const seeker = seen.find(i => i.self === h.party.seeker), hiders = seen.filter(i => i.self !== h.party.seeker);
  check(!!seeker && seeker.party.hider === 0 && hiders.length === 2 && hiders.every(i => i.party.hider === 1),
        `one seeker (slot ${h.party.seeker}), two hiders, on every screen`);
  const held = [];
  for (const i of hiders) {
    const p = pages[seen.indexOf(i)];
    await p.bringToFront();   // a page behind the others gets a frame now and then
    held.push((await waitFor(p, 'empty hands', j => j.equip === 0, 60000).catch(e => null) || await info(p)).equip);
  }
  check(held.every(e => e === 0), `the hiders hold nothing (${held})`);
  check(seen.every(i => i.villagers === h.villagers), `the same crowd of people on every screen as at the host (${h.villagers})`);
  const seekerPage = pages[seen.indexOf(seeker)], hiderPages = hiders.map(i => pages[seen.indexOf(i)]);

  // A hider cannot run: Shift with W goes no faster than a walk.
  const hp = hiderPages[0];
  let a = await info(hp);
  await hp.keyboard.down('Shift');
  await hp.keyboard.down('w');
  await sleep(1500);
  let b = await info(hp);
  await hp.keyboard.up('w');
  await hp.keyboard.up('Shift');
  const speed = Math.hypot(b.x - a.x, b.z - a.z) / 1.5;
  check(speed < 5.5, `a hider walks, never runs (${speed.toFixed(1)} m/s)`);
  await seekerPage.screenshot({ path: __dirname + '/shots/hide-seeker.png' });
  await hp.screenshot({ path: __dirname + '/shots/hide-hider.png' });

  // The seeker finds the first hider: off to the farthest village, +6 for the seeker; the game goes on.
  await hostCmd(ROOM, 31, hiders[0].self);
  await hiderPages[0].bringToFront();   // a page behind the others gets a frame now and then: 5 s down takes long
  a = await waitFor(hiderPages[0], 'the found hider far away', i => i.party.hider === 0 && Math.hypot(i.x, i.z + 6) > 150, 150000).catch(e => null) || await info(hiderPages[0]);
  check(a.party.hider === 0 && Math.hypot(a.x, a.z + 6) > 150 && a.equip === 1, `found: off to the farthest village, bow in hand (${Math.hypot(a.x, a.z + 6).toFixed(0)} m from the city, at ${a.x.toFixed(0)},${a.z.toFixed(0)}: "${a.msg}")`);
  h = await hostInfo(ROOM);
  const sc = h.party.scores.find(t => t[0] === seeker.self);
  check(h.party.phase === PP_GAME && sc && sc[1] >= 6, `the seeker +6 (${sc ? sc[1] : '?'}); one hider left, the game goes on`);
  await hiderPages[0].screenshot({ path: __dirname + '/shots/hide-found.png' });

  // The last one found: the game is over.
  await hostCmd(ROOM, 31, hiders[1].self);
  h = await waitHost(ROOM, 'the game over', i => i.party.phase !== PP_GAME, 30000).catch(e => null);
  check(!!h, 'every hider found: the game is over');

  await browser.close();
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('ERROR ' + e.message); process.exit(1); });
