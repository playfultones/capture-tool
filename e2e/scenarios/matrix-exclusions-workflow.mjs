// Full UI workflow for capture-matrix exclusions, driven against the real glare
// project. Narrows the 90-combination matrix down to probe passes 1-5 (the
// fit-unblocking set: Mode=Middle, Shape=5, Gain=min, all Glare steps) using the
// real filter + bulk-action UI, generates the trimmed list, exercises the
// staleness banner + undo, and screenshots the matrix panel.
// Run: nix develop -c node scenarios/matrix-exclusions-workflow.mjs
import {createHarness, outDir, sleep} from '../lib.mjs';
import {copyFileSync, readFileSync} from 'fs';
import {createHash} from 'crypto';
import {join} from 'path';
import assert from 'assert';

const GLARE_PROJECT = '/Users/bence/Work/spline/captures/glare/glare.rcp';
const md5 = (p) => createHash('md5').update(readFileSync(p)).digest('hex');

const h = createHarness();

// Poll a JS predicate (evaluated in the webview) until it is truthy. Deterministic
// replacement for a fixed sleep: a bulk button click dispatches an async handler
// that round-trips to C++ before mutating state, so we wait for the state, not a
// guessed duration.
const waitFor = async (predJs, label, timeoutMs = 6000) => {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    if (await h.evalJs(`!!(${predJs})`)) return;
    await sleep(50);
  }
  throw new Error(`waitFor timed out (${label}): ${predJs}`);
};
// Click a real DOM button, then wait for the expected state to settle.
const clickAndWait = async (id, predJs, label) => {
  await h.evalJs(`document.getElementById(${JSON.stringify(id)}).click()`);
  await waitFor(predJs, label);
};
const INCLUDED = 'matrixCombinationsState.combinations.filter((c) => c.included).length';
const includedCount = () => h.evalJs(INCLUDED);
const staleHidden = () =>
  h.evalJs("document.getElementById('capture-list-stale-banner').classList.contains('hidden')");
const setFilters = (obj) =>
  h.evalJs(`(() => { matrixCombinationsState.filters = ${JSON.stringify(obj)}; renderMatrixCombinations(); return true; })()`);

// Region for probe passes 1-5: Mode=Middle, Shape=5, Gain=min(0); Glare unconstrained.
const REGION = {Mode: 'Middle', Shape: '5', Gain: '0'};
const inRegion = (cv) => cv.Mode === 'Middle' && cv.Shape === '5' && cv.Gain === '0';

const glareChecksumBefore = md5(GLARE_PROJECT);

