// News from the host's game reaches a guest in the guest's language; a returning name is case-blind.
const { launch, open, info, cmd, waitFor, sleep } = require('./lib.js');
const BASE = 'http://localhost:8123/';
const ROOM = 'tr' + Date.now() % 100000;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
(async () => {
  const browser = await launch();
  const host = await open(browser, 'host', `${BASE}?auto=host&name=Ann&room=${ROOM}`);
  await waitFor(host, 'the host game', i => i.lobby === 0 && i.role === 1);
  const bob = await open(browser, 'bob', `${BASE}?auto=join&name=Bob&room=${ROOM}`);
  await waitFor(bob, 'Bob joining', i => i.lobby === 0 && i.role === 2);
  await cmd(bob, 11, 1);
  const cem = await open(browser, 'cem', `${BASE}?auto=join&name=Cem&room=${ROOM}`);
  await waitFor(cem, 'Cem joining', i => i.lobby === 0 && i.role === 2);
  const b = await waitFor(bob, 'Turkish news', i => /oyuna katıldı/.test(i.msg), 60000).catch(e => null);
  check(!!b, `Bob, playing in Turkish, reads "${b ? b.msg : ''}"`);
  const h = await info(host);
  check(/joined the game/.test(h.msg), `the host, in English, reads "${h.msg}"`);
  // A second "bob" while Bob plays is turned away.
  const dup = await open(browser, 'dup', `${BASE}?auto=join&name=bob&room=${ROOM}`);
  const d = await waitFor(dup, 'the refusal', i => i.failure.length > 0, 120000).catch(e => null);
  check(!!d && d.lobby === 1 && d.role === 0 && /already playing/.test(d.failure), `a second "bob" is turned away: "${d ? d.failure : ''}"`);
  await sleep(3000);
  await dup.screenshot({ path: __dirname + '/shots/dup.png' });
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  await browser.close();
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(2); });
