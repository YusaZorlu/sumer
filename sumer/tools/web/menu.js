// The title screen by real clicks and typing: creating an account (and the mistakes it catches), logging
// out and back in, the password changed, then the server lists: Sumer Royale > Royale makes one battle royale, and Sumer Survival >
// Survival 1 is a game apart. Last, an account deleted, and the privacy page.
const { BASE, RUN, launch, open, info, waitFor, sleep } = require('./lib.js');
// The title's panel is 720 x 530 at (120, 8) in a 960 x 540 page; these are offsets in it.
async function click(p, dx, dy) { await p.mouse.click(120 + dx, 8 + dy); await sleep(1500); }
const TAB_LOGIN = [255, 136], TAB_CREATE = [465, 136], GO_LOGIN = [360, 298], GO_CREATE = [360, 346];
const ACCOUNT = [606, 89], PRIVACY = [606, 57], CHANGE = [225, 326], DELETE = [495, 326], BACK = [100, 427];
const SURVIVAL = [165, 142], ROYALE = [165, 200], ROOM_1 = [165, 218], ROYALE_1 = [165, 188], LOG_OUT = [620, 503];
let fails = 0; const check = (ok, w) => { console.log((ok ? 'PASS: ' : 'FAIL: ') + w); if (!ok) fails++; };

