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
   (client-side only). Single-select per chip (multi-select deferred).
3. **"Include shown" / "Exclude shown"** bulk buttons acting on the currently
   *filter-visible* rows, with the live count baked into the label ("Exclude 90
   shown"). These are the only bulk mechanism — there is no separate header
   select-all checkbox.
4. A "**N of M included**" readout (replaces / augments the existing total-count
   display), with microcopy that exclusions apply on the next **Generate List**.

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
- **This invalidation must not be silent.** When a control edit strands one or
  more excluded keys (they no longer match any current combination), the frontend
  warns with the count — e.g. "This edit reset the include/exclude selection for
  45 combinations." Losing a hand-picked selection with no notice reads as a bug;
  the warning is the floor (a rename-specific key migration is a deferred upgrade).
  The `addCaptureControl` / `removeCaptureControl` / `updateCaptureControl`
  responses expose the stranded-key count so the frontend can surface it.

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
generation.

The status-preserving regenerate (keep completed captures across a regenerate)
remains **deferred** per prior decision. Because this feature makes mid-session
trimming likely, two cheap guardrails ship now so the wipe is never a silent
dead-end:

- **Staleness indicator.** When the current exclusion selection no longer matches
  the already-generated capture list (the user edited the selection after
  generating), show a banner on the capture list: "Selection changed since last
  generate — regenerate to apply." This is a boolean derived state (selection
  fingerprint vs. the fingerprint captured at last generate); it does not require
  diffing UI.
- **Destructive-generate confirmation.** If Generate would discard completed
  captures, confirm first, stating the count: "This will reset N completed
  captures. Continue?" Also confirm when the resulting list would be
  **empty of matrix rows** (everything excluded → only the roundtrip remains):
  "0 combinations included — the list will contain only the roundtrip. Continue?"

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
row. (The destructive-generate / empty-list confirmations live in the frontend,
which knows the current completed-capture count; the backend just builds the list.)

The existing `addCaptureControl` / `removeCaptureControl` / `updateCaptureControl`
responses gain a **`strandedExcludedCount`** field — the number of previously
excluded keys that no longer match any combination after the edit — so the
frontend can warn about selection loss (see Data model).

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
  appears while any filter is active, positioned **next to the bulk buttons** so
  an active filter is visible at the moment of clicking Include/Exclude shown
  (guards against acting on the wrong rows because a stale filter was left on).
  "shown" throughout means **filter-visible**, not viewport-visible.
- **Bulk buttons (the only bulk mechanism):** "Include shown" / "Exclude shown",
  each with the live count in the label ("Exclude 90 shown"). They collect the
  keys of the currently filter-visible rows and call
  `setCombinationsIncluded(keys, …)`. No separate header select-all checkbox.
- **Undo:** after a bulk operation, show a one-level undo toast ("Excluded 76 —
  Undo") that restores the prior include state of exactly the affected keys.
- **"N of M included"** readout (M = total combinations, N = included). Reuses /
  replaces the existing `total-capture-count` element. The existing
  warning/error thresholds guard **render cost**, so they key off **M** (the
  number of rows actually rendered in the combinations table); the readout also
  shows **N** as the count that will actually be recorded, so a small N under a
  large M is not mistaken for an alarm.
- Individual checkbox change → `setCombinationsIncluded([key], checked)`. Mutations
  update the count and the row's style from the returned counts (no full re-fetch
  required for a single toggle).

**Capture list: essentially unchanged.** `updateCaptureListDisplay`,
row-click/current-capture navigation, progress ("X / Y complete"), and status
handling stay as they are. After this change the list simply contains included
combinations + roundtrip. Two small, non-interactive additions so a trimmed list
is never mysterious:

- A one-line header readout: "**14 of 90 combinations included (+ roundtrip)**"
  that links/scrolls back to the matrix stage — answers "why is this combination
  missing?" without cluttering the rows themselves.
- The **staleness banner** described under "Regeneration behavior" when the
  selection has changed since the last generate.

**Large / continuous matrices (known limitation):** a continuous control expands
to many values (e.g. `0-10:0.5` → 21), so the product — and thus this table —
grows fastest exactly there, and with single-select chips a value-range region
takes one include pass per value. The combinations table renders all rows (same
approach as the current capture list); virtualization and multi-select chips are
out of scope for v1. The existing >1000 / >10000 thresholds guard render cost
(keyed off M). This is an accepted v1 limitation, called out here so it is a
conscious tradeoff rather than a surprise; multi-select chips are the first
mitigation to pull in if continuous controls become common.

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
  always appears once in the generated list, and is unaffected by bulk actions.
- **Stranded-key warning:** exclude combinations, then rename a control/value so
  those keys no longer match; confirm the control-edit response reports the
  stranded count and the frontend surfaces the warning.
- **Bulk undo:** "Exclude shown" then Undo restores exactly the prior include
  state of the affected keys and nothing else.
- **Staleness banner:** generate, then change the selection; confirm the capture
  list shows the "selection changed since last generate" banner, which clears
  after regenerating.
- **Destructive / empty generate:** with completed captures present, Generate
  confirms with the reset count; with everything excluded, Generate confirms the
  roundtrip-only outcome.
- **Trace-back readout:** the capture list header shows "N of M combinations
  included (+ roundtrip)" matching the generated contents.

## Out of scope (possible later upgrades)

- Multi-select filter chips (one filter expressing `Glare ∈ {0,5,10}`).
- A read-only "pick 2 axes" heat-grid visualization of include density.
- Virtualized rendering for very large products.
- Preserving exclusions across control renames (keys are order/name-derived).
- Revisiting the wipe-on-generate status reset.
