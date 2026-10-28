// A Sumer Party server: no start until everyone in (two or more) is ready, a vote by real clicks that drops
// a game, four games with breaks between, someone who joins mid-game playing from the next one, the scores
// and the party's top three with their experience; then the scores start again.
const { BASE, RUN, launch, open, info, hostInfo, hostCmd, waitFor, waitHost, sleep } = require('./lib.js');
const ROOM = 'party-p' + RUN;
const URL = `${BASE}?auto=join&room=${ROOM}`;
const ANN = 'Ann' + RUN, BOB = 'Bob' + RUN, CEM = 'Cem' + RUN;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const PG_LAVA = 0, PG_ARCHERS = 2, PG_HIDE = 4, PP_WAIT = 1, PP_COUNT = 2, PP_GAME = 3, PP_BREAK = 4, PP_END = 5;

// What each game sets up, seen at Ann's: floor is lava starts up on a roof, out of the lava; one-shot
// archers hold the bow and nothing else.
async function inGame(g) {
  if (g === PG_LAVA) {
    const a = await waitFor(ann, 'Ann on a roof', i => i.party.game === PG_LAVA && i.party.up > 1.5, 20000).catch(e => null) || await info(ann);
    check(a.party.up > 1.5 && !a.party.lava, `floor is lava: Ann starts up on a roof (${a.party.up} m up), out of the lava`);
    await sleep(3000);
    const b = await info(ann);
    check(b.health >= 99, `up there nothing burns (${b.health})`);
    await ann.screenshot({ path: __dirname + '/shots/party-lava.png' });
  }
  if (g === PG_ARCHERS) {
    await key(ann, '2');
    const a = await waitFor(ann, 'the bow', i => i.party.game === PG_ARCHERS && i.equip === 1, 20000).catch(e => null) || await info(ann);
    check(a.equip === 1, `one-shot archers: the bow and nothing else (equip ${a.equip})`);
  }
}
let ann;

async function key(page, k) {
  await page.keyboard.down(k);
  await sleep(800);
  await page.keyboard.up(k);
  await sleep(800);
}

// The vote panel by real clicks: Esc, V, then "Vote out" on hide and seek's row (the panel is 760 x 470,
// centered in a 960 x 540 page).
async function voteOutHide(page) {
  await key(page, 'Escape');
  await waitFor(page, 'the pause menu', i => i.panel === 4, 20000);
  await key(page, 'v');
  await waitFor(page, 'the party panel', i => i.panel === 10, 20000);
  await page.mouse.click(130 + 590 + 55, 35 + 70 + 66 * PG_HIDE + 15);
  await waitFor(page, 'the vote', i => i.party.vote === PG_HIDE, 20000).catch(e => null);
  await key(page, 'Escape');
}

