// Helpers for driving the browser build in headless Chromium, against the test server (serve.sh).
// Playwright is found through PLAYWRIGHT (the path of its node module) or the usual global install.
const { chromium } = require(process.env.PLAYWRIGHT || 'playwright');
const sleep = ms => new Promise(r => setTimeout(r, ms));
const BASE = process.env.BASE || 'http://localhost:8123/';
// A few letters of this run's own, for its accounts and rooms: every run starts with new ones.
const RUN = Math.random().toString(36).slice(2, 6);

async function launch() {
  return chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--enable-webgl'] });
}

// The browser context logs in to account 'name' (made if new; the test server asks for no work).
async function login(ctx, name, password = 'sumer-test') {
  const ch = await ctx.request.post(BASE + 'api/challenge');
  const challenge = (await ch.text()).split('\t')[0];
  let r = await ctx.request.post(BASE + 'api/create', { data: { name, password, challenge, nonce: 0 } });
  if (r.status() === 409) r = await ctx.request.post(BASE + 'api/login', { data: { name, password } });
  if (r.status() !== 200) throw new Error(`could not log in as ${name}: ${r.status()} ${await r.text()}`);
  return r.text();
}

// A player of their own: a browser context logged in as 'name' (unless name is null), on 'url'.
async function open(browser, name, url, log, options) {
  const ctx = await browser.newContext(options || { viewport: { width: 960, height: 540 } });
  if (name) await login(ctx, name);
  const page = await ctx.newPage();
  page.on('console', m => { if (log) console.log(`[${name}] ${m.text()}`); });
  page.on('pageerror', e => console.log(`[${name}] PAGE ERROR ${e}`));
  await page.goto(url);
  page.name = name;
  return page;
}

async function info(page) {
  return page.evaluate(() => {
    if (!window.Module || !Module.ccall || !Module.ctx) return null;
    try { return JSON.parse(Module.ccall('TestInfo', 'string', [], [])); } catch (e) { return { error: String(e) }; }
  });
}

async function cmd(page, c, a = 0, b = 0) {
  return page.evaluate(([c, a, b]) => Module.ccall('TestCommand', null, ['number', 'number', 'number'], [c, a, b]), [c, a, b]);
}

// The world as the room's server program has it (tools/web/hosthooks.c), and its commands.
async function hostInfo(room) {
  const r = await fetch(`${BASE}test/host?room=${room}`, { method: 'POST', body: 'info' });
  return r.ok ? r.json() : null;
}

async function hostCmd(room, c, a = 0, b = 0) {
  return fetch(`${BASE}test/host?room=${room}`, { method: 'POST', body: `cmd ${c} ${a} ${b}` });
}

// The room's server program stops (saving its world), as when it stands empty a while.
async function stopRoom(room) {
  return fetch(`${BASE}test/stop?room=${room}`, { method: 'POST' });
}

async function waitFor(page, what, pred, timeout = 90000) {
  const t0 = Date.now();
  let last = null;
  while (Date.now() - t0 < timeout) {
    last = await info(page);
    if (last && !last.error && pred(last)) return last;
    await sleep(250);
  }
  throw new Error(`${page.name}: timed out waiting for ${what}; last state ${JSON.stringify(last && { lobby: last.lobby, role: last.role, players: last.players, msg: last.msg, failure: last.failure, note: last.note, x: last.x, z: last.z })}`);
}

// Until the room's server program says so.
async function waitHost(room, what, pred, timeout = 60000) {
  const t0 = Date.now();
  let last = null;
  while (Date.now() - t0 < timeout) {
    last = await hostInfo(room).catch(() => null);
    if (last && pred(last)) return last;
    await sleep(250);
  }
  throw new Error(`room ${room}: timed out waiting for ${what}`);
}

module.exports = { chromium, BASE, RUN, launch, login, open, info, cmd, hostInfo, hostCmd, stopRoom, waitFor, waitHost, sleep };
