// The alehouse's board games with real keys: a player sits at the Royal Game of Ur (E), has a townsman sit
// across (Enter), throws (Space) and moves (1) until the game is won, gaining experience; then two players
// play each other at the table upstairs, the winner gaining more. (A board is set near its end by the
// server's test command 40, so a game takes a minute, not twenty.)
const { BASE, RUN, launch, open, info, cmd, hostCmd, waitFor, sleep } = require('./lib.js');
const ROOM = 'survival-b' + RUN;
const URL = `${BASE}?auto=join&room=${ROOM}`;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const PANEL_BOARD = 11, SEAT_NPC = -2;

async function key(page, k) {
  await page.bringToFront();   // a page behind the other gets a frame now and then: its keys would go unread
  await page.keyboard.down(k);
  await sleep(500);
  await page.keyboard.up(k);
  await sleep(500);
}

// Sits at board b, side s: to the stool, E.
async function sit(page, b, s) {
  await page.bringToFront();
  await cmd(page, 25, b, s);
  await sleep(1500);
  await cmd(page, 2);
  return waitFor(page, `sitting at board ${b}`, i => i.board === b && i.panel === PANEL_BOARD, 30000).catch(e => null);
}

// Plays the side 'me' of board b on 'page' until someone wins (others' turns pass by themselves).
async function playOut(pages, b, sides) {
  for (let t = 0; t < 400; t++) {
    const i = await info(pages[0]), x = i.boards[b];
    if (x.winner >= 0) return x.winner;
    const k = sides.indexOf(x.turn);
    if (k >= 0) {
      if (x.roll < 0) await key(pages[k], 'Space');
      else if (x.roll > 0) await key(pages[k], '1');
      else await sleep(800);
    } else {
      await sleep(800);
    }
  }
  return -1;
}

(async () => {
  const browser = await launch();
  const ann = await open(browser, 'Ann' + RUN, URL);
  await waitFor(ann, 'Ann joining', i => i.lobby === 0 && i.role === 2);
  let a = await sit(ann, 0, 0);
  check(!!a && a.boards[0].seat[0] === a.self, 'E at the Royal Game of Ur: Ann sits down and the board opens');
  await ann.screenshot({ path: __dirname + '/shots/board-sit.png' });
  await key(ann, 'Enter');
  a = await waitFor(ann, 'a townsman across', i => i.boards[0].seat[1] === SEAT_NPC, 20000).catch(e => null) || await info(ann);
  check(a.boards[0].seat[1] === SEAT_NPC, 'Enter: a townsman sits across');
  await key(ann, 'Space');
  a = await waitFor(ann, 'a throw', i => i.boards[0].roll >= 0 || i.boards[0].turn === 1, 20000).catch(e => null) || await info(ann);
  check(a.boards[0].roll >= 0 || a.boards[0].turn === 1, `Space throws the four dice (${a.boards[0].roll})`);
  await sleep(1500);
  await ann.screenshot({ path: __dirname + '/shots/board-ur.png' });
  const xp0 = (await info(ann)).xp;
  await hostCmd(ROOM, 40, 0, 0);
  await sleep(1000);
  const w = await playOut([ann], 0, [0]);
  a = await waitFor(ann, 'experience', i => i.xp >= xp0 + 10, 20000).catch(e => null) || await info(ann);
  check(w === 0 && a.xp === xp0 + 10, `Ann wins against the townsman: +10 experience (${w}, +${a.xp - xp0})`);
  await ann.screenshot({ path: __dirname + '/shots/board-won.png' });

  // Up and away: the seat is free. Then two players upstairs.
  await key(ann, 'Escape');
  await cmd(ann, 25, 1, 1);   // to the other table: away from this one
  a = await waitFor(ann, 'Ann up', i => i.board < 0 && i.boards[0].seat[0] === -1, 20000).catch(e => null) || await info(ann);
  check(a.board < 0 && a.boards[0].seat[0] === -1 && a.boards[0].seat[1] === -1, 'away from the table: both seats free, the townsman gone');
  const bob = await open(browser, 'Bob' + RUN, URL);
  await waitFor(bob, 'Bob joining', i => i.lobby === 0 && i.role === 2);
  const b = await sit(bob, 2, 0);
  a = await sit(ann, 2, 1);
  check(!!b && !!a && a.boards[2].seat[0] === b.self && a.boards[2].seat[1] === a.self, 'upstairs: Bob and Ann sit across the board from each other');
  const xa = (await info(ann)).xp, xb = (await info(bob)).xp;
  await hostCmd(ROOM, 40, 2, 0);
  await sleep(1000);
  const w2 = await playOut([bob, ann], 2, [0, 1]);
  const bb = await waitFor(bob, 'Bob gains', i => i.xp >= xb + 20, 20000).catch(e => null) || await info(bob);
  a = await info(ann);
  check(w2 === 0 && bb.xp === xb + 20 && a.xp === xa, `Bob wins against a player: +20 experience; Ann none (+${bb.xp - xb}, +${a.xp - xa})`);
  await bob.screenshot({ path: __dirname + '/shots/board-players.png' });

  await browser.close();
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('ERROR ' + e.message); process.exit(1); });
