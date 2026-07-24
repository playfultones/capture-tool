# Capture Matrix Exclusions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the user selectively exclude combinations from the capture matrix so "Generate List" produces a clean capture list of only the wanted combinations (plus the mandatory roundtrip).

**Architecture:** Exclusion is matrix-state, stored as a set of canonical combination keys on `CaptureControlManager` — not on `CaptureItem`. A new combinations table in the matrix panel (checkboxes + filter chips + bulk include/exclude) programs the exclusions; `generateCaptureList` skips excluded combinations. The capture list keeps its current UX (just fewer rows) plus two non-interactive readouts (trace-back + staleness).

**Tech Stack:** JUCE 8 (C++), `juce::var`/`DynamicObject` JSON, `playfultones_webbridge` native functions, vanilla JS/HTML/CSS in a WKWebView, juce-end-to-end for tests.

**Spec:** `docs/superpowers/specs/2026-07-24-capture-matrix-exclusions-design.md`

---

## Testing strategy (read first)

This app has **no C++ unit-test framework**. The real test layer is the **e2e harness** (`e2e/lib.mjs` → `createHarness()` → `evalJsAsync("backend.call(...)")`), which drives the running app's native functions and DOM. Consequently:

- **C++ tasks** use the compile as their first gate:
  `busybee -- nix develop -c cmake --build build-e2e --target ReferenceCapturer`
  (the e2e build is configured with `-DE2E_TESTING=ON`; see project CLAUDE.md).
- **Behavior** is verified by e2e scenario scripts under `e2e/scenarios/`, run with
  `nix develop -c node scenarios/<name>.mjs` from `e2e/`.
- **Frontend** is verified by DOM assertions and native screenshots through the same harness.

Where the writing-plans skill says "write the failing test first," the backend equivalent is: the e2e scenario in **Task 5** calls native functions that do not yet exist and must **fail** (time out) before Tasks 1–4 are built, then **pass** after. Build the e2e app once per backend task; run the scenario at Task 5.

Assume the e2e app is built at least once already (`build-e2e/…/Reference Capturer.app`). If not:
```bash
nix develop -c cmake -B build-e2e -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DE2E_TESTING=ON
busybee -- nix develop -c cmake --build build-e2e --target ReferenceCapturer
```

---

## File structure

**Modified (C++):**
- `Source/capture/CaptureControl.h` — `MatrixCombination` struct; exclusion API + `excludedKeys` on `CaptureControlManager`.
- `Source/capture/CaptureControl.cpp` — implement the new methods.
- `Source/capture/CaptureList.cpp` — `generate()` enumerates via `getCombinations()` and skips excluded.
- `Source/MainComponent.cpp` — serialize/deserialize `excludedKeys`; new native functions; `strandedExcludedCount` on control mutators; e2e save-to-path bypass.

**Modified (frontend):**
- `Resources/webapp/index.html` — combinations table + filter/bulk DOM in the Capture Matrix panel; trace-back + staleness elements in the capture-list panel.
- `Resources/webapp/js/capture.js` — combinations state/render/filter/bulk/undo; key helper; stranded warning; staleness; generate confirmations.
- `Resources/webapp/css/capture.css` — styles for the new elements.

**Created (tests):**
- `e2e/scenarios/matrix-exclusions-backend.mjs` — backend behavior scenario.
- `e2e/scenarios/matrix-exclusions-workflow.mjs` — glare 14-of-90 UI workflow + screenshot.

---

## Task 1: Combination model + exclusion API on CaptureControlManager

**Files:**
- Modify: `Source/capture/CaptureControl.h`
- Modify: `Source/capture/CaptureControl.cpp`

- [ ] **Step 1: Add the `MatrixCombination` struct and exclusion members/methods to the header**

In `CaptureControl.h`, immediately above `class CaptureControlManager`, add:

```cpp
/**
 * One enumerated cell of the capture matrix (a specific combination of
 * control values). Not a capture item and not the roundtrip.
 */
struct MatrixCombination
{
    juce::String key;                     // canonical key (see makeCombinationKey)
    juce::StringPairArray controlValues;  // control name -> value
};
```

Inside `class CaptureControlManager`, in the `public:` section (after `getTotalCaptureCount`), add declarations:

```cpp
    /** Total number of combinations (== getTotalCaptureCount). */
    int getTotalCombinationCount() const { return getTotalCaptureCount(); }

    /**
     * Canonical key for a combination: "name=value" for each control, in the
     * controls' defined order, joined by the unit-separator char (31).
     */
    juce::String makeCombinationKey(const juce::StringPairArray& controlValues) const;

    /**
     * Enumerate the full cartesian product (no roundtrip). Empty when there are
     * no controls or the product exceeds 100000.
     */
    juce::Array<MatrixCombination> getCombinations() const;

    /** True if this combination key is currently excluded. */
    bool isExcluded(const juce::String& key) const { return excludedKeys.contains(key); }

    /** Add/remove a key from the excluded set. */
    void setExcluded(const juce::String& key, bool excluded);

    /** Number of combinations NOT excluded. */
    int getIncludedCount() const;

    /**
     * Drop excluded keys that no longer match any current combination (after a
     * control edit). Returns the number removed.
     */
    int pruneStrandedKeys();

    const juce::StringArray& getExcludedKeys() const { return excludedKeys; }
    void setExcludedKeys(const juce::StringArray& keys) { excludedKeys = keys; }
```

In the `private:` section (after `juce::Array<CaptureControl> controls;`) add:

```cpp
    juce::StringArray excludedKeys;   // canonical keys currently excluded
```

- [ ] **Step 2: Implement the methods in the .cpp**

Append to `CaptureControl.cpp` (after `toVar()`):

