// The alehouse's board game on a phone held sideways (844 x 390, touch only): Use at the table sits down,
// taps play a townsman, throw and move pieces until the game is won; Close puts the board away while the
// seat is kept, Use brings it back, Get up frees it. The board leaves movement and archery as they were: the
// game's buttons are put away while it is open, a tap on it neither turns nor moves the player, and up from
// the stool (Get up, or walking off) the player stands, walks and shoots as before.
// Screenshots go to tools/web/shots/phone-board*.png.
const { BASE, RUN, launch, open, info, cmd, hostCmd, waitFor, sleep } = require('./lib.js');
const ROOM = 'survival-pb' + RUN;
const PHONE = {
  viewport: { width: 844, height: 390 }, deviceScaleFactor: 3, isMobile: true, hasTouch: true,
  userAgent: 'Mozilla/5.0 (Linux; Android 14; Pixel 8) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Mobile Safari/537.36',
};
const PANEL_BOARD = 11, SEAT_NPC = -2, ATTACK = 0, USE = 2, MENU = 6;
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
  i = await waitFor(page, 'on the stool', j => j.crouch === 1, 20000).catch(e => null) || await info(page);
  check(i.crouch === 1, 'seated on the stool');
  check(i.buttons.every((b, k) => k === MENU ? b[0] > 0 : b[0] < 0), 'while the board is open only the Close button of the game\'s own stays');
  await page.screenshot({ path: __dirname + '/shots/phone-board.png', timeout: 180000 });
  const still = await info(page);
  await tap([200, 200]);   // on the board, off any piece
  await tap([560, 330]);   // under the buttons
  i = await info(page);
  check(Math.abs(i.yaw - still.yaw) < 0.01 && Math.hypot(i.x - still.x, i.z - still.z) < 0.05 && i.swing <= 0,
        `taps on the board neither turn, move nor strike (yaw ${still.yaw.toFixed(2)} -> ${i.yaw.toFixed(2)})`);

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
  // (a snapshot sent before the host had her up can show the seat taken for a frame more)
  i = await waitFor(page, 'standing', j => j.crouch === 0, 10000).catch(e => null) || await info(page);
  check(i.crouch === 0 && i.buttons.every(b => b[0] > 0), `up from the stool: standing, every touch button back (crouch ${i.crouch})`);

  // Down again, then away with the stick: up off the stool at once, and the seat freed.
  const cdp = await page.context().newCDPSession(page);
  const touch = (type, pts) => cdp.send('Input.dispatchTouchEvent', { type, touchPoints: pts.map(([x, y, id]) => ({ x, y, id })) });
  await cmd(page, 25, 0, 0);
  await sleep(1500);
  await button(USE);
  await waitFor(page, 'seated again', j => j.panel === PANEL_BOARD && j.board === 0, 30000).catch(e => null);
  await button(MENU);
  await waitFor(page, 'the board put away', j => j.panel === 0, 20000).catch(e => null);
  await cmd(page, 4, -Math.PI / 2, 0);   // facing east along the hall: backward is clear past the table's end
  await sleep(800);
  const s0 = await info(page);
  await touch('touchStart', [[120, 300, 1]]);
  await touch('touchMove', [[120, 350, 1]]);   // pulled back: away from the stool
  const off = await waitFor(page, 'off the stool', j => Math.hypot(j.x - s0.x, j.z - s0.z) > 0.8, 60000).catch(e => null);
  const freed = await waitFor(page, 'the seat freed', j => j.board < 0, 60000).catch(e => null);
  await touch('touchEnd', []);
  check(s0.crouch === 1 && !!off && off.crouch === 0, `the stick walks Pia off the stool, standing (crouch ${s0.crouch} -> ${off ? off.crouch : '?'})`);
  check(!!freed && freed.boards[0].seat[0] === -1, 'walked away: the seat is free');

  // Out in the open: the bow drawn by holding Attack, aimed by sliding, shot by letting go.
  await cmd(page, 1, 40, 10);
  await cmd(page, 14, 1);
  await sleep(1500);
  i = await info(page);
  const q0 = i.quiver, a = i.buttons[ATTACK];
  await touch('touchStart', [[a[0], a[1], 3]]);
  const drawn = await waitFor(page, 'the bow drawn', j => j.aim === 1, 20000).catch(e => null);
  await touch('touchMove', [[a[0] - 40, a[1], 3]]);
  await touch('touchMove', [[a[0] - 80, a[1], 3]]);
  const aimed = await waitFor(page, 'aiming', j => !!drawn && Math.abs(j.yaw - drawn.yaw) > 0.1, 20000).catch(e => null);
  await sleep(1500);
  await touch('touchEnd', []);
  const shot = await waitFor(page, 'a shot', j => j.quiver === q0 - 1, 20000).catch(e => null);
  check(!!drawn && !!aimed && !!shot && drawn.crouch === 0, `after the game the bow draws, aims and shoots as before (quiver ${q0} -> ${shot ? shot.quiver : '?'})`);

  await browser.close();
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('ERROR ' + e.message); process.exit(1); });
