// Backend behavior for capture-matrix exclusions, driven against the REAL glare
// project. The glare probe session (cathedral-training/05_glare/probe/README.md)
// captures only 14 specific passes out of the full 90-combination matrix
// (Gain2 x Mode3 x Shape3 x Glare5). This scenario opens the saved project,
// excludes the 76 unwanted combinations, and verifies the 14-of-90 selection
// generates, persists, and survives control edits.
// Run: nix develop -c node scenarios/matrix-exclusions-backend.mjs
import {createHarness, outDir} from '../lib.mjs';
import {join} from 'path';
import {copyFileSync, readFileSync} from 'fs';
import assert from 'assert';
import {createHash} from 'crypto';

const GLARE_PROJECT = '/Users/bence/Work/spline/captures/glare/glare.rcp';

// The 14 wanted probe passes. README pass 6 uses Shape=2, which is not in the
// saved matrix's Shape axis {3,5,8}; per the session owner it maps to Shape=3.
// Gain "min" = "0", "~mid" = "5".
const WANTED = [
  {Mode: 'Middle', Glare: '0',  Shape: '5', Gain: '0'}, // pass 1
  {Mode: 'Middle', Glare: '3',  Shape: '5', Gain: '0'}, // pass 2
  {Mode: 'Middle', Glare: '5',  Shape: '5', Gain: '0'}, // pass 3
  {Mode: 'Middle', Glare: '8',  Shape: '5', Gain: '0'}, // pass 4
  {Mode: 'Middle', Glare: '10', Shape: '5', Gain: '0'}, // pass 5
  {Mode: 'Middle', Glare: '10', Shape: '3', Gain: '0'}, // pass 6 (Shape 2 -> 3)
  {Mode: 'Middle', Glare: '10', Shape: '8', Gain: '0'}, // pass 7
  {Mode: 'Middle', Glare: '10', Shape: '5', Gain: '5'}, // pass 8 (Gain ~mid)
  {Mode: 'Up',     Glare: '0',  Shape: '5', Gain: '0'}, // pass 9
  {Mode: 'Up',     Glare: '5',  Shape: '5', Gain: '0'}, // pass 10
  {Mode: 'Up',     Glare: '10', Shape: '5', Gain: '0'}, // pass 11
  {Mode: 'Down',   Glare: '0',  Shape: '5', Gain: '0'}, // pass 12
  {Mode: 'Down',   Glare: '5',  Shape: '5', Gain: '0'}, // pass 13
  {Mode: 'Down',   Glare: '10', Shape: '5', Gain: '0'}, // pass 14
];

const cvMatches = (cv, p) =>
  cv.Mode === p.Mode && cv.Glare === p.Glare && cv.Shape === p.Shape && cv.Gain === p.Gain;
const isWanted = (cv) => WANTED.some((p) => cvMatches(cv, p));

const md5 = (path) =>
  createHash('md5').update(readFileSync(path)).digest('hex');

const h = createHarness();
const call = (fn, ...a) =>
  h.evalJsAsync(`backend.call(${[JSON.stringify(fn), ...a.map((x) => JSON.stringify(x))].join(', ')})`);

function fail(msg) {
  console.error(`FAIL: ${msg}`);
  process.exitCode = 1;
}

// Snapshot glare.rcp before touching anything. All app operations work on a
// working copy; the original must be byte-for-byte identical afterwards.
const glareChecksumBefore = md5(GLARE_PROJECT);
// Working copy opened first: when the app later loads a second project it saves
// the currently open one — by working from a copy we ensure the real file is
// never the active project during that save.
const workingCopy = join(outDir, 'glare-working-copy.rcp');
copyFileSync(GLARE_PROJECT, workingCopy);