```cpp
juce::String CaptureControlManager::makeCombinationKey(const juce::StringPairArray& controlValues) const
{
    static const juce::String sep = juce::String::charToString(static_cast<juce::juce_wchar>(31));

    juce::String key;
    for (int i = 0; i < controls.size(); ++i)
    {
        if (i > 0)
            key << sep;
        key << controls[i].name << "=" << controlValues[controls[i].name];
    }
    return key;
}

juce::Array<MatrixCombination> CaptureControlManager::getCombinations() const
{
    juce::Array<MatrixCombination> result;

    if (controls.isEmpty())
        return result;

    const int total = getTotalCombinationCount();
    if (total == 0 || total > 100000)
        return result;

    juce::Array<int> indices;
    indices.resize(controls.size());
    for (int i = 0; i < indices.size(); ++i)
        indices.set(i, 0);

    while (true)
    {
        MatrixCombination combo;
        for (int i = 0; i < controls.size(); ++i)
            combo.controlValues.set(controls[i].name, controls[i].values[indices[i]]);
        combo.key = makeCombinationKey(combo.controlValues);
        result.add(combo);

        int position = indices.size() - 1;
        while (position >= 0)
        {
            indices.set(position, indices[position] + 1);
            if (indices[position] < controls[position].values.size())
                break;
            indices.set(position, 0);
            position--;
        }
        if (position < 0)
            break;
    }

    return result;
}

void CaptureControlManager::setExcluded(const juce::String& key, bool excluded)
{
    if (excluded)
    {
        if (!excludedKeys.contains(key))
            excludedKeys.add(key);
    }
    else
    {
        excludedKeys.removeString(key);
    }
}

int CaptureControlManager::getIncludedCount() const
{
    int included = 0;
    for (const auto& combo : getCombinations())
        if (!isExcluded(combo.key))
            ++included;
    return included;
}

int CaptureControlManager::pruneStrandedKeys()
{
    juce::StringArray valid;
    for (const auto& combo : getCombinations())
        valid.add(combo.key);

    int removed = 0;
    for (int i = excludedKeys.size(); --i >= 0;)
    {
        if (!valid.contains(excludedKeys[i]))
        {
            excludedKeys.remove(i);
            ++removed;
        }
    }
    return removed;
}
```

- [ ] **Step 3: Build (compile gate)**

Run:
```bash
busybee -- nix develop -c cmake --build build-e2e --target ReferenceCapturer
```
Expected: builds clean. (No behavior wired in yet — this is API only.)

- [ ] **Step 4: Commit**

```bash
git add Source/capture/CaptureControl.h Source/capture/CaptureControl.cpp
git commit -m "Add matrix combination enumeration and exclusion API"
```

---

## Task 2: generate() enumerates via getCombinations and skips excluded

**Files:**
- Modify: `Source/capture/CaptureList.cpp:13-94` (the `generate` method)

- [ ] **Step 1: Replace the body of `CaptureListManager::generate`**

Replace the whole method (from `void CaptureListManager::generate` through its closing brace) with:

```cpp
void CaptureListManager::generate(const CaptureControlManager& controlManager)
{
    items.clear();

    // Always add the roundtrip entry first (unit bypassed, no control values).
    CaptureItem roundtripItem;
    roundtripItem.id = generateItemId();
    roundtripItem.index = 1;
    roundtripItem.status = CaptureStatus::PENDING;
    roundtripItem.isRoundtrip = true;
    items.add(roundtripItem);

    // Emit one item per included combination, in enumeration order.
    int captureIndex = 2; // roundtrip is index 1
    for (const auto& combo : controlManager.getCombinations())
    {
        if (controlManager.isExcluded(combo.key))
            continue;

        CaptureItem item;
        item.id = generateItemId();
        item.index = captureIndex++;
        item.status = CaptureStatus::PENDING;
        item.isRoundtrip = false;
        item.controlValues = combo.controlValues;
        items.add(item);
    }
}
```

This preserves prior behavior when nothing is excluded (roundtrip + full product, same order) and DRYs the cartesian logic into `getCombinations()`.

- [ ] **Step 2: Build (compile gate)**

Run:
```bash
busybee -- nix develop -c cmake --build build-e2e --target ReferenceCapturer
```
Expected: builds clean.

- [ ] **Step 3: Commit**

```bash
git add Source/capture/CaptureList.cpp
git commit -m "Generate capture list via shared enumeration, skipping excluded combinations"
```

---

## Task 3: Persist excludedKeys + e2e save-to-path bypass

**Files:**
- Modify: `Source/MainComponent.cpp:1966-1968` (serialize matrix), `:2114-2153` (deserialize matrix), `saveProjectAs` (~`:1098`).

- [ ] **Step 1: Write excludedKeys into the serialized matrix**

Replace the "Matrix (Controls)" serialize line (currently
`projectObj->setProperty("matrix", ProjectSerializer::serializeControls(captureControlManager.getControls()));`)
with:

```cpp
    //--------------------------------------------------------------------------
    // Matrix (Controls + exclusions) - use ProjectSerializer helper, then attach
    // the matrix-level excludedKeys set.
    auto matrixVar = ProjectSerializer::serializeControls(captureControlManager.getControls());
    if (auto* matrixObj = matrixVar.getDynamicObject())
        matrixObj->setProperty("excludedKeys",
                               ProjectSerializer::stringArrayToVar(captureControlManager.getExcludedKeys()));
    projectObj->setProperty("matrix", matrixVar);
```

- [ ] **Step 2: Read excludedKeys back on load and prune**

In `deserializeProjectState`, inside the `if (matrix != nullptr)` block, immediately **after** the `for (const auto& ctrlVar : *controlsArray)` loop that re-adds controls (i.e. after the closing brace of that inner loop, still inside the `matrix != nullptr` block), add:

```cpp
                // Restore matrix-level exclusions (empty for legacy projects).
                juce::StringArray excluded;
                auto excludedVar = matrix->getProperty("excludedKeys");
                if (auto* excludedArray = excludedVar.getArray())
                    for (const auto& v : *excludedArray)
                        excluded.add(v.toString());
                captureControlManager.setExcludedKeys(excluded);
                captureControlManager.pruneStrandedKeys();
```

- [ ] **Step 3: Add an e2e-only save-to-path branch to `saveProjectAs`**

The native chooser cannot be driven by the harness, so mirror the `loadProject` e2e injection. Inside the `saveProjectAs` lambda, **before** the `fileChooser = std::make_unique<juce::FileChooser>(...)` line, add:

