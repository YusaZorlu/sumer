// An account plays on one server at a time: the same account in a second browser is turned away, whatever
// the case of its name. A browser that is not logged in cannot join.
const { BASE, RUN, launch, open, waitFor, sleep } = require('./lib.js');
const ROOM = 'survival-tr' + RUN;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
(async () => {
  const browser = await launch();
  const bob = await open(browser, 'Bob' + RUN, `${BASE}?auto=join&room=${ROOM}`);
  await waitFor(bob, 'Bob joining', i => i.lobby === 0 && i.role === 2);
  const dup = await open(browser, 'BOB' + RUN.toUpperCase(), `${BASE}?auto=join&room=${ROOM}`);
  const d = await waitFor(dup, 'the refusal', i => i.failure.length > 0, 120000).catch(e => null);
  check(!!d && d.lobby === 1 && d.role === 0 && /already playing/.test(d.failure), `Bob's account in a second browser is turned away: "${d ? d.failure : ''}"`);
  await sleep(2000);
  await dup.screenshot({ path: __dirname + '/shots/dup.png' });
  const anon = await open(browser, null, `${BASE}?auto=join&room=${ROOM}`);
  await sleep(6000);
  const answer = await anon.evaluate(room => new Promise(done => {
    const ws = new WebSocket(`ws://${location.host}/ws?room=${room}`);
    ws.binaryType = 'arraybuffer';
    ws.onmessage = e => { done(new Uint8Array(e.data)[0]); ws.close(); };
    ws.onerror = () => done(-1);
    setTimeout(() => done(-2), 10000);
  }), ROOM);
  check(answer === 9, `a browser not logged in is turned away (answer ${answer}, 9 means log in first)`);
  await anon.screenshot({ path: __dirname + '/shots/login.png' });
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  await browser.close();
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(2); });
