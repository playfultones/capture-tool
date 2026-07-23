// Shared helpers for driving Reference Capturer through juce-end-to-end.
// The app's UI lives entirely inside a WKWebView, so all interaction goes
// through the custom evaluate-js / native-screenshot commands provided by
// Source/e2e/E2EBridge.cpp (built with -DE2E_TESTING=ON).

import {AppConnection} from '@focusritegroup/juce-end-to-end';
import {mkdirSync, writeFileSync} from 'fs';
import {dirname, join} from 'path';
import {fileURLToPath} from 'url';

const here = dirname(fileURLToPath(import.meta.url));

export const outDir = join(here, 'out');
export const defaultAppPath =
  process.env.APP_PATH ||
  join(
    here,
    '../build-e2e/ReferenceCapturer_artefacts/Debug/Reference Capturer.app/Contents/MacOS/Reference Capturer'
  );

export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

export function createHarness({appPath = defaultAppPath} = {}) {
  mkdirSync(outDir, {recursive: true});

  const app = new AppConnection({appPath, logDirectory: outDir});
  let seq = 0;

  // Resolved/rejected when the app process exits. `exit` stays null while the
  // app is running; a crash shows up as {error: 'App exited with signal: ...'}.
  const state = {pid: null, exit: null};

  async function launch(extraArgs = []) {
    await app.launch(extraArgs);
    state.pid = app['process']?.pid ?? null;
    writeFileSync(join(outDir, 'app.pid'), String(state.pid ?? ''));

    app
      .waitForExit()
      .then(() => (state.exit = {clean: true}))
      .catch((err) => (state.exit = {clean: false, error: String(err?.message ?? err)}));

    return state.pid;
  }

  async function evalJs(script, timeoutMs = 15000) {
    const id = `req-${++seq}`;
    let captured;
    const wait = app.waitForEvent(
      'js-response',
      (data) => (data.id === id ? ((captured = data), true) : false),
      timeoutMs
    );
    await app.sendCommand({type: 'evaluate-js', args: {id, script}});
    await wait;
    if (captured.error !== undefined) throw new Error(`JS error: ${captured.error}`);
    return captured.result;
  }

  // Run an async JS expression (a Promise) to completion. evaluate-js cannot
  // serialise a pending Promise, so stash the outcome on window and poll.
  async function evalJsAsync(promiseExpression, timeoutMs = 30000) {
    const key = `__e2eAsync${++seq}`;
    await evalJs(
      `(() => { window['${key}'] = {pending: true};
         Promise.resolve( ${promiseExpression} )
           .then(r => { window['${key}'] = {done: true, result: r === undefined ? null : r}; })
           .catch(e => { window['${key}'] = {failed: String(e)}; });
         return true; })()`
    );
    const deadline = Date.now() + timeoutMs;
    for (;;) {
      const status = await evalJs(`JSON.stringify(window['${key}'])`);
      const parsed = JSON.parse(status);
      if (parsed.done) {
        await evalJs(`delete window['${key}']`);
        return parsed.result;
      }
      if (parsed.failed) {
        await evalJs(`delete window['${key}']`);
        throw new Error(`async JS failed: ${parsed.failed}`);
      }
      if (Date.now() > deadline) throw new Error(`async JS timed out: ${promiseExpression}`);
      await sleep(150);
    }
  }

  async function waitForAppReady(timeoutMs = 15000) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      try {
        if (await evalJs(`!!document.getElementById('app')`, 3000)) return;
      } catch {
        // webview not ready to evaluate yet
      }
      await sleep(200);
    }
    throw new Error('app DOM never appeared');
  }

  async function nativeScreenshot(fileName = 'screenshot.png', timeoutMs = 15000) {
    const id = `shot-${++seq}`;
    let captured;
    const wait = app.waitForEvent(
      'native-screenshot',
      (data) => (data.id === id ? ((captured = data), true) : false),
      timeoutMs
    );
    await app.sendCommand({type: 'native-screenshot', args: {id}});
    await wait;
    if (captured.error) throw new Error(`native screenshot failed: ${captured.error}`);
    const path = join(outDir, fileName);
    writeFileSync(path, captured.image, 'base64');
    return path;
  }

  // Dismiss the startup modal via "New Project", or open a project from an
  // explicit path (chooser bypass wired through handleStartupOpenProject).
  async function newProject() {
    await evalJsAsync(`handleStartupNewProject()`);
  }

  async function openProject(path) {
    // KNOWN APP BUG: refreshAllUIState() hangs on backend.call('getRecordingTailMs'),
    // a native function that does not exist in C++ — the promise never resolves,
    // so everything after it (output folder, capture controls, capture list)
    // never refreshes. Until that's fixed, give the flow a short grace period,
    // then verify the load via DOM and warn loudly.
    try {
      await evalJsAsync(`handleStartupOpenProject(${JSON.stringify(path)})`, 8000);
    } catch (err) {
      if (!String(err).includes('timed out')) throw err;
      console.warn(
        `[harness] open-project promise did not resolve (known app bug: missing getRecordingTailMs native fn) — verifying via DOM`
      );
    }
    const name = await evalJs(`document.getElementById('project-name')?.textContent`);
    const expected = path.split('/').pop();
    if (name !== expected)
      throw new Error(`project did not load: UI shows '${name}', expected '${expected}'`);
    return name;
  }

  // Select an input/output device the way a user would: set the dropdown and
  // fire its change handler, which calls into the audio backend.
  async function selectDevice(kind, deviceName, channelIndex = null) {
    const selectId = kind === 'input' ? 'input-device' : 'output-device';
    const handler = kind === 'input' ? 'onInputDeviceChange' : 'onOutputDeviceChange';
    await evalJsAsync(
      `(() => { const s = document.getElementById('${selectId}');
         s.value = ${JSON.stringify(deviceName)};
         return ${handler}(); })()`,
      45000
    );
    if (channelIndex !== null) {
      const chId = kind === 'input' ? 'input-channel' : 'output-channel';
      const chHandler = kind === 'input' ? 'onInputChannelChange' : 'onOutputChannelChange';
      await evalJsAsync(
        `(() => { const s = document.getElementById('${chId}');
           s.value = String(${channelIndex});
           return ${chHandler}(); })()`,
        30000
      );
    }
  }

  async function listDevices(kind) {
    const selectId = kind === 'input' ? 'input-device' : 'output-device';
    const json = await evalJs(
      `JSON.stringify([...document.getElementById('${selectId}').options].map(o => o.value).filter(Boolean))`
    );
    return JSON.parse(json);
  }

  return {
    app,
    state,
    launch,
    evalJs,
    evalJsAsync,
    waitForAppReady,
    nativeScreenshot,
    newProject,
    openProject,
    selectDevice,
    listDevices,
  };
}