```cpp
#if REFCAP_E2E
                // e2e automation: save to an explicit path one level below the
                // chooser. Only honored under the e2e driver (--e2e-test-port).
                if (E2EBridge::isRequested() && !args.isEmpty() && args[0].isString())
                {
                    auto file = juce::File(args[0].toString());
                    if (!file.hasFileExtension(".rcp") && !file.hasFileExtension(".json"))
                        file = file.withFileExtension(".rcp");

                    auto projectData = serializeProjectState();
                    auto jsonString = juce::JSON::toString(projectData, true);

                    auto* resultObj = new juce::DynamicObject();
                    if (file.replaceWithText(jsonString))
                    {
                        currentProjectFile = file;
                        resultObj->setProperty("success", true);
                        resultObj->setProperty("filePath", file.getFullPathName());
                    }
                    else
                    {
                        resultObj->setProperty("success", false);
                        resultObj->setProperty("errorMessage", "Failed to write project file");
                    }
                    complete(juce::var(resultObj));
                    return;
                }
#endif
```

Note: the lambda signature is currently `[this](const juce::Array<juce::var>& /*args*/, Completion complete)`. Change `/*args*/` to `args` so the parameter is named.

- [ ] **Step 4: Build (compile gate)**

Run:
```bash
busybee -- nix develop -c cmake --build build-e2e --target ReferenceCapturer
```
Expected: builds clean.

- [ ] **Step 5: Commit**

```bash
git add Source/MainComponent.cpp
git commit -m "Persist matrix exclusions; add e2e save-to-path bypass"
```

---

## Task 4: Native functions — enumerate, toggle, stranded count

**Files:**
- Modify: `Source/MainComponent.cpp` — add two native functions near the other matrix functions (after `getTotalCaptureCount`, ~`:883`); extend `addCaptureControl`/`removeCaptureControl`/`updateCaptureControl` responses.

- [ ] **Step 1: Add `getMatrixCombinations` and `setCombinationsIncluded`**

After the `getTotalCaptureCount` `.withNativeFunction(...)` block, add:

```cpp
        // Enumerate all matrix combinations with current include state.
        .withNativeFunction(
            "getMatrixCombinations",
            [this](const juce::Array<juce::var>& /*args*/, Completion complete) {
                juce::Array<juce::var> result;
                for (const auto& combo : captureControlManager.getCombinations())
                {
                    auto* obj = new juce::DynamicObject();
                    obj->setProperty("key", combo.key);

                    auto* valuesObj = new juce::DynamicObject();
                    for (int i = 0; i < combo.controlValues.size(); ++i)
                        valuesObj->setProperty(juce::Identifier(combo.controlValues.getAllKeys()[i]),
                                               combo.controlValues.getAllValues()[i]);
                    obj->setProperty("controlValues", juce::var(valuesObj));
                    obj->setProperty("included", !captureControlManager.isExcluded(combo.key));
                    result.add(juce::var(obj));
                }
                complete(juce::var(result));
            })

        // Include/exclude a set of combinations by key.
        // Args: [keys: string[], included: bool]
        .withNativeFunction(
            "setCombinationsIncluded",
            [this](const juce::Array<juce::var>& args, Completion complete) {
                auto* resultObj = new juce::DynamicObject();
                if (args.size() < 2 || !args[0].isArray())
                {
                    resultObj->setProperty("success", false);
                    resultObj->setProperty("error", "requires keys[] and included");
                    complete(juce::var(resultObj));
                    return;
                }

                const bool included = static_cast<bool>(args[1]);
                for (const auto& k : *args[0].getArray())
                    captureControlManager.setExcluded(k.toString(), !included);
                markProjectDirty();

                resultObj->setProperty("success", true);
                resultObj->setProperty("includedCount", captureControlManager.getIncludedCount());
                resultObj->setProperty("totalCount", captureControlManager.getTotalCombinationCount());
                complete(juce::var(resultObj));
            })
```

- [ ] **Step 2: Report stranded exclusions from the three control mutators**

In `addCaptureControl`, after `markProjectDirty();` add `int stranded = captureControlManager.pruneStrandedKeys();` and, on `resultObj`, add:
```cpp
                resultObj->setProperty("strandedExcludedCount", stranded);
                resultObj->setProperty("includedCount", captureControlManager.getIncludedCount());
```
Do the same in `removeCaptureControl` and `updateCaptureControl` — in each, after their `if (success) markProjectDirty();`, add `int stranded = captureControlManager.pruneStrandedKeys();` and set the same two properties on their `resultObj`. (Adding a control strands all prior keys, which is correct — the warning will fire.)

- [ ] **Step 3: Build (compile gate)**

Run:
```bash
busybee -- nix develop -c cmake --build build-e2e --target ReferenceCapturer
```
Expected: builds clean.

- [ ] **Step 4: Commit**

```bash
git add Source/MainComponent.cpp
git commit -m "Add matrix combination native functions and stranded-exclusion reporting"
```

---

## Task 5: Backend e2e scenario (behavior gate for Tasks 1–4)

**Files:**
- Create: `e2e/scenarios/matrix-exclusions-backend.mjs`

- [ ] **Step 1: Write the scenario**

