// The mouse in a browser: only Esc gives it back. A window opened over the game (the baskets with Tab, the
// board with E) keeps it, worked with the game's own cursor that the mouse moves, and closing the window
// (Tab, E) goes straight back to looking round, with no click; Esc in a window closes it and gives the
// mouse back without opening the pause menu.
const { BASE, RUN, launch, open, info, cmd, waitFor, sleep } = require('./lib.js');
const ROOM = 'survival-m' + RUN;
const URL = `${BASE}?auto=join&room=${ROOM}`;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const PANEL_BAG = 1, PANEL_PAUSE = 4, PANEL_BOARD = 11, RULES = 10;

(async () => {
  const browser = await launch();
  const p = await open(browser, 'Mia' + RUN, URL);
  await waitFor(p, 'Mia joining', i => i.lobby === 0 && i.role === 2, 120000);
  if ((await info(p)).panel !== 0) { await p.keyboard.press('Escape'); await sleep(1500); }
  let at = [480, 270];
  await p.mouse.click(at[0], at[1]);
  let i = await waitFor(p, 'the mouse taken', j => j.captured === 1, 20000).catch(e => null) || await info(p);
  check(i.captured === 1, 'a click takes the mouse');
  // Moves the captured mouse by (dx, dy) CSS pixels. Headless Chromium reports no real movement of a captured
  // mouse (each move comes as a jump there and back), so the movement events a browser sends are sent here.
  const nudge = async (dx, dy) => {
    await p.evaluate(([dx, dy]) => {
      for (let k = 0; k < 4; k++)
        document.dispatchEvent(new MouseEvent('mousemove', { movementX: dx / 4, movementY: dy / 4, bubbles: true }));
    }, [dx, dy]);
    await sleep(800);
  };

  await p.keyboard.press('Tab');
  i = await waitFor(p, 'the baskets', j => j.panel === PANEL_BAG, 20000).catch(e => null) || await info(p);
  check(i.panel === PANEL_BAG && i.captured === 1, `Tab: the baskets open and the mouse stays with the game (captured ${i.captured})`);
  const c0 = i.pointer;
  await nudge(120, 60);
  i = await info(p);
  check(i.pointer[0] > c0[0] + 60 && i.pointer[1] > c0[1] + 30, `the game's cursor follows the mouse (${c0} -> ${i.pointer})`);
  await p.screenshot({ path: __dirname + '/shots/mouse-bag.png', timeout: 180000 });
  await p.keyboard.press('Tab');
  i = await waitFor(p, 'the baskets closed', j => j.panel === 0, 20000).catch(e => null) || await info(p);
  const yaw0 = i.yaw;
  await nudge(-150, 0);
  i = await info(p);
  check(i.panel === 0 && i.captured === 1 && Math.abs(i.yaw - yaw0) > 0.1, `Tab again: back to play, the mouse turns the view at once, no click (yaw ${yaw0.toFixed(2)} -> ${i.yaw.toFixed(2)})`);

  // Esc in a window: closed, the mouse given back, and no pause menu.
  await p.keyboard.press('Tab');
  await waitFor(p, 'the baskets', j => j.panel === PANEL_BAG, 20000).catch(e => null);
  await p.keyboard.press('Escape');
  await sleep(2500);
  i = await info(p);
  check(i.panel === 0 && i.captured === 0, `Esc in the baskets: they close and the mouse is given back, no pause menu (panel ${i.panel}, captured ${i.captured})`);
  await p.keyboard.press('Escape');
  i = await waitFor(p, 'the pause menu', j => j.panel === PANEL_PAUSE, 20000).catch(e => null) || await info(p);
  check(i.panel === PANEL_PAUSE, 'and Esc then opens the pause menu as before');
  await p.keyboard.press('Escape');
  await waitFor(p, 'the menu closed', j => j.panel === 0, 20000).catch(e => null);
  await cmd(p, 14, 1);   // the bow in hand: the click that takes the mouse back must not shoot
  await sleep(800);
  const q0 = (await info(p)).quiver;
  await p.mouse.click(at[0], at[1]);
  i = await waitFor(p, 'the mouse taken', j => j.captured === 1, 20000).catch(e => null) || await info(p);
  await sleep(1500);
  check(i.captured === 1 && (await info(p)).quiver === q0, 'a click takes the mouse back, and is not a shot');

  // The board, opened with E: the cursor clicks Rules, R goes back, E leaves; the mouse kept throughout.
  await cmd(p, 25, 0, 0);
  await sleep(1500);
  await p.keyboard.press('e');
  i = await waitFor(p, 'the board', j => j.panel === PANEL_BOARD, 30000).catch(e => null) || await info(p);
  check(i.panel === PANEL_BOARD && i.captured === 1, `E at the table: the board opens, the mouse kept (captured ${i.captured})`);
  await sleep(800);
  i = await info(p);
  const r = i.boardTaps[RULES];
  await nudge(r[0] - i.pointer[0], r[1] - i.pointer[1]);
  await p.mouse.down();
  await sleep(300);
  await p.mouse.up();
  i = await waitFor(p, 'the rules', j => j.rules === 1, 20000).catch(e => null) || await info(p);
  check(i.rules === 1, `the game's cursor clicks Rules: the rules show (cursor ${i.pointer}, button ${r})`);
  await p.screenshot({ path: __dirname + '/shots/board-rules.png', timeout: 180000 });
  await p.keyboard.press('r');
  i = await waitFor(p, 'the board again', j => j.rules === 0, 20000).catch(e => null) || await info(p);
  check(i.rules === 0, 'R: back to the board');
  await p.keyboard.press('e');
  i = await waitFor(p, 'away from the board', j => j.panel === 0, 20000).catch(e => null) || await info(p);
  check(i.panel === 0 && i.captured === 1 && i.board === 0, 'E: away from the board, the seat kept and the mouse still the game\'s');

  await browser.close();
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('ERROR ' + e.message); process.exit(1); });