try {
  await h.launch();
  await h.waitForAppReady();

  // Open a COPY of the real project (the app auto-saves the open project when a
  // second is loaded; working from a copy keeps the original untouched).
  const wc = join(outDir, 'glare-working-copy.rcp');
  copyFileSync(GLARE_PROJECT, wc);
  await h.openProject(wc);

  await h.evalJsAsync('loadMatrixCombinations()');
  assert.strictEqual(await h.evalJs('matrixCombinationsState.combinations.length'), 90, 'glare matrix has 90 combinations');
  assert.strictEqual(await includedCount(), 90, 'all included by default');

  // 1) Exclude everything: no filter -> all 90 visible -> click "Exclude shown".
  await setFilters({});
  await clickAndWait('matrix-exclude-shown', `${INCLUDED} === 0`, 'exclude all');
  assert.strictEqual(await includedCount(), 0, 'all 90 excluded');

  // 2) Filter to the fit-unblocking region (passes 1-5): Mode=Middle, Shape=5,
  //    Gain=0; Glare unconstrained -> 5 rows. Then click "Include shown".
  await setFilters(REGION);
  assert.strictEqual(
    await h.evalJs("document.querySelectorAll('#matrix-combinations-tbody tr').length"),
    5,
    'filtered region shows 5 rows (Glare 0,3,5,8,10)'
  );
  await clickAndWait('matrix-include-shown', `${INCLUDED} === 5`, 'include region');
  assert.strictEqual(await includedCount(), 5, 'exactly the 5 fit-unblocking passes included');
  // Identity (not just count): the 5 included are exactly Mode=Middle/Shape=5/Gain=0,
  // one per distinct Glare step. Guards against a filter that selects the wrong 5.
  const includedCV = JSON.parse(
    await h.evalJs('JSON.stringify(matrixCombinationsState.combinations.filter((c) => c.included).map((c) => c.controlValues))')
  );
  assert.ok(includedCV.every(inRegion), 'every included combo is Mode=Middle,Shape=5,Gain=0');
  assert.deepStrictEqual(
    includedCV.map((cv) => cv.Glare).sort(),
    ['0', '10', '3', '5', '8'],
    'included combos span exactly the 5 Glare steps'
  );
  assert.strictEqual(
    await h.evalJs("document.getElementById('matrix-included-count').textContent"),
    '5 of 90 included',
    'count readout reflects 5 of 90'
  );

  // Dismiss the startup modal before screenshotting. It stays visually open
  // because the known getRecordingTailMs hang stops handleStartupOpenProject
  // from reaching hideStartupModal(); the project itself loaded fine.
  await h.evalJs(
    "(() => { document.getElementById('startup-modal')?.classList.add('hidden'); document.body.classList.remove('modal-open'); return true; })()"
  );
  // Screenshot the matrix panel with the filtered/included region visible.
  await h.nativeScreenshot('matrix-exclusions.png');

  // 3) Generate. Normal path (no completed captures, >0 included) -> no confirm.
  await h.evalJsAsync('generateCaptureList()');
  const items = JSON.parse(
    await h.evalJs('JSON.stringify(captureListState.items.map((i) => ({ rt: !!i.isRoundtrip, cv: i.controlValues })))')
  );
  assert.strictEqual(items.length, 6, 'list = 5 passes + roundtrip');
  assert.strictEqual(items.filter((i) => i.rt).length, 1, 'exactly one roundtrip item');
  const listedPasses = items.filter((i) => !i.rt);
  assert.strictEqual(listedPasses.length, 5, '5 non-roundtrip passes');
  assert.ok(listedPasses.every((i) => inRegion(i.cv)), 'generated passes are exactly the fit-unblocking region');

  // 4) Staleness UX: right after generate the list matches the selection -> banner hidden.
  assert.strictEqual(await staleHidden(), true, 'not stale immediately after generate');

  // Exclude the included region again (diverges from the generated list) -> stale shows.
  await setFilters(REGION);
  await clickAndWait('matrix-exclude-shown', `${INCLUDED} === 0`, 'exclude region post-generate');
  assert.strictEqual(await staleHidden(), false, 'stale banner shows after post-generate selection change');

  // 5) Undo restores the prior selection -> matches the list again -> banner clears.
  await clickAndWait('matrix-undo-btn', `${INCLUDED} === 5`, 'undo');
  assert.strictEqual(await includedCount(), 5, 'undo restored the 5 included');
  assert.strictEqual(await staleHidden(), true, 'undo restores selection, banner clears');

  console.log('PASS: matrix-exclusions-workflow');
} catch (err) {
  console.error(`FAIL: ${err && err.stack ? err.stack : err}`);
  process.exitCode = 1;
} finally {
  // Real-file safety: the scenario only ever opens/saves the working copy, never
  // GLARE_PROJECT. Guard against a regression that opens or writes the original.
  try {
    const after = md5(GLARE_PROJECT);
    if (after !== glareChecksumBefore) {
      console.error(`FAIL: REAL glare.rcp was modified! before=${glareChecksumBefore} after=${after}`);
      process.exitCode = 1;
    }
  } catch (e) {
    console.error(`glare.rcp integrity check errored: ${e}`);
  }
  await h.app.quit?.().catch(() => {});
  process.exit(process.exitCode ?? 0);
}
