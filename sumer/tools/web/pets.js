// Cats and dogs in a shared game: fifteen of them; a cat given a fish (E) eats it and follows its friend, and
// the other player sees whose friend it is; a dog takes roast meat. None in a party.
const { BASE, RUN, launch, open, info, cmd, waitFor, sleep } = require('./lib.js');
const ROOM = 'survival-c' + RUN;
const URL = `${BASE}?auto=join&room=${ROOM}`;
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }
const IT_ROAST = 10, IT_FISH = 23, TGT_PET = 13;
const count = (i, item) => i.bag.filter(b => b[0] === item).reduce((n, b) => n + b[1], 0);

(async () => {
  const browser = await launch();
  const ann = await open(browser, 'Ann' + RUN, URL);
  let a = await waitFor(ann, 'Ann joining', i => i.lobby === 0 && i.role === 2, 120000);
  const cats = a.pets.filter(p => !p[3]).length, dogs = a.pets.filter(p => p[3]).length;
  check(cats === 8 && dogs === 7, `${cats} cats and ${dogs} dogs`);

  // The harbor's cat, a fish, E.
  await cmd(ann, 26, 0, 1.0);
  await cmd(ann, 7, IT_FISH, 1);
  a = await waitFor(ann, 'the cat in front', i => i.target === TGT_PET, 20000).catch(e => null) || await info(ann);
  check(a.target === TGT_PET && /fish/.test(a.promptE), `beside the cat: "${a.promptE}"`);
  await ann.screenshot({ path: __dirname + '/shots/pets-cat.png' });
  await ann.keyboard.press('e');
  a = await waitFor(ann, 'the cat fed', i => i.pets[0][4] === i.self, 20000).catch(e => null) || await info(ann);
  check(a.pets[0][4] === a.self && count(a, IT_FISH) === 0, `E: the fish is eaten, the cat is Ann's friend ("${a.msg}")`);
  const bob = await open(browser, 'Bob' + RUN, URL);
  const b = await waitFor(bob, 'Bob sees it', i => i.lobby === 0 && i.role === 2 && i.pets.length && i.pets[0][4] === a.self, 120000).catch(e => null) || await info(bob);
  check(b.pets[0][4] === a.self, `Bob, joining, sees whose friend the cat is (${b.pets[0][4]})`);
  await ann.bringToFront();
  const c = a.pets[0];
  await cmd(ann, 1, c[0] + 6, c[2]);
  a = await waitFor(ann, 'the cat following', i => Math.hypot(i.pets[0][0] - i.x, i.pets[0][2] - i.z) < 3, 90000).catch(e => null) || await info(ann);
  check(Math.hypot(a.pets[0][0] - a.x, a.pets[0][2] - a.z) < 3, `the cat follows Ann (${Math.hypot(a.pets[0][0] - a.x, a.pets[0][2] - a.z).toFixed(1)} m)`);
  await ann.screenshot({ path: __dirname + '/shots/pets-follow.png' });

  // The temple's dog: roast meat.
  const dog = a.pets.findIndex((p, k) => p[3] && Math.abs(p[0]) < 10 && p[2] < 0 && p[2] > -12);
  await cmd(ann, 26, dog, 1.2);
  await cmd(ann, 7, IT_ROAST, 1);
  a = await waitFor(ann, 'the dog in front', i => i.target === TGT_PET && i.targetIndex === dog, 20000).catch(e => null) || await info(ann);
  await ann.screenshot({ path: __dirname + '/shots/pets-dog.png' });
  await ann.keyboard.press('e');
  a = await waitFor(ann, 'the dog fed', i => i.pets[dog][4] === i.self, 20000).catch(e => null) || await info(ann);
  check(a.pets[dog][4] === a.self && count(a, IT_ROAST) === 0, `the temple's dog takes roast meat ("${a.promptE}" / "${a.msg}")`);

  const party = await open(browser, 'Cem' + RUN, `${BASE}?auto=join&room=party-c${RUN}`);
  const pc = await waitFor(party, 'a party', i => i.lobby === 0 && i.role === 2, 120000).catch(e => null) || await info(party);
  check(pc.pets.length === 0, `no cats or dogs in a party (${pc.pets.length})`);

  await browser.close();
  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('ERROR ' + e.message); process.exit(1); });
