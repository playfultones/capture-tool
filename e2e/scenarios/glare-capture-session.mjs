// Drive a real hardware capture session end to end, unattended.
//
// Built for the 2026-07-26 Glare session (59 reference signals, ~15.6 min of
// transport per Glare knob position), but nothing here is specific to it beyond
// the defaults — point --rcp at any project.
//
// THREE STAGES, deliberately separate and individually runnable:
//
//   --preflight   verify setup only, capture nothing. Loads the project through
//                 the bridge and reads every piece of state BACK, because
//                 deserializeProjectState drops things silently: audioSettings
//                 if the device name is not enumerated, any reference signal
//                 that is missing or whose sample rate differs, and outputFolder
//                 if the directory does not already exist. A project that
//                 "loaded fine" can be missing half its signals.
//
//   --smoke       capture a one-signal project and report the recorded RMS/peak.
//                 THIS IS THE ONE THAT MATTERS BEFORE A LONG RUN: without a TCC
//                 microphone grant, CoreAudio input does not error — it returns
//                 zeros. A full pass would complete "successfully" and write 15
//                 minutes of digital silence. Use with a project authored by
//                 author_rcp.py --smoke.
//
//   --run         run the capture list for real, one item at a time.
//
// Roundtrip: CaptureList::generate() always inserts a roundtrip item first, which
// re-records every reference signal a second time. Since the tool does not move
// the pedal's knobs, that item is not a true roundtrip — it is a duplicate pass
// through the same signal chain. Skipped unless --with-roundtrip is passed.
//
// Usage:
//   APP_PATH="$PWD/build/ReferenceCapturer_artefacts/Release/Reference Capturer.app/Contents/MacOS/Reference Capturer" \
//     node e2e/scenarios/glare-capture-session.mjs --preflight --rcp /path/to.rcp
//
// NOTE ON APP_PATH: use the Release bundle in build/. It is the one that holds
// the TCC microphone grant, and CMake now ad-hoc re-signs it with the real bundle
// identifier so that grant survives rebuilds. The Debug build-e2e bundle is a
// different identity and would need its own grant.

import {createHarness, outDir, sleep} from '../lib.mjs';
import {readFileSync, existsSync} from 'fs';
import {join} from 'path';

const argv = process.argv.slice(2);
const flag = (name) => argv.includes(`--${name}`);
const opt = (name, fallback = null) => {
  const i = argv.indexOf(`--${name}`);
  return i >= 0 && argv[i + 1] ? argv[i + 1] : fallback;
};

const RCP = opt(
  'rcp',
  '/Users/bence/Work/spline/cathedral-training/05_glare/capture_2026-07-26/glare_2026-07-26.rcp'
);
const MODE = flag('run') ? 'run' : flag('smoke') ? 'smoke' : 'preflight';
const WITH_ROUNDTRIP = flag('with-roundtrip');

const log = (...a) => console.log('[glare]', ...a);
const fail = (msg) => {
  throw new Error(msg);
};

/** Read a 24-bit or 32-bit PCM mono wav's RMS and peak, in dBFS. Enough to tell
 *  "real audio" from "digital silence", which is the only question --smoke asks. */
