<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-snap-engine

The manual placement engine, based on zones. It implements `IPlacementEngine` for screens that run a layout the user
drew. It handles auto-snap when a window opens, directional zone navigation, floating, rotation, resnap when the
layout changes, and serialization through `SnapState`.

## Responsibility

The Phosphor daemon sends each window-lifecycle and shortcut event through `PhosphorEngine::IPlacementEngine`, so it
does not branch on the current placement mode. `phosphor-snap-engine` is the implementation for manual mode. When a
screen has an active zone layout, this engine decides these items:

- which zone a new window joins
- how Super+arrow moves between zones
- how rotation cycles windows
- what float and unfloat do

The engine reads these shared services. It owns none of them:

| Service | What it gives |
| --- | --- |
| `PhosphorEngine::IWindowTrackingService` | The store that all engines share for zone assignments, pre-tile geometries and floating state. |
| `PhosphorEngine::IVirtualDesktopManager` | The current virtual desktop. |
| `PhosphorZones::LayoutRegistry` and `PhosphorZones::IZoneDetector` | The layout catalog and the resolver from cursor to zone, from [`phosphor-zones`](../phosphor-zones/README.md). |
| `PhosphorEngine::ISnapSettings` | Exclusions, sticky-window handling and the "auto-assign all layouts" master toggle. It is declared in `PhosphorEngine`, but this library ships the implementation. |
| `INavigationStateProvider` | A narrow read-only contract that the daemon's `WindowTrackingAdaptor` implements. With it, the engine does not reach into the compositor-shadow state through `QObject*` + invokeMethod. |
| `IZoneAdjacencyResolver` | A narrow contract for directional zone lookups, with the same shape as the navigation-state provider. |

The engine **owns** `SnapState` (the mutable per-screen state that implements `IPlacementState`) and
`SnapNavigationTargetResolver` (pure computation of the target geometries for keyboard navigation).

## Key types

| Type | Purpose |
|------|---------|
| `PhosphorSnapEngine::SnapEngine`                   | Concrete `IPlacementEngine` for manual zone layouts |
| `PhosphorSnapEngine::SnapState`                    | Per-screen `IPlacementState`: zone assignments, pre-tile geometry, floating set |
| `PhosphorEngine::ISnapSettings`                    | Settings the engine reads (exclusions, sticky-window handling, master toggle) |
| `PhosphorSnapEngine::INavigationStateProvider`     | Narrow read-only state contract the daemon implements |
| `PhosphorSnapEngine::IZoneAdjacencyResolver`       | Directional zone lookup contract the daemon implements |
| `PhosphorSnapEngine::SnapNavigationTargetResolver` | Pure computation of move, focus, swap, push, cycle and restore target geometries |

## Use

Connect the engine in the composition root of the daemon:

```cpp
#include <PhosphorSnapEngine/SnapEngine.h>

using namespace PhosphorSnapEngine;

auto* snap = new SnapEngine(layoutRegistry,
                            windowTrackingService,
                            zoneDetector,
                            virtualDesktopManager,
                            /*parent*/ daemon);
snap->setEngineSettings(snapSettingsAdaptor);
snap->setNavigationStateProvider(windowTrackingAdaptor);
snap->setZoneAdjacencyResolver(zoneDetectionAdaptor);
```

The daemon then sends each screen's events to the engine of that screen. In phosphor-engine-runtime,
`ScreenModeRouter` takes the snap, autotile and scroll engines, and `engineFor(screenId)` returns the one to call.

## Design notes

- **Inherits `PlacementEngineBase`.** The base gives the shared plumbing (settings injection, pruning of stale
  windows, common signals). This engine adds only the decisions for manual zones: which zone, when to auto-snap and
  how to navigate.
- **Typed interfaces, not `QObject*` + invokeMethod.** The daemon dispatch uses the narrow
  `INavigationStateProvider` and `IZoneAdjacencyResolver` contracts. They are strictly typed, with no string method
  names and no opaque pointers.
- **State is shared, not owned.** Window tracking, the virtual-desktop manager, the layout registry and the zone
  detector are all outside the engine. `SnapState` is the one mutable item that this library owns. The engine reads
  or commands everything else.
- **Cross-engine handoff.** `engineId()`, `handoffReceive` and `handoffRelease` implement the placement-engine
  handoff protocol. When a window moves from a snap screen to an autotile screen, its ownership moves cleanly, and
  the daemon does no bookkeeping.
- **Auto-assign master toggle.** `autoAssignAllLayouts()` is the override for the whole process. The effective
  behavior is `globalAutoAssign OR layout->autoAssign()`. Autotile screens stop earlier, in `windowOpened`, and never
  reach this path.

## Dependencies

- `QtCore`, `QtGui`
- [`phosphor-engine`](../phosphor-engine/README.md): `IPlacementEngine`, `IPlacementState`, `PlacementEngineBase`,
  `IWindowTrackingService`, `IVirtualDesktopManager`
- [`phosphor-zones`](../phosphor-zones/README.md): `LayoutRegistry`, `IZoneDetector`
- [`phosphor-protocol`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-protocol/README.md):
  navigation result types (`MoveTargetResult`, `FocusTargetResult`, …) through the QtCore-only
  `PhosphorProtocol::Types` target. This library never links QtDBus.
- [`phosphor-rules`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-rules/README.md):
  `RuleEvaluator`, `RuleSet` (evaluation of Exclude rules)
- [`phosphor-screens`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-screens/README.md):
  screen topology (private link)
- [`phosphor-identity`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-identity/README.md):
  window IDs (private link)

## See also

- [`phosphor-tile-engine`](../phosphor-tile-engine/README.md): the sibling autotile engine. Both implement the same
  `IPlacementEngine` contract.
