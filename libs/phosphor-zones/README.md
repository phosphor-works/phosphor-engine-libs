<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-zones

The zone data model, layout persistence, zone detection, and the `ILayoutSource` adapter that publishes manual zone
layouts to the daemon.

## Responsibility

A **zone** is a named rectangular area of screen space with metadata: appearance, label and quick-select slot. A
**layout** is a collection of zones, with assignment rules for each context (screen, desktop, activity). This
library owns these parts:

| Part | What it is |
| --- | --- |
| Data model | `Zone`, `Layout`, and utilities that convert between relative and absolute coordinates. |
| Detection | From a cursor point and the modifier state, it finds the zone that the window snaps into. `ZoneDetector` implements the `IZoneDetector` interface. |
| Layout registry | `LayoutRegistry` is the concrete catalog of manual zone layouts and the per-context assignment store (which layout is active on which screen, desktop and activity). It implements `IZoneLayoutRegistry` (the contract to read and change the catalog) and `ILayoutSourceRegistry` (the provider contract that `ZonesLayoutSource` subscribes to). |
| Layout source factory | `ZonesLayoutSource` adapts a layout to the generic `ILayoutSource` contract. Thus the rest of the stack uses manual layouts the same way as autotile output. `ZonesLayoutSourceFactory` registers this source with `LayoutSourceProviderRegistry`. |
| Highlighter | `ZoneHighlighter` drives the state machine of the per-zone emphasis in the overlay (hover, active drag target, just-snapped flash). |
| Assignment value type | `AssignmentEntry` and `LayoutAssignmentKey` carry the `(screenId, desktop, activity)` keys that `LayoutRegistry` stores assignments under. The injected `PhosphorRules::RuleStore` is the single source of truth for every per-context assignment. Each assignment is a context-only rule, and the cascade is the walk of the evaluator in descending priority. |

`LayoutRegistry` is the *concrete* counterpart of `IZoneLayoutRegistry`. The interface exists for consumers that only
list or look up layouts (read-only previews, settings UI thumbnails). They depend on the smaller contract, which is
also easy to mock.

## Key types

| Type | Purpose |
|------|---------|
| `PhosphorZones::Zone`                       | Value type: rect, id, label, colors, appearance |
| `PhosphorZones::Layout`                     | Collection of zones with app-rule auto-snap mappings |
| `PhosphorZones::IZoneDetector`              | Abstract resolver from cursor to zone |
| `PhosphorZones::ZoneDetector`               | Concrete implementation with adjacency-graph navigation |
| `PhosphorZones::IZoneLayoutRegistry`        | Catalog contract: list, change, set the active layout |
| `PhosphorZones::LayoutRegistry`             | Concrete `IZoneLayoutRegistry` and per-context assignment store |
| `PhosphorZones::ZonesLayoutSource`          | `ILayoutSource` adapter for manual layouts |
| `PhosphorZones::ZonesLayoutSourceFactory`   | Provider-side factory. Registers through `LayoutSourceProviderRegistry` |
| `PhosphorZones::AssignmentEntry`            | Per-context layout assignment value type |
| `PhosphorZones::LayoutAssignmentKey`        | `(screenId, desktop, activity)` key for assignment lookups |
| `PhosphorZones::ZoneHighlighter`            | Overlay highlight state machine |

## Use

```cpp
#include <PhosphorZones/LayoutRegistry.h>
#include <PhosphorZones/ZoneDetector.h>

using namespace PhosphorZones;

LayoutRegistry *registry = /* injected */;
Layout *current = registry->layoutForScreen(screenId, desktop, activity);

// Detect which zone the cursor is over
ZoneDetector det;
det.setLayout(current);
Zone *z = det.zoneAtPoint(cursorPos);
if (z) {
    overlay.highlight(*z);
}
```

## Design notes

- **Zone IDs are UUIDs, never indices.** When the editor changes the order of zones, no persisted assignment from a
  window to a zone becomes orphaned.
- **Relative coordinates on disk.** Zone rects in JSON are normalized to the `0.0 - 1.0` range, so the same layout
  works on any screen size. The conversion to pixels occurs at read time.
- **CRUD is on the concrete registry, not on a separate manager.** `LayoutRegistry` is the single concrete class.
  `IZoneLayoutRegistry` exists only where mocking is useful. This is the same shape as the `AlgorithmRegistry` of
  `phosphor-tiles`.
- **No process-global singleton.** Each composition root constructs one `LayoutRegistry` for each process and
  injects it into every consumer. Tests build their own instance for each fixture.
- **Quick-layout slots and screen-assignment CRUD are in the consumer.** The Phosphor daemon owns the user-facing
  surface (D-Bus methods, quick-layout 1-9, and more). This library gives the data that the daemon drives that
  surface from.

## Dependencies

- `QtCore`, `QtGui`
- [`phosphor-layout-api`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-layout-api/README.md):
  the `ILayoutSource` and registry contracts
- [`phosphor-geometry`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-geometry/README.md):
  zone clamping, overlap resolution, helpers from rect to JSON
- [`phosphor-rules`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-rules/README.md):
  `RuleStore` + `RuleEvaluator`, the assignment authority (the `LayoutRegistry` ctor takes a `RuleStore*`)
- [`phosphor-identity`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-identity/README.md):
  window IDs for assignments (private link)
- [`phosphor-screens`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-screens/README.md):
  normalization of screen IDs (private link)

## See also

- [`phosphor-snap-engine`](../phosphor-snap-engine/README.md): uses `LayoutRegistry` + `ZoneDetector` to implement
  manual snapping.
- [`phosphor-tiles`](../phosphor-tiles/README.md): autotile algorithms make the same `Layout` shape through
  `AutotileLayoutSource`.