```javascript
// Backend behavior for capture-matrix exclusions. Drives native functions
// directly through the webview bridge. Run: nix develop -c node scenarios/matrix-exclusions-backend.mjs
import {createHarness, outDir} from '../lib.mjs';
import {join} from 'path';
import assert from 'assert';

const h = createHarness();
const call = (fn, ...a) =>
  h.evalJsAsync(`backend.call(${[JSON.stringify(fn), ...a.map((x) => JSON.stringify(x))].join(', ')})`);

function fail(msg) {
  console.error(`FAIL: ${msg}`);
  process.exitCode = 1;
}

try {
  await h.launch();
  await h.waitForAppReady();
  await h.newProject();

  // Two small controls: 2 x 3 = 6 combinations.
  await call('addCaptureControl', 'Mode', 'discrete', 'Up, Down');
  await call('addCaptureControl', 'Glare', 'discrete', '0, 5, 10');

  let combos = await call('getMatrixCombinations');
  assert.strictEqual(combos.length, 6, 'expected 6 combinations');
  assert.ok(combos.every((c) => c.included), 'all included by default');

  // Exclude the two Mode=Down combos at Glare 0 and 10 via their keys.
  const toExclude = combos
    .filter((c) => c.controlValues.Mode === 'Down' && c.controlValues.Glare !== '5')
    .map((c) => c.key);
  assert.strictEqual(toExclude.length, 2, 'expected 2 keys to exclude');
  let res = await call('setCombinationsIncluded', toExclude, false);
  assert.strictEqual(res.includedCount, 4, 'includedCount should drop to 4');

  combos = await call('getMatrixCombinations');
  assert.strictEqual(combos.filter((c) => !c.included).length, 2, 'two excluded');

  // Generate: roundtrip + 4 included = 5 items, no excluded combos present.
  const gen = await call('generateCaptureList');
  assert.strictEqual(gen.count, 5, 'capture list should have 5 items');
  const list = gen.captureList;
  assert.ok(list.some((i) => i.isRoundtrip), 'roundtrip present');
  const excludedInList = list.filter(
    (i) => !i.isRoundtrip && i.controlValues.Mode === 'Down' && i.controlValues.Glare !== '5'
  );
  assert.strictEqual(excludedInList.length, 0, 'excluded combos absent from list');

  // Persistence round-trip (e2e save-to-path bypass + loadProject path arg).
  const projectPath = join(outDir, 'exclusions-test.rcp');
  await call('saveProjectAs', projectPath);
  await call('loadProject', projectPath);
  combos = await call('getMatrixCombinations');
  assert.strictEqual(combos.filter((c) => !c.included).length, 2, 'exclusions survive reload');

  // Stranded-key warning: adding a control strands all prior exclusions.
  const add = await call('addCaptureControl', 'Shape', 'discrete', '2, 5');
  assert.ok(add.strandedExcludedCount >= 2, 'adding a control strands prior keys');
  combos = await call('getMatrixCombinations');
  assert.ok(combos.every((c) => c.included), 'all included after strand-and-prune');

  console.log('PASS: matrix-exclusions-backend');
} catch (err) {
  fail(String(err && err.stack ? err.stack : err));
} finally {
  try {
    await h.evalJs('window.close && window.close()');
  } catch {}
  await h.app.quit?.().catch(() => {});
  process.exit(process.exitCode ?? 0);
}
```

- [ ] **Step 2: Run it — expect PASS (Tasks 1–4 are built)**

Run:
```bash
cd e2e && nix develop -c node scenarios/matrix-exclusions-backend.mjs
```
Expected: `PASS: matrix-exclusions-backend`, exit 0.

If you ran this *before* building Tasks 1–4, it fails/times out on the missing `getMatrixCombinations` — that is the red state. It must be green now.

- [ ] **Step 3: Commit**

```bash
git add e2e/scenarios/matrix-exclusions-backend.mjs
git commit -m "Add e2e backend scenario for matrix exclusions"
```

---

## Task 6: Frontend scaffold — key helper, state, HTML, CSS

**Files:**
- Modify: `Resources/webapp/js/capture.js` (near `captureControlsState`, ~`:529`)
- Modify: `Resources/webapp/index.html` (Capture Matrix panel, ~`:294-307`)
- Modify: `Resources/webapp/css/capture.css` (append)

- [ ] **Step 1: Add the key helper and combinations state to capture.js**

Immediately after the `window.captureControlsState = captureControlsState;` line, add:

```javascript
// Canonical combination key, mirroring C++ CaptureControlManager::makeCombinationKey:
// "name=value" per control in defined order, joined by the unit-separator char.
const MATRIX_KEY_SEP = '';
function makeCombinationKey(controlValues) {
    const controls = (window.captureControlsState && window.captureControlsState.controls) || [];
    return controls.map((c) => `${c.name}=${controlValues[c.name] != null ? controlValues[c.name] : ''}`).join(MATRIX_KEY_SEP);
}
window.makeCombinationKey = makeCombinationKey;

const matrixCombinationsState = {
    combinations: [],   // [{ key, controlValues:{name:value}, included }]
    filters: {},        // controlName -> selected value, or '' for any
    lastBulkUndo: null,  // { keys:[], included:bool } — prior state for one-level undo
};
window.matrixCombinationsState = matrixCombinationsState;
```

- [ ] **Step 2: Add the combinations-table DOM to index.html**

Keep the existing `<div class="capture-count-display">…</div>` block untouched — `updateCaptureControlsDisplay()` early-returns if `#total-capture-count` is missing, which would break the controls panel. **Insert** the following block *immediately after* the closing `</div>` of `capture-count-display` (and before `<div class="capture-list-actions">`):

```html
                        <div class="matrix-combinations" id="matrix-combinations">
                            <div class="matrix-combinations-header">
                                <span class="matrix-combinations-title">Combinations</span>
                                <span class="matrix-included-count" id="matrix-included-count">0 of 0 included</span>
                            </div>
                            <div class="matrix-combinations-hint">Exclusions apply on the next Generate List.</div>

                            <div class="matrix-filter-bar" id="matrix-filter-bar"></div>

                            <div class="matrix-bulk-bar">
                                <span class="matrix-filter-status hidden" id="matrix-filter-status"></span>
                                <button class="btn-secondary" id="matrix-include-shown">Include shown</button>
                                <button class="btn-secondary" id="matrix-exclude-shown">Exclude shown</button>
                            </div>

                            <div class="matrix-combinations-table-wrapper">
                                <table class="matrix-combinations-table" id="matrix-combinations-table">
                                    <thead><tr id="matrix-combinations-thead"></tr></thead>
                                    <tbody id="matrix-combinations-tbody"></tbody>
                                </table>
                            </div>

                            <div class="matrix-undo-toast hidden" id="matrix-undo-toast">
                                <span id="matrix-undo-text"></span>
                                <button class="btn-link" id="matrix-undo-btn">Undo</button>
                            </div>
                        </div>
```

- [ ] **Step 3: Append styles to capture.css**

