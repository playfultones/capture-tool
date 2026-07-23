// PoC: drive the WebView UI of Reference Capturer through juce-end-to-end's
// custom-command channel (evaluate-js in C++, see Source/e2e/E2EBridge.cpp).
//
// Phases (announced on stdout so an observer can coordinate):
//   1. launch app, write out/app.pid
//   2. wait for DOM ready, dump UI state
//   3. click #audio-setup-header, verify the collapsed class toggles
//   4. save a JUCE-side screenshot (expected mostly blank over the WKWebView)
//   5. HOLD for 60s (observer may attach lldb / take native screenshots)
//   6. quit cleanly

import {AppConnection} from '@focusritegroup/juce-end-to-end';
import {mkdirSync, writeFileSync} from 'fs';
import {dirname, join} from 'path';
import {fileURLToPath} from 'url';

const here = dirname(fileURLToPath(import.meta.url));
const outDir = join(here, 'out');
mkdirSync(outDir, {recursive: true});

const appPath = join(
  here,
  '../build-e2e/ReferenceCapturer_artefacts/Debug/Reference Capturer.app/Contents/MacOS/Reference Capturer'
);

const app = new AppConnection({appPath, logDirectory: outDir});

let seq = 0;
async function evalJs(script) {
  const id = `req-${++seq}`;
  let captured;
  const wait = app.waitForEvent(
    'js-response',
    (data) => (data.id === id ? ((captured = data), true) : false),
    15000
  );
  await app.sendCommand({type: 'evaluate-js', args: {id, script}});
  await wait;
  if (captured.error !== undefined) throw new Error(`JS error: ${captured.error}`);
  return captured.result;
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

console.log('[poc] launching app...');
await app.launch();
const pid = app['process']?.pid;
writeFileSync(join(outDir, 'app.pid'), String(pid ?? ''));
console.log(`[poc] launched, pid=${pid}`);

// Wait for the webview to finish loading the UI. The initial blank page
// already reports readyState 'complete', so poll for the app's root element.
let domReady = false;
for (let i = 0; i < 50; i++) {
  try {
    if (await evalJs(`!!document.getElementById('app')`)) {
      domReady = true;
      break;
    }
  } catch (e) {
    // webview may not be ready to evaluate yet
  }
  await sleep(200);
}
if (!domReady) {
  const diag = await evalJs(
    `JSON.stringify({href: location.href, ready: document.readyState, htmlLength: document.documentElement.outerHTML.length})`
  );
  throw new Error(`app DOM never appeared: ${diag}`);
}
console.log('[poc] DOM ready');

const uiState = await evalJs(`JSON.stringify({
  title: document.title,
  projectName: document.getElementById('project-name')?.textContent,
  saveStatus: document.getElementById('save-status')?.textContent,
  sections: [...document.querySelectorAll('[id]')].slice(0, 12).map(e => e.id),
  audioSetupCollapsed: document.getElementById('audio-setup')?.classList.contains('collapsed'),
  inputDeviceOptions: [...(document.getElementById('input-device')?.options ?? [])].map(o => o.textContent),
})`);
console.log('[poc] UI state:', uiState);

// Interaction: toggle the audio-setup panel and verify the class change
const before = await evalJs(
  `document.getElementById('audio-setup').classList.contains('collapsed')`
);
await evalJs(`document.getElementById('audio-setup-header').click()`);
await sleep(300);
const after = await evalJs(
  `document.getElementById('audio-setup').classList.contains('collapsed')`
);
console.log(
  `[poc] collapse toggle: before=${before} after=${after} -> ${
    before !== after ? 'OK' : 'FAILED'
  }`
);

// JUCE-side screenshot (empty component-id = whole main window)
await app.saveScreenshot('', 'juce-snapshot.png');
console.log('[poc] JUCE screenshot saved to out/juce-snapshot.png');

// Native WKWebView snapshot via the custom bridge command (no TCC needed)
{
  const id = `shot-${++seq}`;
  let captured;
  const wait = app.waitForEvent(
    'native-screenshot',
    (data) => (data.id === id ? ((captured = data), true) : false),
    15000
  );
  await app.sendCommand({type: 'native-screenshot', args: {id}});
  await wait;
  if (captured.error) {
    console.log('[poc] native screenshot FAILED:', captured.error);
  } else {
    writeFileSync(join(outDir, 'native-snapshot.png'), captured.image, 'base64');
    console.log('[poc] native screenshot saved to out/native-snapshot.png');
  }
}

const holdSeconds = Number(process.env.POC_HOLD_SECONDS ?? 5);
console.log(`[poc] HOLD: keeping app alive for ${holdSeconds}s (lldb/screencapture window)...`);
await sleep(holdSeconds * 1000);

console.log('[poc] quitting app');
await app.quit();
console.log('[poc] done, app exited cleanly');