try {
  await h.launch();
  await h.waitForAppReady();

  // Start from a copy of the real saved project so the original is never open
  // in the app (and therefore never auto-saved back). The copy has the same
  // matrix: Gain(2) x Mode(3) x Shape(3) x Glare(5) = 90 combinations.
  await h.openProject(workingCopy);

  // Full matrix: Gain(2) x Mode(3) x Shape(3) x Glare(5) = 90 combinations.
  let combos = await call('getMatrixCombinations');
  assert.strictEqual(combos.length, 90, 'expected 90 combinations from glare project');
  assert.ok(combos.every((c) => c.included), 'all included by default');

  // Every one of the 14 wanted passes must exist in the matrix.
  for (const p of WANTED)
    assert.ok(combos.some((c) => cvMatches(c.controlValues, p)),
      `wanted pass not found in matrix: ${JSON.stringify(p)}`);

  // Exclude everything that is NOT one of the 14 wanted passes: 90 - 14 = 76.
  const excludeKeys = combos.filter((c) => !isWanted(c.controlValues)).map((c) => c.key);
  assert.strictEqual(excludeKeys.length, 76, 'expected 76 combinations to exclude');
  const res = await call('setCombinationsIncluded', excludeKeys, false);
  assert.strictEqual(res.includedCount, 14, 'includedCount should be exactly 14');
  assert.strictEqual(res.totalCount, 90, 'totalCount stays 90');

  combos = await call('getMatrixCombinations');
  const included = combos.filter((c) => c.included);
  assert.strictEqual(included.length, 14, 'exactly 14 included');
  assert.ok(included.every((c) => isWanted(c.controlValues)), 'only wanted passes are included');

  // Generate: roundtrip + 14 passes = 15 items; every non-roundtrip is a wanted pass.
  const gen = await call('generateCaptureList');
  assert.strictEqual(gen.count, 15, 'capture list should have 15 items (roundtrip + 14)');
  assert.ok(gen.captureList.some((i) => i.isRoundtrip), 'roundtrip present');
  const passes = gen.captureList.filter((i) => !i.isRoundtrip);
  assert.strictEqual(passes.length, 14, 'exactly 14 passes in the list');
  assert.ok(passes.every((i) => isWanted(i.controlValues)), 'list contains only wanted passes');

  // Persistence round-trip. Save the 14-of-90 selection to a named output file,
  // reload it, confirm the selection survives.
  const projectPath = join(outDir, 'glare-probe-14of90.rcp');
  await call('saveProjectAs', projectPath);
  await h.openProject(projectPath);
  combos = await call('getMatrixCombinations');
  assert.strictEqual(combos.filter((c) => c.included).length, 14, 'the 14 survive reload');
  assert.strictEqual(combos.filter((c) => !c.included).length, 76, '76 still excluded after reload');

  // Key stability: adding a VALUE to Glare keeps the still-valid exclusions
  // (all 76 excluded combos still exist).
  const controls = await call('getCaptureControls');
  const glare = controls.find((c) => c.name === 'Glare');
  assert.ok(glare && glare.id, 'found Glare control id');
  const upd = await call('updateCaptureControl', glare.id, 'Glare', 'discrete', '0, 3, 5, 8, 10, 12');
  assert.strictEqual(upd.strandedExcludedCount, 0, 'adding a Glare value strands nothing');
  combos = await call('getMatrixCombinations');
  assert.strictEqual(combos.filter((c) => !c.included).length, 76, 'exclusions survive value addition');

  // Stranded-key handling: adding a whole control invalidates every prior key.
  const add = await call('addCaptureControl', 'Tone', 'discrete', '2, 5');
  assert.strictEqual(add.strandedExcludedCount, 76, 'adding a control strands all 76 prior keys');
  combos = await call('getMatrixCombinations');
  assert.ok(combos.every((c) => c.included), 'all included after strand-and-prune');

  // Verify the real glare project was never touched.
  const glareChecksumAfter = md5(GLARE_PROJECT);
  assert.strictEqual(glareChecksumAfter, glareChecksumBefore,
    `REAL glare.rcp was modified! before=${glareChecksumBefore} after=${glareChecksumAfter}`);

  console.log('PASS: matrix-exclusions-backend');
  console.log(`glare.rcp integrity: ${glareChecksumBefore} (unchanged)`);
} catch (err) {
  fail(String(err && err.stack ? err.stack : err));
} finally {
  try {
    await h.evalJs('window.close && window.close()');
  } catch {}
  await h.app.quit?.().catch(() => {});
  process.exit(process.exitCode ?? 0);
}
