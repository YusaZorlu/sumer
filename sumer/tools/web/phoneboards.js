// The alehouse's board game on a phone held sideways (844 x 390, touch only): Use at the table sits down,
// taps play a townsman, throw and move pieces until the game is won; Close puts the board away while the
// seat is kept, Use brings it back, Get up frees it. Screenshots go to tools/web/shots/phone-board*.png.
const { BASE, RUN, launch, open, info, cmd, hostCmd, waitFor, sleep } = require('./lib.js');
const ROOM = 'survival-pb' + RUN;
const PHONE = {
  viewport: { width: 844, height: 390 }, deviceScaleFactor: 3, isMobile: true, hasTouch: true,
  userAgent: 'Mozilla/5.0 (Linux; Android 14; Pixel 8) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Mobile Safari/537.36',
};
const PANEL_BOARD = 11, SEAT_NPC = -2, USE = 2, MENU = 6;
const THROW = 0, TOWNSMAN = 1, GET_UP = 2, PIECE = 3;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const shown = c => c && c[0] >= 0;
const onScreen = c => shown(c) && c[0] < 844 && c[1] < 390;

(async () => {
  const browser = await launch();
  const page = await open(browser, 'Pia' + RUN, `${BASE}?auto=join&room=${ROOM}`, false, PHONE);
  const tap = async c => { await page.touchscreen.tap(c[0], c[1]); await sleep(600); };
  const button = async b => tap((await info(page)).buttons[b]);
  let i = await waitFor(page, 'Pia joining', j => j.lobby === 0 && j.role === 2, 120000);
  check(i.touch === 1, 'the phone plays by touch');
  if (i.panel !== 0) { await button(MENU); await waitFor(page, 'no panel', j => j.panel === 0, 20000).catch(e => null); }

  // At the stool: Use sits down and the board opens, every button on the screen.
  await cmd(page, 25, 0, 0);
  await sleep(1500);
  await button(USE);
  i = await waitFor(page, 'the board', j => j.panel === PANEL_BOARD && j.board === 0, 30000).catch(e => null) || await info(page);
  check(i.panel === PANEL_BOARD && i.board === 0, 'Use at the table: Pia sits down and the board opens');
  check(onScreen(i.boardTaps[THROW]) && onScreen(i.boardTaps[TOWNSMAN]) && onScreen(i.boardTaps[GET_UP]),
        `Throw, Play a townsman and Get up are on the phone's screen (${JSON.stringify(i.boardTaps.slice(0, 3))})`);
  await page.screenshot({ path: __dirname + '/shots/phone-board.png', timeout: 180000 });

  await tap(i.boardTaps[TOWNSMAN]);
  i = await waitFor(page, 'a townsman', j => j.boards[0].seat[1] === SEAT_NPC, 20000).catch(e => null) || await info(page);
  check(i.boards[0].seat[1] === SEAT_NPC, 'a tap on Play a townsman: he sits across');

  // A tap throws; a tap on a ringed piece moves it. Near the end (server test command 40), played out.
  await hostCmd(ROOM, 40, 0, 0);
  await sleep(1000);
  const xp0 = (await info(page)).xp;
  let threw = false, moved = false;
  for (let t = 0; t < 300; t++) {
    i = await info(page);
    const x = i.boards[0];
    if (x.winner >= 0) break;
    if (x.turn === 0 && x.roll < 0 && shown(i.boardTaps[THROW])) { await tap(i.boardTaps[THROW]); threw = true; }
    else if (x.turn === 0 && x.roll > 0 && shown(i.boardTaps[PIECE])) {
      if (!moved) await page.screenshot({ path: __dirname + '/shots/phone-board-move.png', timeout: 180000 });
      await tap(i.boardTaps[PIECE]); moved = true;
    }
    else await sleep(700);
  }
  i = await waitFor(page, 'experience', j => j.xp >= xp0 + 10, 20000).catch(e => null) || await info(page);
  check(threw && moved && i.boards[0].winner === 0 && i.xp === xp0 + 10,
        `taps throw and move a piece until Pia wins: +10 experience (winner ${i.boards[0].winner}, +${i.xp - xp0})`);
  await page.screenshot({ path: __dirname + '/shots/phone-board-won.png', timeout: 180000 });

  // Close (the Menu button's place) puts the board away; the seat is kept, and Use brings it back.
  await button(MENU);
  i = await waitFor(page, 'the board put away', j => j.panel === 0, 20000).catch(e => null) || await info(page);
  check(i.panel === 0 && i.board === 0, 'Close puts the board away and Pia keeps the seat');
  await button(USE);
  i = await waitFor(page, 'back at the board', j => j.panel === PANEL_BOARD, 20000).catch(e => null) || await info(page);
  check(i.panel === PANEL_BOARD, 'Use brings the board back');
  await tap(i.boardTaps[GET_UP]);
  i = await waitFor(page, 'up', j => j.board < 0 && j.boards[0].seat[0] === -1, 20000).catch(e => null) || await info(page);
  check(i.board < 0 && i.panel === 0 && i.boards[0].seat[0] === -1 && i.boards[0].seat[1] === -1, 'Get up: the board closes and both seats are free');

  await browser.close();
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('ERROR ' + e.message); process.exit(1); });
