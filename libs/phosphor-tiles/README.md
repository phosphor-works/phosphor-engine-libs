<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-tiles

Tiling algorithms (Luau scripts through the `pluau` standard library), `TilingState`, the algorithm registry, and
the `ILayoutSource` adapter that publishes autotile output to the daemon.

## Responsibility

Zones are static layouts that the user draws in an editor. Autotiling is the *dynamic* counterpart. As windows open
and close, a tiling algorithm calculates the zone boundaries again. This library owns the **algorithm vocabulary**
and the **per-screen tiling state**. The runtime engine that drives them is in
[`phosphor-tile-engine`](../phosphor-tile-engine/README.md). The generic Luau host (sandbox, watchdog, marshalling)
is in [`phosphor-scripting`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-scripting/README.md).

| Part | What it is |
| --- | --- |
| Algorithms | `TilingAlgorithm` is the base. `LuauTileAlgorithm` is the single concrete implementation, a `TilingAlgorithm` that delegates to a Luau script. Every layout (binary-split, master-stack, columns, spiral, …) ships as a `.luau` script in `data/algorithms/`. There are no hard-coded C++ geometry algorithms. |
| `pluau` standard library | Scripts use `pluau`, a Luau table of geometry primitives: a `Rect` with gap-aware splits, plus about 25 layout and helper functions. It is compiled into the library as a Qt resource (`pluau.luau`). It is injected as a **frozen** global before each untrusted script runs. `pluau.d.luau` gives its types to editors. |
| Sandbox and limits | `phosphor-scripting`'s `LuauEngine` provides them, not this library: `luaL_sandbox` read-only globals, a heap cap for each engine, and a shared `LuauWatchdog` that stops a runaway script at a CPU deadline. |
| Loader | `ScriptedAlgorithmLoader` finds `*.luau` files in system and user data directories that the consumer chooses. It validates each basename, creates a `LuauTileAlgorithm` and registers it. It reloads through `QFileSystemWatcher`, with a debounced refresh. |
| Registry | `ITileAlgorithmRegistry` is the read-side contract. `AlgorithmRegistry` is the concrete catalog, with the same shape as the [`phosphor-zones`](../phosphor-zones/README.md) `LayoutRegistry`. |
| State | `TilingState` records the per-screen window order, master count, split tree and floating set. It implements `PhosphorEngine::IPlacementState`, so the persistence and D-Bus surface of the daemon treat it the same as `SnapState`. |
| Layout source | `AutotileLayoutSource` wraps a registry as an `ILayoutSource`. Thus the rest of the stack uses autotile output the same way as manual zones. `AutotileLayoutSourceFactory` registers this source through `LayoutSourceProviderRegistry`. |
| Preview | `AutotilePreviewRender` paints a layout thumbnail directly on a `QPainter` for the algorithm-picker UI. |

## Key types

| Type | Purpose |
|------|---------|
| `PhosphorTiles::TilingAlgorithm`             | Abstract base: `calculateZones(const TilingParams&) -> QVector<QRect>` |
| `PhosphorTiles::LuauTileAlgorithm`           | The concrete `TilingAlgorithm` that delegates to a Luau script |
| `PhosphorTiles::TilingState`                 | Per-screen window order, master count and split tree. Implements `IPlacementState` |
| `PhosphorTiles::SplitTree`                   | Binary-split tree node and ratio for tree-style algorithms |
| `PhosphorTiles::ITileAlgorithmRegistry`      | Read-side contract. Subclasses `ILayoutSourceRegistry` |
| `PhosphorTiles::AlgorithmRegistry`           | Concrete catalog of the registered `LuauTileAlgorithm`s |
| `PhosphorTiles::ScriptedAlgorithmLoader`     | Finds `*.luau` files, validates names, creates and registers `LuauTileAlgorithm`s |
| `PhosphorTiles::AutotileLayoutSource`        | `ILayoutSource` adapter that wraps a registry |
| `PhosphorTiles::AutotileLayoutSourceFactory` | Provider factory. Registers with `LayoutSourceProviderRegistry` |
| `PhosphorTiles::AutotilePreviewRender`       | Helper that paints a thumbnail for the algorithm picker |

