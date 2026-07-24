# Capture matrix exclusions — design

**Date:** 2026-07-24
**Status:** Approved, ready for implementation planning
**Area:** Capture matrix (controls) — `Source/capture/`, `Source/project/`,
`Resources/webapp/js/capture.js`, `Source/MainComponent.cpp`

## Problem

The tool generates the full Cartesian product of all capture controls as a flat
capture list (e.g. Mode{Middle,Up,Down} × Glare{0,3,5,8,10} × Shape{2,5,8} ×
Gain{min,mid} = 90 rows), plus one roundtrip calibration row. Today every
combination is recorded — there is no way to skip a subset.

As the user hones in on specific pedal features, they increasingly want to record
*most* of the matrix but **selectively exclude** certain combinations. The real
target session (`05_glare/probe/README.md`) wants only 14 of the 90 combinations,
structured as a few small sweeps where 1–2 controls vary while the rest stay
pinned.

A single tickable 2D grid was rejected: the matrix is 4-dimensional, so a 2D grid
can only represent it by flattening 4 controls onto 2 axes (sparse, arbitrary axis
pairing, tri-state cells) and is the most expensive thing to build.

## Workflow and where exclusions live

Exclusions are a **matrix-design** concern, not a capture-list concern. The user's
flow is:

1. Define controls (as today).
2. **Program exclusions** on a combinations table in the matrix stage.
3. **Generate List** → a clean capture list containing only included combinations
   (+ the roundtrip row).

Consequences that shape the design:

- The **capture list keeps its current UX** — it is pure recording output. No
  checkboxes, filters, or greyed rows there. It simply contains fewer rows.
- Exclusion state lives **upstream of generation**, as a property of the matrix.
  Regenerating re-derives the list from it, so there is no "re-inclusion trap":
  "Generate wipes state" only resets the capture list's per-row record status, not
  the exclusion selection.

## Chosen approach

Add a **combinations table** to the matrix stage (alongside the controls list):

1. A row per generated combination, each with an **include checkbox** (default
   checked).
2. **Per-control filter chips** above the table that narrow the visible rows
   (client-side only).
3. **"Include shown" / "Exclude shown"** bulk buttons acting on the filtered rows.
4. A "**N of M included**" readout (replaces / augments the existing total-count
   display).

Generate reads the include state and emits only included combinations. This reuses
the flat-table + filter + bulk pattern validated during brainstorming (chosen over
a 2D pivot grid and a per-value rules engine for use-speed, verifiability, and
build simplicity), placed *before* generation so the capture list stays clean.

## Data model

Exclusion is a property of the **matrix**, stored on `CaptureControlManager`
(`Source/capture/CaptureControl.h/.cpp`) — **not** on `CaptureItem`. `CaptureItem`
is unchanged.

- Store the set of **excluded combination keys**:
  `juce::StringArray excludedKeys;` (empty = everything included).
- A **combination key** canonically identifies one product cell as the ordered
  join of `name=value` pairs across all controls, in the controls' defined order,
  using a non-printable separator (e.g. `\x1f`) to avoid delimiter collisions.
  Example: `Mode=Middle\x1fGlare=0\x1fShape=5\x1fGain=min`.
- New `CaptureControlManager` methods:
  - `getCombinations()` → enumerates the full product as an ordered list of
    `{ key, StringPairArray controlValues }`. Factored from the existing
    Cartesian-product logic so enumeration lives in **one** place (C++), shared
    with `CaptureListManager::generate`.
  - `isExcluded(key)`, `setExcluded(key, bool)`, `getIncludedCount()` /
    `getTotalCombinationCount()`.
- **Editing controls invalidates only affected keys.** Adding/removing a control,
  or renaming a control/value, changes the keys of affected combinations; stale
  excluded keys that no longer match any combination are simply ignored, and any
  genuinely new combination defaults to included. This matches "default all
  included" and needs no migration.

## Persistence

- Serialize `excludedKeys` in the project's **matrix** section
  (`ProjectState.cpp`, alongside `serializeControls`).
- Deserialize with default **empty** when absent, so existing `.rcp` projects load
  with everything included. Additive and back-compatible — no schema version bump.
- `CaptureItem` serialization is **unchanged** (no `included` field added).

## Regeneration behavior (decided)

`generateCaptureList` rebuilds the capture list from scratch and resets statuses,
as today. It now **skips excluded combinations** while doing so. It does **not**
touch `excludedKeys` — the exclusion selection is matrix state that survives
generation. (Deferred: preserving/altering the wipe-on-generate behavior for
statuses is out of scope; current behavior is acceptable.)

## Backend (native functions)