```css
/* ---- Capture matrix combinations table ---- */
.matrix-combinations { margin-top: 12px; }
.matrix-combinations-header { display: flex; justify-content: space-between; align-items: baseline; }
.matrix-included-count { font-variant-numeric: tabular-nums; opacity: 0.85; }
.matrix-combinations-hint { font-size: 11px; opacity: 0.6; margin: 2px 0 8px; }
.matrix-filter-bar { display: flex; flex-wrap: wrap; gap: 6px; margin-bottom: 6px; }
.matrix-filter-chip { display: inline-flex; align-items: center; gap: 4px; font-size: 12px; }
.matrix-bulk-bar { display: flex; align-items: center; gap: 8px; margin-bottom: 6px; }
.matrix-filter-status { font-size: 12px; opacity: 0.75; }
.matrix-filter-status .clear-filter { margin-left: 6px; cursor: pointer; text-decoration: underline; }
.matrix-combinations-table-wrapper { max-height: 320px; overflow: auto; border: 1px solid rgba(255,255,255,0.08); }
.matrix-combinations-table { width: 100%; border-collapse: collapse; font-size: 12px; }
.matrix-combinations-table th, .matrix-combinations-table td { padding: 3px 8px; text-align: left; }
.matrix-combinations-table tbody tr.excluded { opacity: 0.4; }
.matrix-undo-toast { display: flex; align-items: center; gap: 10px; margin-top: 6px; font-size: 12px; }
.btn-link { background: none; border: none; color: inherit; text-decoration: underline; cursor: pointer; padding: 0; }
.hidden { display: none !important; }

/* ---- Capture-list trace-back + staleness ---- */
.capture-list-included-readout { font-size: 12px; opacity: 0.8; }
.capture-list-stale-banner { font-size: 12px; margin: 4px 0; padding: 4px 8px; border-radius: 4px;
    background: rgba(255,180,0,0.15); }

/* ---- Confirm dialog ---- */
.app-confirm-overlay { position: fixed; inset: 0; background: rgba(0,0,0,0.5);
    display: flex; align-items: center; justify-content: center; z-index: 1000; }
.app-confirm-box { background: #23262b; padding: 18px 20px; border-radius: 8px; max-width: 380px; }
.app-confirm-actions { display: flex; justify-content: flex-end; gap: 8px; margin-top: 14px; }
```

Note: if `.hidden` already exists in capture.css, do not duplicate it — keep the existing rule.

- [ ] **Step 4: Commit**

```bash
git add Resources/webapp/js/capture.js Resources/webapp/index.html Resources/webapp/css/capture.css
git commit -m "Scaffold matrix combinations table DOM, state, and styles"
```

---

## Task 7: Render combinations table + counts + refresh + stranded warning

**Files:**
- Modify: `Resources/webapp/js/capture.js`

- [ ] **Step 1: Add render + load functions**

Add these functions to capture.js (near the other capture-controls functions, e.g. after `updateCaptureControlsDisplay`):

```javascript
async function loadMatrixCombinations() {
    try {
        const combos = await backend.call('getMatrixCombinations');
        matrixCombinationsState.combinations = combos || [];
        // Drop filters that reference controls/values no longer present.
        const controlNames = new Set((captureControlsState.controls || []).map((c) => c.name));
        for (const name of Object.keys(matrixCombinationsState.filters))
            if (!controlNames.has(name)) delete matrixCombinationsState.filters[name];
        renderMatrixCombinations();
    } catch (err) {
        console.error('Failed to load matrix combinations:', err);
    }
}

function getVisibleCombinations() {
    const filters = matrixCombinationsState.filters;
    return matrixCombinationsState.combinations.filter((combo) =>
        Object.entries(filters).every(([name, val]) => !val || combo.controlValues[name] === val)
    );
}

function renderMatrixCombinations() {
    const controls = captureControlsState.controls || [];
    const combos = matrixCombinationsState.combinations;

    // Counts.
    const total = combos.length;
    const included = combos.filter((c) => c.included).length;
    const countEl = document.getElementById('matrix-included-count');
    if (countEl) countEl.textContent = `${included} of ${total} included`;

    // Filter chips (single-select dropdowns).
    const filterBar = document.getElementById('matrix-filter-bar');
    if (filterBar) {
        filterBar.innerHTML = controls
            .map((c) => {
                const opts = ['<option value="">any</option>']
                    .concat((c.values || []).map((v) => `<option value="${escapeHtml(v)}">${escapeHtml(v)}</option>`))
                    .join('');
                const sel = matrixCombinationsState.filters[c.name] || '';
                return `<label class="matrix-filter-chip">${escapeHtml(c.name)}
                    <select data-filter-control="${escapeHtml(c.name)}">${opts}</select></label>`;
            })
            .join('');
        filterBar.querySelectorAll('select[data-filter-control]').forEach((sel) => {
            sel.value = matrixCombinationsState.filters[sel.dataset.filterControl] || '';
            sel.addEventListener('change', () => {
                matrixCombinationsState.filters[sel.dataset.filterControl] = sel.value;
                renderMatrixCombinations();
            });
        });
    }

    // Filter status banner.
    const anyFilter = Object.values(matrixCombinationsState.filters).some((v) => v);
    const visible = getVisibleCombinations();
    const statusEl = document.getElementById('matrix-filter-status');
    if (statusEl) {
        statusEl.classList.toggle('hidden', !anyFilter);
        if (anyFilter)
            statusEl.innerHTML = `Showing ${visible.length} of ${total} (filtered)<span class="clear-filter" id="matrix-clear-filter">clear</span>`;
    }
    const clearEl = document.getElementById('matrix-clear-filter');
    if (clearEl)
        clearEl.addEventListener('click', () => {
            matrixCombinationsState.filters = {};
            renderMatrixCombinations();
        });

    // Bulk button labels with live counts.
    const incBtn = document.getElementById('matrix-include-shown');
    const excBtn = document.getElementById('matrix-exclude-shown');
    if (incBtn) incBtn.textContent = `Include ${visible.length} shown`;
    if (excBtn) excBtn.textContent = `Exclude ${visible.length} shown`;

    // Header.
    const thead = document.getElementById('matrix-combinations-thead');
    if (thead)
        thead.innerHTML =
            '<th></th>' + controls.map((c) => `<th>${escapeHtml(c.name)}</th>`).join('');

    // Body (visible rows only).
    const tbody = document.getElementById('matrix-combinations-tbody');
    if (tbody) {
        tbody.innerHTML = visible
            .map((combo) => {
                const cells = controls
                    .map((c) => `<td>${escapeHtml(combo.controlValues[c.name] || '')}</td>`)
                    .join('');
                return `<tr class="${combo.included ? '' : 'excluded'}" data-combo-key="${escapeHtml(combo.key)}">
                    <td><input type="checkbox" class="matrix-combo-check" ${combo.included ? 'checked' : ''}></td>
                    ${cells}</tr>`;
            })
            .join('');
        tbody.querySelectorAll('tr').forEach((row) => {
            const cb = row.querySelector('.matrix-combo-check');
            if (cb) cb.addEventListener('change', () => onCombinationCheckboxChange(row.dataset.comboKey, cb.checked));
        });
    }

    // Defined in Task 9; guard so this works before Task 9 is executed.
    if (typeof updateCaptureListStaleness === 'function') updateCaptureListStaleness();
}
```