function wavLevelDbfs(path) {
  const buf = readFileSync(path);
  if (buf.toString('ascii', 0, 4) !== 'RIFF') fail(`${path}: not RIFF`);

  let pos = 12;
  let bitsPerSample = 0;
  let dataStart = -1;
  let dataLen = 0;
  while (pos + 8 <= buf.length) {
    const id = buf.toString('ascii', pos, pos + 4);
    const size = buf.readUInt32LE(pos + 4);
    if (id === 'fmt ') bitsPerSample = buf.readUInt16LE(pos + 22);
    else if (id === 'data') {
      dataStart = pos + 8;
      dataLen = size;
      break;
    }
    pos += 8 + size + (size % 2);
  }
  if (dataStart < 0) fail(`${path}: no data chunk`);

  // 16, 24 and 32-bit are all accepted: captures made before 2026-07-26 are
  // 16-bit (the recorder hardcoded it), newer ones are 24-bit.
  const bytes = bitsPerSample / 8;
  if (![2, 3, 4].includes(bytes)) fail(`${path}: unexpected ${bitsPerSample}-bit`);
  const full = Math.pow(2, bitsPerSample - 1);
  const n = Math.floor(Math.min(dataLen, buf.length - dataStart) / bytes);

  let sum = 0;
  let peak = 0;
  for (let i = 0; i < n; ++i) {
    const off = dataStart + i * bytes;
    const v = buf.readIntLE(off, bytes) / full;
    sum += v * v;
    const a = Math.abs(v);
    if (a > peak) peak = a;
  }
  const db = (x) => (x > 0 ? 20 * Math.log10(x) : -Infinity);
  return {
    frames: n,
    bitsPerSample,
    rmsDb: db(Math.sqrt(sum / Math.max(n, 1))),
    peakDb: db(peak),
  };
}

const h = createHarness();

