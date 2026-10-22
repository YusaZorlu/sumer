// Music in the browser: the Web Audio output carries sound, the mood follows the game, N changes the volume.
const { chromium } = require('./lib.js');
const { info, cmd, waitFor, sleep } = require('./lib.js');
let fails = 0;
function check(ok, what) { console.log((ok ? 'PASS: ' : 'FAIL: ') + what); if (!ok) fails++; }

(async () => {
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--enable-webgl', '--autoplay-policy=user-gesture-required'] });
  const ctx = await browser.newContext({ viewport: { width: 960, height: 540 } });
  const page = await ctx.newPage();
  page.name = 'solo';
  page.on('pageerror', e => { console.log('PAGE ERROR ' + e); fails++; });
  page.on('console', m => { if (/audio|music|error/i.test(m.text())) console.log('[console] ' + m.text()); });
  await page.addInitScript(() => {
    window.__audio = { nodes: 0, calls: 0, sumSq: 0, n: 0, peak: 0, zero: 0 };
    const orig = AudioContext.prototype.createScriptProcessor;
    AudioContext.prototype.createScriptProcessor = function (...a) {
      const node = orig.apply(this, a);
      window.__audio.nodes++;
      window.__audio.ctx = this;
      let fn = null;
      Object.defineProperty(node, 'onaudioprocess', {
        get() { return fn; },
        set(f) {
          fn = f;
          node.addEventListener('audioprocess', () => {});
          Object.getPrototypeOf(node).__lookup;
          // wrap: run the game's handler, then measure what it wrote
          node._wrapped = e => {
            f.call(node, e);
            const A = window.__audio; A.calls++;
            let any = 0;
            for (let c = 0; c < e.outputBuffer.numberOfChannels; c++) {
              const d = e.outputBuffer.getChannelData(c);
              for (let i = 0; i < d.length; i++) { A.sumSq += d[i] * d[i]; A.n++; if (Math.abs(d[i]) > A.peak) A.peak = Math.abs(d[i]); if (d[i] !== 0) any = 1; }
            }
            if (!any) A.zero++;
          };
          node.addEventListener('audioprocess', node._wrapped);
        },
      });
      return node;
    };
  });
  await page.goto('http://localhost:8123/?auto=solo');
  await waitFor(page, 'game', s => s.lobby === 0 && s.role === 0, 120000);
  await page.mouse.click(480, 270);
  await sleep(2500);
  const a0 = await page.evaluate(() => ({ nodes: window.__audio.nodes, state: window.__audio.ctx && window.__audio.ctx.state, calls: window.__audio.calls, peak: window.__audio.peak }));
  console.log('audio after start', JSON.stringify(a0));
  check(a0.nodes >= 1, 'the game opens a Web Audio output');
  check(a0.state === 'running', 'the audio context is running after a click (' + a0.state + ')');

  async function rms() {
    const r = await page.evaluate(() => { const A = window.__audio; const o = { rms: A.n ? Math.sqrt(A.sumSq / A.n) : 0, peak: A.peak, calls: A.calls, zero: A.zero }; A.zero = 0; A.sumSq = 0; A.n = 0; A.peak = 0; return o; });
    return r;
  }
  await rms();
  await cmd(page, 9, 12);
  await sleep(6000);
  let s = await info(page), m = await rms();
  check(m.calls > 10 && m.rms > 0.005, `sound is playing in the morning (rms ${m.rms.toFixed(4)}, peak ${m.peak.toFixed(3)}, ${m.calls} callbacks)`);
  check(m.peak < 1.0, 'no clipping at the output (peak ' + m.peak.toFixed(3) + ')');
  // This browser renders about 5 frames a second and Web Audio shares its thread, so a few empty blocks are normal here.
  check(m.zero <= Math.max(2, m.calls * 0.05), `almost no dropouts while the music plays (${m.zero} empty callbacks of ${m.calls}, at about 5 frames a second)`);
  check(s.mood === 0, 'mood is the morning tune at noon (' + s.mood + ')');

  await cmd(page, 9, 23.5);
  s = await waitFor(page, 'night mood', x => x.mood === 2, 20000).catch(() => info(page));
  check(s.mood === 2, 'mood is the night tune at 23:30 (' + s.mood + ')');
  await sleep(5000); m = await rms();
  check(m.rms > 0.003, `sound is playing at night (rms ${m.rms.toFixed(4)})`);

  await cmd(page, 13, 3);
  s = await waitFor(page, 'fight mood', x => x.mood === 1, 5000).catch(() => info(page));
  check(s.mood === 1, 'mood is the fight tune after a blow (' + s.mood + ')');
  await sleep(6000); m = await rms();
  check(m.rms > 0.01, `the fight tune is louder than silence (rms ${m.rms.toFixed(4)})`);

  // N cycles the volume: medium -> high -> off -> low -> medium
  const before = (await info(page)).musicLevel;
  await page.keyboard.press('KeyN');
  await sleep(500);
  s = await info(page);
  check(s.musicLevel === (before + 1) % 4, `N changes the volume level (${before} -> ${s.musicLevel}, "${s.msg}")`);
  await page.keyboard.press('KeyN'); await sleep(400);
  s = await info(page);
  check(s.musicLevel === (before + 2) % 4, `N again (${s.musicLevel}, "${s.msg}")`);
  while ((await info(page)).musicLevel !== 0) { await page.keyboard.press('KeyN'); await sleep(300); }
  await sleep(1500); await rms(); await sleep(3000); m = await rms();
  check(m.rms < 0.001, `with the music off the output goes quiet (rms ${m.rms.toFixed(4)})`);
  while ((await info(page)).musicLevel !== 2) { await page.keyboard.press('KeyN'); await sleep(300); }

  await browser.close();
  console.log(fails ? fails + ' FAILED' : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('ERROR ' + e); process.exit(1); });