- [ ] **Step 2: Wire the single-checkbox toggle**

```javascript
async function onCombinationCheckboxChange(key, checked) {
    try {
        const res = await backend.call('setCombinationsIncluded', [key], checked);
        const combo = matrixCombinationsState.combinations.find((c) => c.key === key);
        if (combo) combo.included = checked;
        if (res && typeof res.includedCount === 'number') {
            const countEl = document.getElementById('matrix-included-count');
            if (countEl) countEl.textContent = `${res.includedCount} of ${res.totalCount} included`;
        }
        renderMatrixCombinations();
    } catch (err) {
        console.error('Failed to toggle combination:', err);
    }
}
```

- [ ] **Step 3: Refresh combinations + surface stranded warning on every control edit**

In `onControlFieldChange`, `onControlTypeChange`, `onAddControlClick`, and `onRemoveControlClick`, each already does `captureControlsState.controls = result.controls; captureControlsState.totalCaptureCount = result.totalCaptureCount; updateCaptureControlsDisplay();`. Immediately after `updateCaptureControlsDisplay();` in each, add:

```javascript
        if (result.strandedExcludedCount > 0 && typeof showConfirmDialog === 'function') {
            showConfirmDialog(
                `This edit reset the include/exclude selection for ${result.strandedExcludedCount} combination${result.strandedExcludedCount !== 1 ? 's' : ''}.`,
                { okOnly: true }
            );
        }
        await loadMatrixCombinations();
```