(async () => {
  const browser = await launch();
  ann = await open(browser, ANN, URL);
  let a = await waitFor(ann, 'Ann joining', i => i.lobby === 0 && i.role === 2);
  let h = await hostInfo(ROOM);
  check(a.party.on === 1 && a.party.phase === PP_WAIT && h.guards === 0, `a party: waiting, no guards (${h.guards})`);
  await key(ann, 'Enter');
  await sleep(3000);
  h = await hostInfo(ROOM);
  check(h.party.phase === PP_WAIT, 'alone and ready: no party for one');
  await ann.screenshot({ path: __dirname + '/shots/party-wait.png' });

  const bob = await open(browser, BOB, URL);
  let b = await waitFor(bob, 'Bob joining', i => i.lobby === 0 && i.role === 2);
  b = await waitFor(bob, 'full food', i => i.party.food === 100, 20000).catch(e => null) || await info(bob);
  check(b.party.food === 100, `food is full (${b.party.food})`);
  await voteOutHide(bob);
  await voteOutHide(ann);
  h = await waitHost(ROOM, 'two votes', i => i.party.votes[PG_HIDE] === 2, 20000).catch(e => null);
  check(!!h, `both vote hide and seek out by clicks (${h ? h.party.votes : '?'})`);
  await ann.screenshot({ path: __dirname + '/shots/party-vote.png' });
  a = await info(ann);
  if (!a.party.ready) await key(ann, 'Enter');
  h = await hostInfo(ROOM);
  check(h.party.phase === PP_WAIT, 'Ann ready, Bob not: still waiting');
  await key(bob, 'Enter');
  h = await waitHost(ROOM, 'the countdown', i => i.party.phase === PP_COUNT, 20000).catch(e => null);
  check(!!h, 'both ready: the party counts down');
  h = await waitHost(ROOM, 'the first game', i => i.party.phase === PP_GAME, 30000).catch(e => null);
  check(!!h && h.party.game !== PG_HIDE, `ten seconds later the first game begins (game ${h ? h.party.game : '?'}), not the one voted out`);
  b = await waitFor(bob, 'Bob in the game', i => i.party.phase === PP_GAME && !i.party.ready, 30000).catch(e => null);
  check(!!b, 'Bob plays it, and is no longer ready for the next party');
  await sleep(2000);
  await ann.screenshot({ path: __dirname + '/shots/party-game.png' });

  // Cem joins mid-game: he watches, and plays from the next game.
  const cem = await open(browser, CEM, URL);
  let c = await waitFor(cem, 'Cem joining', i => i.lobby === 0 && i.role === 2);
  c = await waitFor(cem, 'Cem watching', i => i.party.out === 1, 20000).catch(e => null) || await info(cem);
  check(c.party.out === 1, `Cem, joining mid-game, watches (${c.party.out})`);

  const seen = [h ? h.party.game : -1];
  await inGame(seen[0]);
  const xp0 = (await info(ann)).xp, xpBob = (await info(bob)).xp;
  for (let k = 0; k < 4; k++) {
    await hostCmd(ROOM, 30, 239.5);
    h = await waitHost(ROOM, `game ${k + 1} over`, i => i.party.phase === (k < 3 ? PP_BREAK : PP_END), 30000).catch(e => null);
    check(!!h, `game ${k + 1} ends after its four minutes: ${k < 3 ? 'a break' : 'the party is over'} (totals ${h ? JSON.stringify(h.party.scores) : '?'})`);
    if (k < 3) {
      await hostCmd(ROOM, 30, 0.2);
      h = await waitHost(ROOM, `game ${k + 2}`, i => i.party.phase === PP_GAME, 30000).catch(e => null);
      if (h) {
        seen.push(h.party.game);
        await inGame(h.party.game);
      }
      if (k === 0) {
        c = await waitFor(cem, 'Cem in the next game', i => i.party.phase === PP_GAME && i.party.out === 0, 30000).catch(e => null);
        check(!!c, 'the next game Cem plays');
      }
    }
  }
  check(new Set(seen).size === 4 && !seen.includes(PG_HIDE), `four different games, hide and seek left out (${seen})`);
  a = await waitFor(ann, 'the party over at Ann', i => i.party.phase === PP_END, 30000).catch(e => null);
  check(!!a && /party is over/.test(a.msg + a.chat) || (!!a && a.party.phase === PP_END), `the party is over: "${a ? a.msg : ''}"`);
  const tot = h ? h.party.scores : [];
  check(tot.length === 3 && tot.every(t => t[2] > 0), `everyone has points for the party: ${JSON.stringify(tot)}`);
  a = await waitFor(ann, 'experience at Ann', i => i.xp > xp0, 20000).catch(e => null);
  b = await waitFor(bob, 'experience at Bob', i => i.xp > xpBob, 20000).catch(e => null);
  check(!!a && !!b, `the party's top three gain experience (Ann +${a ? a.xp - xp0 : '?'}, Bob +${b ? b.xp - xpBob : '?'})`);
  await sleep(2000);
  await ann.screenshot({ path: __dirname + '/shots/party-end.png' });
  await hostCmd(ROOM, 30, 0.2);
  h = await waitHost(ROOM, 'waiting again', i => i.party.phase === PP_WAIT || i.party.phase === PP_COUNT, 30000).catch(e => null);
  check(!!h && h.party.scores.every(t => t[1] === 0 && t[2] === 0), 'then the scores start again from nothing');

  await browser.close();
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('ERROR ' + e.message); process.exit(1); });