In `MainComponent.cpp`, alongside the existing matrix/list functions:

```
getMatrixCombinations()
  -> [ { key: string, controlValues: {name: value, ...}, included: bool }, ... ]
```
Enumerates the product via `getCombinations()`, tagging each with its current
include state. Returns `[]` when no controls are defined. No roundtrip entry (the
roundtrip is not a matrix combination).

```
setCombinationsIncluded(keys: string[], included: bool)
  -> { success: bool, includedCount: int, totalCount: int }
```
Updates `excludedKeys` for each key, calls `markProjectDirty()`, returns the new
counts. Backs both single-checkbox toggles (`keys` of length 1) and bulk
operations.

```
generateCaptureList()   // existing — modified
```
Skips combinations whose key is in `excludedKeys`; always appends the roundtrip
row.

## Roundtrip handling

The roundtrip row is a mandatory calibration reference, **not** a matrix
combination. It therefore:

- Never appears in the combinations table and has no include state.
- Is always appended to the generated capture list and always recorded.
- Is unaffected by any exclusion UI.

This realizes the intent: the roundtrip is *always in the capture list* but *never
part of the capture matrix*.

## Frontend (`Resources/webapp/js/capture.js`)

**New: combinations table in the matrix stage.**
- Rendered from `getMatrixCombinations()`. Refreshed whenever controls change
  (hook into the existing add/remove/update-control handlers).
- Columns: **checkbox** (checked = included) + one column per control.
- **Filter chips** above it: one dropdown per control, default `any`, values drawn
  from the controls' own value lists. Client-side only — hides non-matching rows,
  never changes include state. A "**Showing K of M (filtered) · clear**" banner
  appears while any filter is active.
- **Bulk buttons:** "Include shown" / "Exclude shown" collect the keys of visible
  (filtered) rows and call `setCombinationsIncluded(keys, …)`. A header
  select-all/none checkbox toggles the visible rows.
- **"N of M included"** readout (M = total combinations, N = included). Reuses /
  replaces the existing `total-capture-count` element. Keep the existing
  warning/error thresholds on the total.
- Individual checkbox change → `setCombinationsIncluded([key], checked)`. Mutations
  update the count and the row's style from the returned counts (no full re-fetch
  required for a single toggle).

**Capture list: unchanged.** `updateCaptureListDisplay`, row-click/current-capture
navigation, progress ("X / Y complete"), and status handling stay as they are.
After this change the list simply contains included combinations + roundtrip.

**Large matrices:** the combinations table renders all rows (same approach as the
current capture list). Virtualization for very large products is out of scope; the
existing >1000 / >10000 warning thresholds on the count remain the guard.

## Example: the glare 14-of-90 workflow

In the matrix stage:
1. Header select-none → **Exclude all**.
2. Filter `Mode: Middle, Shape: 5, Gain: min` → **Include shown** (the 5 Glare steps).
3. Filter `Mode: Middle, Glare: 10, Gain: min` → **Include shown** (the Shape sweep).
4. A couple more region include-clicks for the Gain and Mode variations.
5. **Generate List** → capture list holds the 14 included combinations + roundtrip.

A one-off skip is just unticking a combination before generating.

## Testing

Via the e2e harness (`e2e/`, drives the webview through `evaluate-js`):

- **Enumeration:** with controls defined, `getMatrixCombinations()` returns the
  full product with stable keys and all `included: true` by default.
- **Toggle + persistence:** exclude a subset via `setCombinationsIncluded`; confirm
  counts; save + reload the project and confirm `excludedKeys` round-trips; confirm
  a legacy project (no `excludedKeys`) loads with everything included.
- **Generation:** with a known exclusion set, `generateCaptureList` produces only
  included combinations plus exactly one roundtrip row; excluded combinations are
  absent from the capture list.
- **Filter + bulk:** apply a filter, click Include/Exclude shown, confirm only the
  filtered combinations changed.
- **Key stability under edits:** exclude a combination, add a value to an unrelated
  control, regenerate; confirm the still-existing excluded combination remains
  excluded and new combinations default to included.
- **Regeneration:** exclusions persist across `generateCaptureList`; only capture
  list statuses reset.
- **Roundtrip exemption:** roundtrip never appears in `getMatrixCombinations`,
  always appears once in the generated list, and is unaffected by bulk/select-all.

## Out of scope (possible later upgrades)

- Multi-select filter chips (one filter expressing `Glare ∈ {0,5,10}`).
- A read-only "pick 2 axes" heat-grid visualization of include density.
- Virtualized rendering for very large products.
- Preserving exclusions across control renames (keys are order/name-derived).
- Revisiting the wipe-on-generate status reset.