The `pluau` standard library (`src/pluau/pluau.luau`) and its type stubs (`src/pluau/pluau.d.luau`) are the API
surface on the Luau side. See the [tiling algorithm authoring guide](https://phosphor-works.org/guides/tiling/).

## Use

Find and run the bundled and user Luau algorithms:

```cpp
#include <PhosphorTiles/AlgorithmRegistry.h>
#include <PhosphorTiles/AutotileLayoutSource.h>
#include <PhosphorTiles/ScriptedAlgorithmLoader.h>

using namespace PhosphorTiles;

AlgorithmRegistry reg;

// Point a loader at an XDG-relative subdirectory and scan. The bundled
// algorithms live in the system data dir; user algorithms override by
// filename from the writable data dir. Each discovered *.luau registers a
// LuauTileAlgorithm against the injected registry. The placement apps share
// one tree, phosphor-works/algorithms (phosphor-paths names it).
ScriptedAlgorithmLoader loader(PhosphorPaths::commonDataSubdir(QStringLiteral("algorithms")), &reg);
loader.scanAndRegister();

auto *alg = reg.algorithm(QStringLiteral("master-stack"));
AutotileLayoutSource src(&reg);
// … src is an ILayoutSource the same way ZonesLayoutSource is.
```

A minimal Luau algorithm (for example `$XDG_DATA_HOME/phosphor-works/algorithms/vstack.luau`):

```lua
-- Single column, each window gets an equal vertical slice.
local pluau = pluau

return pluau.algorithm {
    metadata = {
        name = "Vertical Stack",
        id = "vstack",
        description = "Single column, equal vertical slices",
        minimumWindows = 1,
    },

    tile = function(ctx)
        if ctx.windowCount <= 0 then
            return {}
        end
        return ctx.area:rows(ctx.windowCount, ctx.innerGap)
    end,
}
```

## Design notes

- **The library holds the algorithms and the state. The engine and the host are elsewhere.** The runtime engine
  that drives tiling (window-open, focus and float events) is
  [`phosphor-tile-engine`](../phosphor-tile-engine/README.md). The generic Luau VM host is
  [`phosphor-scripting`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-scripting/README.md).
  Because of this split, a settings-UI preview does not link the engine to render a thumbnail. Also, the Luau host
  can be used for more than tiling.
- **The sandbox is defensive.** `LuauEngine` freezes the global table and the `pluau` stdlib (`luaL_sandbox`)
  before an untrusted script runs. A script has no `io`, no `os.execute`, and no filesystem or network access. The
  engine limits both the run time (the watchdog) and the heap (the capped allocator).
- **Algorithms use positions, not window IDs.** An algorithm takes a `TilingParams` and returns a list of zone
  rects. `TilingParams` carries the window count, usable area, inner gap, per-window metadata, screen info and
  custom params. The consuming layer then maps windows to zones. Thus the same algorithm drives preview rendering
  and owns no window bindings.
- **Preview render is headless.** `AutotilePreviewRender` paints a layout thumbnail that a picker can use in
  `QListView` delegates.

## Dependencies

- `QtCore`
- [`phosphor-scripting`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-scripting/README.md):
  the embedded Luau host (`LuauEngine`, `LuauWatchdog`)
- [`phosphor-layout-api`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-layout-api/README.md):
  the `ILayoutSource`, factory and registry contracts
- [`phosphor-engine`](../phosphor-engine/README.md): `IPlacementState` (`TilingState` implements it)
- [`phosphor-fsloader`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-fsloader/README.md):
  the filesystem-backed registry skeleton for algorithm discovery

## See also

- [`phosphor-tile-engine`](../phosphor-tile-engine/README.md): the runtime engine that drives algorithms when
  window events occur.
- [`phosphor-scripting`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-scripting/README.md):
  the generic embedded Luau host.