(`showConfirmDialog` is defined in Task 10; the `typeof` guard keeps this safe if Tasks 7–9 are exercised before Task 10. The warning simply won't display until Task 10 lands, which is acceptable during incremental execution.)

- [ ] **Step 4: Load combinations on startup and after generate**

In `initCaptureControls` (after wiring the add button) and in `loadCaptureControls` (after setting state), add `loadMatrixCombinations();`. In `generateCaptureList` success branch (after `updateCaptureListDisplay();`), also call `loadMatrixCombinations();` so counts/staleness reconcile.

- [ ] **Step 5: Verify in-app (DOM)**

Build the e2e app (already built) and run a quick check:
```bash
cd e2e && nix develop -c node -e "import('./lib.mjs').then(async ({createHarness}) => {
  const h = createHarness(); await h.launch(); await h.waitForAppReady(); await h.newProject();
  await h.evalJsAsync(\"backend.call('addCaptureControl','Mode','discrete','Up, Down')\");
  await h.evalJsAsync('loadMatrixCombinations()');
  const rows = await h.evalJs(\"document.querySelectorAll('#matrix-combinations-tbody tr').length\");
  console.log('rows:', rows); await h.app.quit?.(); process.exit(rows==2?0:1);
});"
```
Expected: `rows: 2`, exit 0.

- [ ] **Step 6: Commit**

```bash
git add Resources/webapp/js/capture.js
git commit -m "Render matrix combinations table with counts, filters, and edit refresh"
```

---

## Task 8: Filter bulk actions + undo toast + init wiring

**Files:**
- Modify: `Resources/webapp/js/capture.js`

- [ ] **Step 1: Add bulk + undo functions**

```javascript
async function bulkSetShown(included) {
    const visible = getVisibleCombinations();
    if (visible.length === 0) return;
    const keys = visible.map((c) => c.key);

    // Capture prior state for one-level undo (only the affected keys).
    matrixCombinationsState.lastBulkUndo = {
        entries: visible.map((c) => ({ key: c.key, included: c.included })),
    };

    try {
        const res = await backend.call('setCombinationsIncluded', keys, included);
        for (const combo of matrixCombinationsState.combinations)
            if (keys.includes(combo.key)) combo.included = included;
        renderMatrixCombinations();
        showUndoToast(`${included ? 'Included' : 'Excluded'} ${keys.length}`);
        if (res && typeof res.includedCount === 'number') {
            const countEl = document.getElementById('matrix-included-count');
            if (countEl) countEl.textContent = `${res.includedCount} of ${res.totalCount} included`;
        }
    } catch (err) {
        console.error('Bulk set failed:', err);
    }
}

function showUndoToast(text) {
    const toast = document.getElementById('matrix-undo-toast');
    const label = document.getElementById('matrix-undo-text');
    if (!toast || !label) return;
    label.textContent = text;
    toast.classList.remove('hidden');
    clearTimeout(showUndoToast._t);
    showUndoToast._t = setTimeout(() => toast.classList.add('hidden'), 6000);
}

async function undoLastBulk() {
    const undo = matrixCombinationsState.lastBulkUndo;
    if (!undo) return;
    // Restore each key to its prior state. Group by target value to minimise calls.
    const toInclude = undo.entries.filter((e) => e.included).map((e) => e.key);
    const toExclude = undo.entries.filter((e) => !e.included).map((e) => e.key);
    if (toInclude.length) await backend.call('setCombinationsIncluded', toInclude, true);
    if (toExclude.length) await backend.call('setCombinationsIncluded', toExclude, false);
    for (const combo of matrixCombinationsState.combinations) {
        const e = undo.entries.find((x) => x.key === combo.key);
        if (e) combo.included = e.included;
    }
    matrixCombinationsState.lastBulkUndo = null;
    document.getElementById('matrix-undo-toast')?.classList.add('hidden');
    renderMatrixCombinations();
}
```

- [ ] **Step 2: Wire the bulk/undo buttons in initCaptureControls**

In `initCaptureControls`, add:

```javascript
    document.getElementById('matrix-include-shown')?.addEventListener('click', () => bulkSetShown(true));
    document.getElementById('matrix-exclude-shown')?.addEventListener('click', () => bulkSetShown(false));
    document.getElementById('matrix-undo-btn')?.addEventListener('click', undoLastBulk);
```

- [ ] **Step 3: Verify (DOM)**

```bash
cd e2e && nix develop -c node -e "import('./lib.mjs').then(async ({createHarness}) => {
  const h = createHarness(); await h.launch(); await h.waitForAppReady(); await h.newProject();
  await h.evalJsAsync(\"backend.call('addCaptureControl','Glare','discrete','0, 5, 10')\");
  await h.evalJsAsync('loadMatrixCombinations()');
  await h.evalJsAsync('bulkSetShown(false)');
  const inc = await h.evalJs('matrixCombinationsState.combinations.filter(c=>c.included).length');
  console.log('included after exclude-all:', inc); await h.app.quit?.(); process.exit(inc==0?0:1);
});"
```
Expected: `included after exclude-all: 0`, exit 0.

- [ ] **Step 4: Commit**

```bash
git add Resources/webapp/js/capture.js
git commit -m "Add bulk include/exclude and one-level undo for matrix combinations"
```

---

## Task 9: Capture-list trace-back readout + staleness banner

**Files:**
- Modify: `Resources/webapp/index.html` (capture-list panel, ~`:398`)
- Modify: `Resources/webapp/js/capture.js`

- [ ] **Step 1: Add DOM elements to the capture-list panel**

Immediately inside `<div class="collapsible-content" id="capture-list-content">` (before `<div class="capture-list-table-wrapper">`), add:

```html
                    <div class="capture-list-included-readout" id="capture-list-included-readout"></div>
                    <div class="capture-list-stale-banner hidden" id="capture-list-stale-banner">
                        Selection changed since last generate — regenerate to apply.
                    </div>
```

- [ ] **Step 2: Add the staleness + readout function to capture.js**

```javascript
function updateCaptureListStaleness() {
    const items = captureListState.items || [];
    const combos = matrixCombinationsState.combinations || [];

    // Trace-back readout: included matrix rows vs total, plus roundtrip note.
    const readout = document.getElementById('capture-list-included-readout');
    if (readout) {
        if (items.length === 0) {
            readout.textContent = '';
        } else {
            const total = combos.length;
            const included = combos.filter((c) => c.included).length;
            readout.textContent = `${included} of ${total} combinations included (+ roundtrip)`;
        }
    }

    // Staleness: does the generated list match the current inclusion set?
    const banner = document.getElementById('capture-list-stale-banner');
    if (!banner) return;
    if (items.length === 0 || combos.length === 0) {
        banner.classList.add('hidden');
        return;
    }
    const includedKeys = new Set(combos.filter((c) => c.included).map((c) => makeCombinationKey(c.controlValues)));
    const listKeys = new Set(items.filter((i) => !i.isRoundtrip).map((i) => makeCombinationKey(i.controlValues)));
    let stale = includedKeys.size !== listKeys.size;
    if (!stale) for (const k of includedKeys) if (!listKeys.has(k)) { stale = true; break; }
    banner.classList.toggle('hidden', !stale);
}
```

- [ ] **Step 3: Call it after list and combination changes**

Ensure `updateCaptureListStaleness()` is called at the end of `renderMatrixCombinations` (already added in Task 7 Step 1), at the end of `updateCaptureListDisplay`, and in `generateCaptureList` after `loadMatrixCombinations()`.

- [ ] **Step 4: Verify (DOM)**

```bash
cd e2e && nix develop -c node -e "import('./lib.mjs').then(async ({createHarness}) => {
  const h = createHarness(); await h.launch(); await h.waitForAppReady(); await h.newProject();
  await h.evalJsAsync(\"backend.call('addCaptureControl','Glare','discrete','0, 5, 10')\");
  await h.evalJsAsync('loadMatrixCombinations()');
  await h.evalJsAsync('generateCaptureList()');
  await h.evalJsAsync('bulkSetShown(false)');  // change selection after generate -> stale
  const hidden = await h.evalJs(\"document.getElementById('capture-list-stale-banner').classList.contains('hidden')\");
  console.log('stale banner hidden:', hidden); await h.app.quit?.(); process.exit(hidden===false?0:1);
});"
```
Expected: `stale banner hidden: false`, exit 0.

- [ ] **Step 5: Commit**

```bash
git add Resources/webapp/index.html Resources/webapp/js/capture.js
git commit -m "Add capture-list trace-back readout and staleness banner"
```

---

## Task 10: In-DOM confirm dialog + Generate confirmations

**Files:**
- Modify: `Resources/webapp/js/capture.js`

Native `confirm()` cannot be driven by e2e, so use an in-DOM confirm.

- [ ] **Step 1: Add `showConfirmDialog`**

```javascript
// Promise-based in-DOM confirm (native dialogs can't be driven by e2e).
// opts.okOnly => single "OK" button (used for warnings), resolves true.
function showConfirmDialog(message, opts = {}) {
    return new Promise((resolve) => {
        let overlay = document.getElementById('app-confirm-overlay');
        if (!overlay) {
            overlay = document.createElement('div');
            overlay.id = 'app-confirm-overlay';
            overlay.className = 'app-confirm-overlay hidden';
            overlay.innerHTML = `
                <div class="app-confirm-box">
                    <p class="app-confirm-message" id="app-confirm-message"></p>
                    <div class="app-confirm-actions">
                        <button class="btn-secondary" id="app-confirm-cancel">Cancel</button>
                        <button class="btn-primary" id="app-confirm-ok">Continue</button>
                    </div>
                </div>`;
            document.body.appendChild(overlay);
        }
        document.getElementById('app-confirm-message').textContent = message;
        const cancelBtn = document.getElementById('app-confirm-cancel');
        const okBtn = document.getElementById('app-confirm-ok');
        cancelBtn.classList.toggle('hidden', !!opts.okOnly);
        okBtn.textContent = opts.okOnly ? 'OK' : 'Continue';
        overlay.classList.remove('hidden');
        const done = (val) => { overlay.classList.add('hidden'); okBtn.onclick = null; cancelBtn.onclick = null; resolve(val); };
        okBtn.onclick = () => done(true);
        cancelBtn.onclick = () => done(false);
    });
}
window.showConfirmDialog = showConfirmDialog;
```

- [ ] **Step 2: Gate `generateCaptureList` behind the confirmations**

At the very top of `generateCaptureList` (before disabling the button), add:

```javascript
    const completed = (captureListState.items || []).filter((i) => i.status === 'complete').length;
    if (completed > 0) {
        const ok = await showConfirmDialog(`This will reset ${completed} completed capture${completed !== 1 ? 's' : ''}. Continue?`);
        if (!ok) return;
    }
    const includedN = (matrixCombinationsState.combinations || []).filter((c) => c.included).length;
    if (includedN === 0) {
        const ok = await showConfirmDialog('0 combinations included — the list will contain only the roundtrip. Continue?');
        if (!ok) return;
    }
```

- [ ] **Step 3: Verify (DOM)**

```bash
cd e2e && nix develop -c node -e "import('./lib.mjs').then(async ({createHarness, sleep}) => {
  const h = createHarness(); await h.launch(); await h.waitForAppReady(); await h.newProject();
  await h.evalJsAsync(\"backend.call('addCaptureControl','Glare','discrete','0, 5')\");
  await h.evalJsAsync('loadMatrixCombinations()');
  await h.evalJsAsync('bulkSetShown(false)');            // 0 included
  h.evalJs('generateCaptureList()');                     // fire (do not await; dialog blocks)
  await sleep(500);
  const visible = await h.evalJs(\"!document.getElementById('app-confirm-overlay').classList.contains('hidden')\");
  console.log('confirm shown for empty generate:', visible); await h.app.quit?.(); process.exit(visible?0:1);
});"
```
Expected: `confirm shown for empty generate: true`, exit 0.

- [ ] **Step 4: Commit**

```bash
git add Resources/webapp/js/capture.js
git commit -m "Add in-DOM confirm and destructive/empty Generate confirmations"
```

---

## Task 11: End-to-end workflow scenario (glare 14-of-90 shape)

**Files:**
- Create: `e2e/scenarios/matrix-exclusions-workflow.mjs`

- [ ] **Step 1: Write the scenario**

```javascript
// Full UI workflow: build a matrix, exclude-all then include a region via
// filters + bulk, generate, and confirm the list is trimmed. Screenshots the
// matrix panel. Run: nix develop -c node scenarios/matrix-exclusions-workflow.mjs
import {createHarness} from '../lib.mjs';
import assert from 'assert';

const h = createHarness();
const call = (fn, ...a) =>
  h.evalJsAsync(`backend.call(${[JSON.stringify(fn), ...a.map((x) => JSON.stringify(x))].join(', ')})`);

try {
  await h.launch();
  await h.waitForAppReady();
  await h.newProject();

  // Mode{Middle,Up,Down} x Glare{0,5,10} x Shape{2,5} = 18 combinations.
  await call('addCaptureControl', 'Mode', 'discrete', 'Middle, Up, Down');
  await call('addCaptureControl', 'Glare', 'discrete', '0, 5, 10');
  await call('addCaptureControl', 'Shape', 'discrete', '2, 5');
  await h.evalJsAsync('loadMatrixCombinations()');

  // Exclude everything, then include the Middle/Shape=5 region (3 combos).
  await h.evalJsAsync('bulkSetShown(false)');
  await h.evalJs(`(() => {
    matrixCombinationsState.filters = { Mode: 'Middle', Shape: '5' };
    renderMatrixCombinations(); return true; })()`);
  await h.evalJsAsync('bulkSetShown(true)');

  const included = await h.evalJs('matrixCombinationsState.combinations.filter(c=>c.included).length');
  assert.strictEqual(included, 3, 'expected 3 included (Middle x Glare{0,5,10} x Shape=5)');

  await h.nativeScreenshot('matrix-exclusions.png');

  // Generate (no completed captures, non-empty -> no confirm) and check list size.
  await h.evalJsAsync('generateCaptureList()');
  const listLen = await h.evalJs('captureListState.items.length');
  assert.strictEqual(listLen, 4, 'expected 4 items (3 included + roundtrip)');

  console.log('PASS: matrix-exclusions-workflow');
} catch (err) {
  console.error(`FAIL: ${err && err.stack ? err.stack : err}`);
  process.exitCode = 1;
} finally {
  await h.app.quit?.().catch(() => {});
  process.exit(process.exitCode ?? 0);
}
```

- [ ] **Step 2: Run it — expect PASS**

```bash
cd e2e && nix develop -c node scenarios/matrix-exclusions-workflow.mjs
```
Expected: `PASS: matrix-exclusions-workflow`, exit 0. Then `Read e2e/out/matrix-exclusions.png` to eyeball the panel.

- [ ] **Step 3: Commit**

```bash
git add e2e/scenarios/matrix-exclusions-workflow.mjs
git commit -m "Add e2e workflow scenario for matrix exclusions"
```

---

## Final verification

- [ ] Run both scenarios green:
  ```bash
  cd e2e && nix develop -c node scenarios/matrix-exclusions-backend.mjs \
    && nix develop -c node scenarios/matrix-exclusions-workflow.mjs
  ```
- [ ] `Read e2e/out/matrix-exclusions.png` and confirm the combinations table, filter chips, bulk buttons, and "N of M included" render correctly.
- [ ] Open a real legacy project (`~/Work/spline/captures/glare/*.rcp`) via `openProject` and confirm it loads with everything included (no `excludedKeys` key present ⇒ empty ⇒ all included).
