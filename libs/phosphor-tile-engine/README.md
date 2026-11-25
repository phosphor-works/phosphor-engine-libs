<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-tile-engine

The automatic tiling placement engine. It implements `IPlacementEngine` for screens that run an autotile algorithm.
It does the layout again when a window opens, closes or gets focus. It also handles navigation, ratio and
master-count changes, per-screen overrides and auto-floating on overflow.

## Responsibility

[`phosphor-tiles`](../phosphor-tiles/README.md) owns the algorithms (`TilingAlgorithm`, `TilingState`, the Luau
algorithm binding). `phosphor-tile-engine` is the **runtime engine** that drives those algorithms when compositor
events occur. Because the two are separate, a settings UI can show an algorithm preview without a link to the
engine. Also, the engine can change and the algorithm vocabulary stays the same.

The engine manages these areas:

| Area | What the engine does |
| --- | --- |
| Lifecycle | When a window opens, the engine runs the active algorithm. When a window closes, it removes the entry from `TilingState` and runs the layout again. Focus events drive the cursor and focus model of the consumer. |
| Navigation | `NavigationController` is a stateless helper that holds a back-pointer to the engine. It handles focus next, previous and master, swap, rotation, directional focus and swap, position moves, and master ratio and count changes. Stateless means that no member data separates from the engine. |
| Per-screen overrides | `PerScreenConfigResolver` resolves the effective gap, algorithm, split ratio and master count. It falls back from the per-screen overrides to the global `AutotileConfig`. |
| Overflow | `OverflowManager` records the windows that were auto-floated when the tiled count is more than `maxWindows`. It returns lists of windows to float and unfloat. The engine does the mutations and emits the signals, so `TilingState` stays the single mutation point. |
| Restore | Autotile keeps no restore queue of its own. When a window closes while autotiled, its slot goes into the shared `WindowPlacementStore` (through `capturePlacement`). When a window with the same `appId` opens again, it goes back to the same tiling slot. The store keeps 16 entries at most for each `appId`, so it cannot grow without limit. |
| Per-algorithm settings | `AlgorithmSettings` (split ratio, master count, custom params) is saved when the user switches away from an algorithm and restored on the switch back. Thus each algorithm keeps its tuning. |

## Key types

| Type | Purpose |
|------|---------|
| `PhosphorTileEngine::AutotileEngine`          | Concrete `IPlacementEngine` for autotile screens |
| `PhosphorTileEngine::AutotileConfig`          | Global config: default algorithm, gaps, master count, per-algorithm `AlgorithmSettings` map |
| `PhosphorEngine::IAutotileSettings`           | Settings contract the engine reads (declared in `PhosphorEngine`, this library ships the implementation) |
| `PhosphorTileEngine::NavigationController`    | Stateless helper for focus, swap, rotate, split ratio and master count |
| `PhosphorTileEngine::OverflowManager`         | Per-screen record of the windows auto-floated when `maxWindows` is exceeded |
| `PhosphorTileEngine::PerScreenConfigResolver` | Resolves per-screen overrides over the global config |
| `PhosphorTileEngine::AlgorithmSettings`       | Per-algorithm split ratio, master count and custom params (saved on switch-away) |

## Use

Connect the engine in the composition root of the daemon:

```cpp
#include <PhosphorTileEngine/AutotileEngine.h>

using namespace PhosphorTileEngine;

auto* autotile = new AutotileEngine(layoutRegistry,           // from phosphor-zones
                                    windowTrackingService,
                                    screenManager,
                                    algorithmRegistry,        // from phosphor-tiles
                                    /*parent*/ daemon);
autotile->setWindowRegistry(windowRegistry);          // late-bound app-id resolver
autotile->setEngineSettings(autotileSettingsAdaptor); // base-class settings wiring
autotile->refreshConfigFromSettings();                // pull initial config from settings
```

The daemon then sends each screen's events to the engine of that screen. In phosphor-engine-runtime,
`ScreenModeRouter` takes the snap, autotile and scroll engines, and `engineFor(screenId)` returns the one to call.

## Design notes

- **Inherits `PlacementEngineBase`.** As the snap engine does, it uses the shared plumbing of the base (settings
  injection, pruning, common signals). It adds the intent dispatch for autotile.
- **`NavigationController` is stateless.** Each method sends reads and writes back to the engine. The controller
  holds only a back-pointer. Thus it isolates the navigation logic and copies no member state.
- **`OverflowManager` does not change `TilingState`.** It returns the lists of windows to float and unfloat. The
  engine does the mutations and emits the signals. Thus `TilingState` stays the only mutation point, and the
  overflow accounting is easy to test.
- **Per-app restore has a limit.** `WindowPlacementStore::MaxPerApp = 16` is the limit of the queue for each
  `appId`. Thus an app that behaves badly cannot grow the restore state without limit.
- **Algorithm settings are saved for each algorithm, not globally.** If the user switches from `master-stack` to
  `spiral` and back, the earlier split ratio and master count of `master-stack` come back. Users keep their tuning
  for each algorithm.

## Dependencies

- `QtCore`
- [`phosphor-engine`](../phosphor-engine/README.md): `IPlacementEngine`, `PlacementEngineBase`, `IWindowRegistry`,
  `IWindowTrackingService`, `PerScreenKeys`
- [`phosphor-tiles`](../phosphor-tiles/README.md): `TilingAlgorithm`, `TilingState`, `ITileAlgorithmRegistry`,
  `AutotileConstants`
- [`phosphor-zones`](../phosphor-zones/README.md): `Layout`, `LayoutRegistry`
- [`phosphor-layout-api`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-layout-api/README.md):
  `EdgeGaps`
- [`phosphor-screens`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-screens/README.md):
  `PhosphorScreens::ScreenManager`
- [`phosphor-identity`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-identity/README.md):
  window IDs (private link)

## See also

- [`phosphor-tiles`](../phosphor-tiles/README.md): the algorithm vocabulary, the Luau binding and `TilingState`.
- [`phosphor-snap-engine`](../phosphor-snap-engine/README.md): the sibling engine for manual zones.
