# Capture matrix exclusions — design

**Date:** 2026-07-24
**Status:** Approved, ready for implementation planning
**Area:** Capture list (matrix) — `Source/capture/`, `Source/project/`, `Resources/webapp/js/capture.js`, `Source/MainComponent.cpp`

## Problem

The tool generates the full Cartesian product of all capture controls as a flat
capture list (e.g. Mode{Middle,Up,Down} × Glare{0,3,5,8,10} × Shape{2,5,8} ×
Gain{min,mid} = 90 rows), plus one roundtrip calibration row. Today every row is
recorded — there is no way to skip a subset.

As the user hones in on specific pedal features, they increasingly want to record
*most* of the matrix but **selectively exclude** certain combinations. The real
target session (`05_glare/probe/README.md`) wants only 14 of the 90 combinations,
structured as a few small sweeps where 1–2 controls vary while the rest stay
pinned.

The user's first instinct was a tickable 2D grid. That was rejected during
brainstorming: the matrix is 4-dimensional, so a single 2D grid can only
represent it by flattening 4 controls onto 2 axes, producing a sparse 90-cell
grid with arbitrary axis pairing and tri-state "mush" when a hidden control
varies. It is also the most expensive option to build (a pivot table).

## Chosen approach

Add exclusion to the **existing flat capture-list table**, not a new grid:

1. A per-row **checkbox column** (default checked) to include/exclude individual
   rows.
2. **Per-control filter chips** above the table that narrow the visible rows
   (client-side only).
3. **"Include shown" / "Exclude shown"** bulk buttons that act on the currently
   filtered rows.

The full matrix stays visible as the backdrop — excluded rows are greyed, never
deleted. This makes the common case (skip a handful) a single click, the
structured case (skip whole regions) a filter-plus-click, and keeps the actual
recording plan — not a projection of it — always on screen.

This was selected over a 2D pivot grid and over a per-value-toggles + rules-engine
approach, on grounds of use-speed for the common case, verifiability, and build
simplicity (see brainstorming analysis; the pivot grid was judged
"seductive-but-wrong").

## Data model

Single field added to `CaptureItem` (`Source/capture/CaptureList.h`):

```cpp
bool included = true;   // false = present in the matrix but skipped when recording
```

- Property of each generated matrix row. Defaults to `true`.
- **The roundtrip row is not a matrix combination and is exempt from exclusion
  entirely** — it is always recorded and its `included` flag is never toggled by
  any UI. It always sits in the capture list. See "Roundtrip handling" below.
- Excluded rows remain in the list; they are never removed from `items`.

## Persistence

- `CaptureListManager::toVar()` (`CaptureList.cpp`) serializes `included`.
- `serializeCaptureList()` (`Source/project/ProjectState.cpp`) writes `included`.
- `deserializeCaptureList()` reads it, **defaulting to `true` when the key is
  absent**, so existing `.rcp` projects load unchanged (same pattern already used
  for `isRoundtrip`).
- No `.rcp` schema version bump needed — the field is additive and back-compatible.

## Regeneration behavior (decided)

`generateCaptureList` already rebuilds the list from scratch and resets all
statuses. It will **also reset all exclusions** (every freshly generated row is
`included = true`). This is consistent with the existing "generate = fresh list"
semantics. Consequence: editing controls and regenerating loses prior exclusions.
Accepted for v1.

## Roundtrip handling

The roundtrip row is a mandatory calibration reference, **not** a matrix
combination. It is exempt from the entire exclusion mechanism:

- Always in the capture list, always recorded — its `included` flag stays `true`
  and is never toggled by any UI or by `setCaptureItemsIncluded` (the backend
  ignores the roundtrip id if passed, defensively).
- **No checkbox** in its row; the select-all header checkbox does not affect it.
- **Filters never hide it and bulk operations never touch it.** It renders
  pinned in the list regardless of active filters.
- Not counted in the matrix counters (M / N / filter K-of-M); it is counted in
  recording progress ("X / Y complete") since it is genuinely recorded.

This matches the intent: the roundtrip is *always in the capture list* but
*never part of the capture matrix* (the exclusion/filter surface).

## Backend (native function)

New native function in `MainComponent.cpp`, alongside the other capture-list
functions:

```
setCaptureItemsIncluded(ids: string[], included: bool)
  -> { success: bool, captureList: <serialized list> }
```

- Sets `included` on each listed item id via the manager.
- Calls `markProjectDirty()`.
- Returns the refreshed serialized list (same shape `setCaptureItemStatus`
  returns), so the frontend re-renders from authoritative state.

