// Helpers for driving the browser build in headless Chromium.
// Playwright is found through PLAYWRIGHT (the path of its node module) or the usual global install.
const { chromium } = require(process.env.PLAYWRIGHT || 'playwright');
const sleep = ms => new Promise(r => setTimeout(r, ms));

async function launch() {
  return chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--enable-webgl'] });
}

async function open(browser, name, url, log) {
  const ctx = await browser.newContext({ viewport: { width: 960, height: 540 } });
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

async function waitFor(page, what, pred, timeout = 90000) {
  const t0 = Date.now();
  let last = null;
  while (Date.now() - t0 < timeout) {
    last = await info(page);
    if (last && !last.error && pred(last)) return last;
    await sleep(250);
  }
  throw new Error(`${page.name}: timed out waiting for ${what}; last state ${JSON.stringify(last && { lobby: last.lobby, role: last.role, players: last.players, msg: last.msg, x: last.x, z: last.z })}`);
}

module.exports = { chromium, launch, open, info, cmd, waitFor, sleep };
