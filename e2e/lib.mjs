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
    // The 8 s grace period that used to live here is gone: refreshAllUIState()
    // hung forever on backend.call('getRecordingTailMs'), an unregistered native
    // function, so everything after it (output folder, capture controls, capture
    // list) never refreshed. Both the stale call and the missing timeout are
    // fixed, so this should now resolve normally — if it starts timing out again,
    // that is a real regression and should not be papered over.
    await evalJsAsync(`handleStartupOpenProject(${JSON.stringify(path)})`, 30000);

    const name = await evalJs(`document.getElementById('project-name')?.textContent`);
    const expected = path.split('/').pop();
    if (name !== expected)
      throw new Error(`project did not load: UI shows '${name}', expected '${expected}'`);
    return name;
  }

  // Invoke a registered C++ native function and await its result. This is the
  // primary way to drive the app headlessly — it bypasses the DOM entirely, so
  // it never depends on UI wiring and never opens a modal chooser.
  async function call(name, ...args) {
    const argList = args.map((a) => JSON.stringify(a)).join(', ');
    return evalJsAsync(
      `backend.call(${JSON.stringify(name)}${argList ? ', ' + argList : ''})`,
      45000
    );
  }

  // Load a project by path through the bridge's chooser bypass, skipping the
  // startup-modal flow entirely. Preferred for unattended runs: no NSOpenPanel,
  // and no dependency on the UI having refreshed.
  //
  // deserializeProjectState DROPS THINGS SILENTLY — audioSettings if the device
  // name is not enumerated, any reference signal that is missing or whose sample
  // rate differs from the project's, and outputFolder if the directory does not
  // already exist. So callers must verify the loaded state by reading it back
  // (getAudioState / getReferenceSignals / getOutputFolderState), never by
  // assuming the load did what the file said.
  async function loadProjectDirect(path) {
    if (!(await call('isAudioInitialized'))) await call('initializeAudio');
    return call('loadProject', path);
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
    call,
    loadProjectDirect,
    selectDevice,
    listDevices,
  };
}
