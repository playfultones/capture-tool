// Crash-hunt scenario: exercise the Audio Setup section by cycling through
// every output/input device and sample rate, the way a user would (DOM change
// handlers -> webbridge -> AudioEngine). Logs every step so that when the app
// dies, the last logged action identifies the trigger; the process exit
// (signal) is reported with it. Attach e2e/crash-watch.sh to the printed pid
// for a native backtrace at the moment of death.
//
//   node scenarios/audio-setup-sweep.mjs [--project /path/to.rcp] [--cycles N]
//                                        [--devices substring,substring]
//
// --devices filters which devices get switched to (substring match), useful to
// go straight at a suspect interface. Default: all listed devices.

import {createHarness, sleep} from '../lib.mjs';

const argv = process.argv.slice(2);
const argValue = (flag) => (argv.includes(flag) ? argv[argv.indexOf(flag) + 1] : null);

const projectPath = argValue('--project');
const cycles = Number(argValue('--cycles') ?? 1);
const deviceFilter = (argValue('--devices') ?? '').split(',').filter(Boolean);
const attachWait = Number(argValue('--attach-wait') ?? 0);

const h = createHarness();
let lastAction = 'launch';

function crashed() {
  return h.state.exit !== null;
}

async function step(label, fn) {
  lastAction = label;
  console.log(`[sweep] ${label}`);
  await fn();
  if (crashed()) throw new Error('app exited during step');
}

try {
  const pid = await h.launch();
  console.log(`[sweep] app pid=${pid}  (crash-watch: ./crash-watch.sh ${pid})`);
  await h.waitForAppReady();

  if (attachWait > 0) {
    console.log(`[sweep] waiting ${attachWait}s for a debugger to attach...`);
    await sleep(attachWait * 1000);
  }

  if (projectPath) {
    await step(`open project ${projectPath}`, async () => {
      const name = await h.openProject(projectPath);
      console.log(`[sweep] project loaded: ${name}`);
    });
  } else {
    await step('new project (dismiss startup modal)', () => h.newProject());
  }

  await step('expand audio setup panel', () =>
    h.evalJs(
      `(() => { const p = document.getElementById('audio-setup');
         if (p.classList.contains('collapsed')) document.getElementById('audio-setup-header').click();
         return true; })()`
    )
  );

  const matches = (name) =>
    deviceFilter.length === 0 || deviceFilter.some((f) => name.toLowerCase().includes(f.toLowerCase()));

  const outputs = (await h.listDevices('output')).filter(matches);
  const inputs = (await h.listDevices('input')).filter(matches);
  console.log(`[sweep] sweeping ${outputs.length} output + ${inputs.length} input devices, ${cycles} cycle(s)`);

  for (let cycle = 1; cycle <= cycles; cycle++) {
    console.log(`[sweep] --- cycle ${cycle}/${cycles} ---`);

    for (const device of outputs) {
      await step(`output device -> ${device}`, () => h.selectDevice('output', device));
      await sleep(250);
    }

    for (const device of inputs) {
      await step(`input device -> ${device}`, () => h.selectDevice('input', device));
      await sleep(250);
    }

    const rates = JSON.parse(
      await h.evalJs(
        `JSON.stringify([...document.getElementById('sample-rate').options].map(o => o.value).filter(Boolean))`
      )
    );
    for (const rate of rates) {
      await step(`sample rate -> ${rate}`, () =>
        h.evalJsAsync(
          `(() => { const s = document.getElementById('sample-rate');
             s.value = ${JSON.stringify(rate)};
             return onSampleRateChange(); })()`
        )
      );
      await sleep(250);
    }
  }

  await h.nativeScreenshot('sweep-final.png');
  console.log('[sweep] PASS: no crash, screenshot at out/sweep-final.png');
  await h.app.quit();
} catch (err) {
  if (crashed()) {
    console.error(`[sweep] CRASH after action: ${lastAction}`);
    console.error(`[sweep] exit: ${JSON.stringify(h.state.exit)}`);
    console.error('[sweep] if crash-watch.sh was attached, backtrace is in out/crash-bt.txt');
    process.exit(2);
  }
  console.error(`[sweep] FAILED during '${lastAction}': ${err?.message ?? err}`);
  h.app.kill();
  process.exit(1);
}