(async () => {
  const browser = await launch();
  const ann = await open(browser, null, BASE);
  let a = await waitFor(ann, 'the account page', i => i.accState === 1, 60000);
  await ann.screenshot({ path: __dirname + '/shots/menu-login.png' });
  check(a.lobby === 1 && await ann.isVisible('#acctName') && !await ann.isVisible('#acctPass2'), 'not logged in, the title asks for an account: name and password');
  await click(ann, ...TAB_CREATE);
  check(await ann.isVisible('#acctPass2'), 'the Create account tab asks for the password twice');
  const name = 'Ann' + RUN;
  await ann.fill('#acctName', name);
  await ann.fill('#acctPass', 'secret-one');
  await ann.fill('#acctPass2', 'secret-two');
  await click(ann, ...GO_CREATE);
  a = await info(ann);
  check(a.accState === 1 && /not the same/.test(a.note), `two different passwords are caught: "${a.note}"`);
  await ann.fill('#acctPass2', 'secret-one');
  await ann.screenshot({ path: __dirname + '/shots/menu-create.png' });
  await click(ann, ...GO_CREATE);
  a = await waitFor(ann, 'the account made', i => i.accState === 3, 30000).catch(e => null);
  check(!!a && a.account === name, `the account is made and logged in: ${a ? a.account : '?'}`);
  check(!await ann.isVisible('#acctName'), 'the account fields are put away');
  await ann.screenshot({ path: __dirname + '/shots/menu-modes.png' });

  // Log out, then back in with a wrong password and the right one (Enter sends it).
  await click(ann, ...LOG_OUT);
  a = await waitFor(ann, 'logged out', i => i.accState === 1, 20000).catch(e => null);
  check(!!a && await ann.isVisible('#acctName'), 'Log out goes back to the account page');
  await click(ann, ...TAB_LOGIN);
  await ann.fill('#acctName', name);
  await ann.fill('#acctPass', 'not-it');
  await click(ann, ...GO_LOGIN);
  a = await waitFor(ann, 'the refusal', i => i.accState === 1 && /Wrong/.test(i.note), 20000).catch(e => null);
  check(!!a, `a wrong password is refused: "${a ? a.note : (await info(ann)).note}"`);
  await ann.fill('#acctPass', 'secret-one');
  await ann.press('#acctPass', 'Enter');
  a = await waitFor(ann, 'logged in again', i => i.accState === 3 && i.account === name, 20000).catch(e => null);
  check(!!a, 'Enter in the password field logs in');
  const cookie = (await ann.context().cookies()).find(c => c.name === 'sumer');
  check(!!cookie && cookie.httpOnly && cookie.sameSite === 'Strict', 'the session is a cookie the page\'s scripts cannot read');

  // Your account: a new password after the current one (a wrong one is refused), then logging in with it.
  await click(ann, ...ACCOUNT);
  check(await ann.isVisible('#acctPass3') && !await ann.isVisible('#acctName'), 'Your account asks for the current password and the new one twice');
  await ann.screenshot({ path: __dirname + '/shots/menu-account.png' });
  await ann.fill('#acctPass', 'not-it');
  await ann.fill('#acctPass2', 'secret-new');
  await ann.fill('#acctPass3', 'secret-new');
  await click(ann, ...CHANGE);
  a = await waitFor(ann, 'the wrong password', i => /Wrong password/.test(i.note), 20000).catch(e => null);
  check(!!a, `the wrong current password is refused: "${a ? a.note : (await info(ann)).note}"`);
  await ann.fill('#acctPass', 'secret-one');
  await click(ann, ...CHANGE);
  a = await waitFor(ann, 'the password changed', i => /password is changed/.test(i.note), 20000).catch(e => null);
  check(!!a && await ann.inputValue('#acctPass') === '', `the password is changed and the fields wiped: "${a ? a.note : (await info(ann)).note}"`);
  await click(ann, ...BACK);
  await click(ann, ...LOG_OUT);
  await waitFor(ann, 'logged out', i => i.accState === 1, 20000).catch(e => null);
  await click(ann, ...TAB_LOGIN);
  await ann.fill('#acctName', name);
  await ann.fill('#acctPass', 'secret-new');
  await ann.press('#acctPass', 'Enter');
  a = await waitFor(ann, 'in with the new password', i => i.accState === 3 && i.account === name, 20000).catch(e => null);
  check(!!a, 'the new password logs in');

  // The server lists, by clicks.
  const bob = await open(browser, 'Bob' + RUN, BASE);
  const cem = await open(browser, 'Cem' + RUN, BASE);
  await waitFor(bob, 'Bob logged in', i => i.accState === 3, 60000);
  await waitFor(cem, 'Cem logged in', i => i.accState === 3, 60000);
  await click(ann, ...ROYALE);
  await ann.screenshot({ path: __dirname + '/shots/menu-royale.png' });
  await click(ann, ...ROYALE_1);
  a = await waitFor(ann, 'Ann in the royale', i => i.lobby === 0 && i.role === 2, 90000).catch(e => null);
  check(!!a && a.royale.on === 1 && a.guards.length === 0, `Sumer Royale > Royale joins a battle royale (royale ${a && a.royale.on}, guards ${a && a.guards.length})`);
  await click(cem, ...SURVIVAL);
  await cem.screenshot({ path: __dirname + '/shots/menu-survival.png' });
  await click(cem, ...ROOM_1);
  let c = await waitFor(cem, 'Cem in Survival 1', i => i.lobby === 0 && i.role === 2, 90000).catch(e => null);
  check(!!c && c.royale.on === 0 && c.guards.length > 0 && c.pvp === 0, `Sumer Survival > Survival 1 is the usual game, PvP off (royale ${c && c.royale.on}, pvp ${c && c.pvp})`);
  await click(bob, ...ROYALE);
  await click(bob, ...ROYALE_1);
  // The list of players comes a moment after joining.
  let b = await waitFor(bob, 'Bob joined', i => i.lobby === 0 && i.role === 2 && i.players >= 2, 90000).catch(e => null);
  check(!!b && b.royale.on === 1 && b.peers.some(p => p.name === name) && !b.peers.some(p => p.name === 'Cem' + RUN),
        `Sumer Royale > Royale puts Bob in Ann's battle royale, not Cem's game (${b && b.peers.map(p => p.name)})`);

  // An account deleted (asked twice), by real clicks; then the privacy page from the title screen.
  const del = await open(browser, 'Del' + RUN, BASE);
  await waitFor(del, 'Del logged in', i => i.accState === 3, 60000);
  await click(del, ...ACCOUNT);
  await del.fill('#acctPass', 'sumer-test');
  await click(del, ...DELETE);
  let d = await info(del);
  check(d.accState === 3, 'the first click on Delete account only asks again');
  await del.screenshot({ path: __dirname + '/shots/menu-delete.png' });
  await click(del, ...DELETE);
  d = await waitFor(del, 'the account deleted', i => i.accState === 1 && /deleted/.test(i.note), 20000).catch(e => null);
  const me = await del.evaluate(() => fetch('/api/me').then(r => r.status));
  check(!!d && me === 401, `the second deletes it, back at the login page: "${d ? d.note : (await info(del)).note}"`);
  await click(del, ...PRIVACY);
  await del.waitForURL(/privacy\.html$/, { timeout: 20000 }).catch(e => null);
  const page = await del.textContent('body').catch(e => '');
  check(/Yuşa Zorlu/.test(page) && /netcup/.test(page) && /Cloudflare/.test(page), 'Privacy opens the privacy page: owner, netcup, Cloudflare');
  await del.screenshot({ path: __dirname + '/shots/privacy.png', fullPage: true });

  await browser.close();
  console.log(fails ? fails + ' FAILED' : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(2); });