A supporting `CaptureListManager` method (e.g. `setIncluded(id, bool)` mirroring
the existing `setStatus`) backs it. Bulk operations call it per id.

## Frontend (`Resources/webapp/js/capture.js`)

All UI lives in the existing capture-list table region.

**Table rendering (`updateCaptureListDisplay`):**
- New leftmost **checkbox column**; checked reflects `item.included`.
- Matrix rows only get a checkbox. The **roundtrip row shows no checkbox** (an
  em-dash / lock glyph or blank cell) — it cannot be excluded.
- Header cell holds a **select-all/none checkbox** that toggles `included` on the
  currently *visible* (filtered) **matrix** rows only.
- Excluded rows get a greyed/`.excluded` style but remain rendered.
- Count/progress readouts:
  - Header gains "**N of M included**", where **M = total matrix rows** (excludes
    the roundtrip) and **N = included matrix rows**.
  - "X / Y complete" reflects actual recording work: **Y = included matrix rows +
    1** (the always-recorded roundtrip), and X counts completes among those.

**Filter bar (new, above the table):**
- One dropdown per control name, each defaulting to `any`, plus the control's
  distinct values. Single-select for v1 (multi-select is a documented later
  upgrade).
- Filtering is **client-side only** — it hides non-matching rows from view; it
  does not change `included`.
- A "**Showing K of M (filtered) · clear**" banner appears whenever any filter is
  active, so a stale filter is never mistaken for data loss. "Clear" resets all
  filters to `any`. (K/M count matrix rows only; the pinned roundtrip row is not
  counted — see "Roundtrip handling".)

**Bulk buttons (new):**
- **Include shown** / **Exclude shown**: collect the ids of currently visible
  (filtered) **matrix** rows and call `setCaptureItemsIncluded(ids, true|false)`.
  The roundtrip row's id is never included in a bulk operation.

**Interaction wiring:**
- Individual checkbox change → `setCaptureItemsIncluded([id], checked)`.
- All mutations re-render from the returned authoritative list.

**Recording / navigation:**
- `advanceToNextCapture()` skips rows where `included === false` (in addition to
  the existing pending check) in both the forward and wrap-around loops.
- Excluded rows are **not clickable** — they cannot be selected as the current
  capture at all (the row-click handler ignores excluded rows). Combined with
  `advanceToNextCapture()` skipping them, an excluded row can never become the
  current capture and so can never be recorded. Simpler than a record-time guard,
  and still upholds "no silent recording".

## Example: the glare 14-of-90 workflow

1. Header select-none → **Exclude all**.
2. Filter `Mode: Middle, Shape: 5, Gain: min` → **Include shown** (the 5 Glare steps).
3. Filter `Mode: Middle, Glare: 10, Gain: min` → **Include shown** (the Shape sweep).
4. A couple more region include-clicks for the Gain and Mode variations.

Each region is one filter set plus one button. A one-off skip is just unticking a
row.

## Testing

Given the e2e harness (`e2e/`, drives the webview via `evaluate-js`), verify:

- **C++ unit-ish via harness:** generate a list, exclude a subset via
  `setCaptureItemsIncluded`, confirm serialized `included` flags; save+reload a
  project and confirm exclusions round-trip; confirm a legacy project (no
  `included` key) loads with all rows included.
- **Advance logic:** with a known exclusion set, confirm `advanceToNextCapture()`
  never lands on an excluded row and that "N of M included" / "X / Y complete"
  reflect included-only counts.
- **Filter + bulk:** apply a filter, click Include/Exclude shown, confirm only the
  filtered rows changed and the rest are untouched.
- **Regeneration reset:** exclude rows, regenerate, confirm all rows return to
  included.
- **Excluded row unclickable:** confirm clicking an excluded row does not make it
  the current capture.
- **Roundtrip exemption:** confirm the roundtrip row has no checkbox, is never
  hidden by any filter, is untouched by "Exclude shown" / select-all, and that
  passing its id to `setCaptureItemsIncluded(false)` leaves it included.

## Out of scope (possible later upgrades)

- Multi-select filter chips (one filter expressing `Glare ∈ {0,5,10}`).
- A read-only "pick 2 axes" heat-grid *visualization* (cells shaded by
  included-count) as a sanity-check view — no editing, ships only if the filtered
  table count proves insufficient.
- Preserving exclusions across regeneration (keyed by control-value combination
  rather than row id).
