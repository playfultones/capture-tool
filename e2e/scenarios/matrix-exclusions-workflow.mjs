// Full UI workflow for capture-matrix exclusions, driven against the real glare
// project. Narrows the 90-combination matrix down to probe passes 1-5 (the
// fit-unblocking set: Mode=Middle, Shape=5, Gain=min, all Glare steps) using the
// real filter + bulk-action UI, generates the trimmed list, exercises the
// staleness banner + undo, and screenshots the matrix panel.
// Run: nix develop -c node scenarios/matrix-exclusions-workflow.mjs
import {createHarness, outDir, sleep} from '../lib.mjs';
import {copyFileSync} from 'fs';
import {join} from 'path';
import assert from 'assert';

const GLARE_PROJECT = '/Users/bence/Work/spline/captures/glare/glare.rcp';

const h = createHarness();
// Click a real DOM button and give its async click handler time to settle.
const clickAndSettle = async (id, ms = 500) => {
  await h.evalJs(`document.getElementById(${JSON.stringify(id)}).click()`);
  await sleep(ms);
};
const includedCount = () =>
  h.evalJs('matrixCombinationsState.combinations.filter((c) => c.included).length');
const staleHidden = () =>
  h.evalJs("document.getElementById('capture-list-stale-banner').classList.contains('hidden')");
const setFilters = (obj) =>
  h.evalJs(`(() => { matrixCombinationsState.filters = ${JSON.stringify(obj)}; renderMatrixCombinations(); return true; })()`);

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
  await clickAndSettle('matrix-exclude-shown');
  assert.strictEqual(await includedCount(), 0, 'all 90 excluded');

  // 2) Filter to the fit-unblocking region (passes 1-5): Mode=Middle, Shape=5,
  //    Gain=0; Glare unconstrained -> 5 rows. Then click "Include shown".
  await setFilters({Mode: 'Middle', Shape: '5', Gain: '0'});
  assert.strictEqual(
    await h.evalJs("document.querySelectorAll('#matrix-combinations-tbody tr').length"),
    5,
    'filtered region shows 5 rows (Glare 0,3,5,8,10)'
  );
  await clickAndSettle('matrix-include-shown');
  assert.strictEqual(await includedCount(), 5, 'exactly the 5 fit-unblocking passes included');
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
  assert.strictEqual(await h.evalJs('captureListState.items.length'), 6, 'list = 5 passes + roundtrip');

  // 4) Staleness UX: right after generate the list matches the selection -> banner hidden.
  assert.strictEqual(await staleHidden(), true, 'not stale immediately after generate');

  // Exclude the included region again (diverges from the generated list) -> stale shows.
  await setFilters({Mode: 'Middle', Shape: '5', Gain: '0'});
  await clickAndSettle('matrix-exclude-shown');
  assert.strictEqual(await staleHidden(), false, 'stale banner shows after post-generate selection change');

  // 5) Undo restores the prior selection -> matches the list again -> banner clears.
  await clickAndSettle('matrix-undo-btn');
  assert.strictEqual(await includedCount(), 5, 'undo restored the 5 included');
  assert.strictEqual(await staleHidden(), true, 'undo restores selection, banner clears');

  console.log('PASS: matrix-exclusions-workflow');
} catch (err) {
  console.error(`FAIL: ${err && err.stack ? err.stack : err}`);
  process.exitCode = 1;
} finally {
  await h.app.quit?.().catch(() => {});
  process.exit(process.exitCode ?? 0);
}