try {
  if (!existsSync(RCP)) fail(`project not found: ${RCP}`);
  const expected = JSON.parse(readFileSync(RCP, 'utf8'));

  log(`mode=${MODE}  rcp=${RCP}`);
  // No args: AppConnection.launch() appends --e2e-test-port=<chosen port> itself
  // (app-connection.js:77). Passing one here would be a second, conflicting flag.
  await h.launch();
  await h.waitForAppReady();
  log('app ready, pid', h.state.pid);

  // ── Load, then read everything back ────────────────────────────────
  await h.call('initializeAudio');
  const loaded = await h.loadProjectDirect(RCP);
  if (loaded && loaded.success === false)
    fail(`loadProject failed: ${loaded.errorMessage ?? JSON.stringify(loaded)}`);

  const audio = await h.call('getAudioState');
  const want = expected.audioSettings;
  log('audio:', JSON.stringify(audio));
  for (const [key, expectedValue] of [
    ['inputDevice', want.inputDevice],
    ['outputDevice', want.outputDevice],
  ]) {
    if (audio?.[key] && audio[key] !== expectedValue)
      fail(
        `${key} is '${audio[key]}', project asked for '${expectedValue}' — ` +
          `deserialize drops audioSettings when the device name is not enumerated. ` +
          `Is the Fireface connected?`
      );
  }
  const sr = await h.call('getCurrentSampleRate');
  if (Number(sr) !== want.sampleRate)
    fail(`sample rate is ${sr}, project asked for ${want.sampleRate}`);

  // getReferenceSignals returns the whole panel state, not a bare array:
  // {signals, selectedId, isPlaying, isLooping}.
  const signalState = await h.call('getReferenceSignals');
  const signals = signalState?.signals ?? [];
  const got = Array.isArray(signals) ? signals.length : 0;
  const wantN = expected.referenceSignals.length;
  if (got !== wantN) {
    const gotNames = new Set(signals.map((s) => s.fileName));
    const missing = expected.referenceSignals
      .map((s) => s.fileName)
      .filter((n) => !gotNames.has(n));
    fail(
      `${got}/${wantN} reference signals loaded — ${missing.length} silently dropped ` +
        `(missing file, or sample rate != ${want.sampleRate}):\n  ` +
        missing.slice(0, 10).join('\n  ') +
        (missing.length > 10 ? `\n  ...and ${missing.length - 10} more` : '')
    );
  }
  log(`reference signals: ${got}/${wantN} — none dropped`);

  const folder = await h.call('getOutputFolderState');
  log('output folder:', JSON.stringify(folder));
  if (!folder || folder.isWritable === false || !folder.path)
    fail(
      `output folder not usable: ${JSON.stringify(folder)} — it must EXIST before ` +
        `the project is loaded or deserialize discards it, and startCapture then refuses`
    );

  const controls = await h.call('getCaptureControls');
  log('controls:', JSON.stringify(controls));

  await h.call('generateCaptureList');
  const list = await h.call('getCaptureList');
  if (!Array.isArray(list) || list.length === 0) fail('capture list is empty');
  const items = list.filter((it) => WITH_ROUNDTRIP || !it.isRoundtrip);
  log(
    `capture list: ${list.length} item(s), running ${items.length} ` +
      `(${list.length - items.length} roundtrip skipped)`
  );
  for (const it of items)
    log(`  item ${it.id} settings=${JSON.stringify(it.controlValues ?? {})}`);

  if (MODE === 'preflight') {
    await h.nativeScreenshot('glare-preflight.png');
    log('PREFLIGHT OK — setup verified, nothing captured.');
    log(`screenshot -> ${join(outDir, 'glare-preflight.png')}`);
  } else {
    // ── Capture ────────────────────────────────────────────────────
    // startCapture(itemId) runs EVERY reference signal for that item
    // unattended; C++ auto-advances and marks the item complete. So poll the
    // item's status rather than trying to await a single capture: DONE -> IDLE
    // happens inside one callAsync, so getCaptureState is unpollable.
    const perSignalMs = expected.referenceSignals.reduce(
      (acc, s) => acc + s.durationSeconds * 1000 + s.tailMs + 50,
      0
    );
    const budgetMs = Math.ceil(perSignalMs * 1.5) + 60000;
    log(`estimated ${(perSignalMs / 60000).toFixed(1)} min per item, ` +
        `timeout ${(budgetMs / 60000).toFixed(1)} min`);

    for (const item of items) {
      log(`starting item ${item.id}...`);
      const started = await h.call('startCapture', item.id);
      if (!started || started.success === false)
        fail(`startCapture refused: ${started?.errorMessage ?? JSON.stringify(started)}`);

      const deadline = Date.now() + budgetMs;
      let lastSeen = -1;
      for (;;) {
        if (h.state.exit) fail(`app exited mid-capture: ${JSON.stringify(h.state.exit)}`);
        const now = await h.call('getCaptureList');
        const cur = (now ?? []).find((it) => it.id === item.id);
        const done = (cur?.outputFilePaths ?? []).length;
        if (done !== lastSeen) {
          log(`  ${done}/${expected.referenceSignals.length} signals captured`);
          lastSeen = done;
        }
        if (cur?.status === 'complete') break;
        if (Date.now() > deadline)
          fail(`item ${item.id} did not complete within ${(budgetMs / 60000).toFixed(1)} min ` +
               `(status='${cur?.status}', ${done} files)`);
        await sleep(2000);
      }
      log(`item ${item.id} complete`);
    }

    // ── Prove it is not silence ────────────────────────────────────
    const finalList = await h.call('getCaptureList');
    const files = (finalList ?? []).flatMap((it) => it.outputFilePaths ?? []);
    log(`${files.length} file(s) written to ${folder.path}`);

    let silent = 0;
    const toCheck = MODE === 'smoke' ? files : files.slice(0, 5);
    for (const f of toCheck) {
      if (!existsSync(f)) fail(`capture reported '${f}' but it does not exist`);
      const {frames, rmsDb, peakDb} = wavLevelDbfs(f);
      const tag = peakDb === -Infinity ? '  <-- DIGITAL SILENCE' : '';
      log(`  ${f.split('/').pop()}: ${frames} frames, RMS ${rmsDb.toFixed(1)} dBFS, ` +
          `peak ${peakDb.toFixed(1)} dBFS${tag}`);
      if (peakDb === -Infinity || peakDb < -90) silent++;
    }
    if (silent > 0)
      fail(
        `${silent} of ${toCheck.length} checked file(s) are silent. The most likely ` +
          `cause is a missing TCC microphone grant: CoreAudio input returns zeros ` +
          `rather than erroring. Check the input channel and that this exact bundle ` +
          `has microphone permission.`
      );
    log(`${MODE.toUpperCase()} OK — ${toCheck.length} file(s) checked, all carry signal.`);
  }
} finally {
  await h.app.quit().catch(() => h.app.kill?.());
}
