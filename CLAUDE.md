<!-- SPDX-FileCopyrightText: 2026 fuddlesworth -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-engine-libs

phosphor-engine-libs holds the window-placement libraries: the snap, tile and scroll engines, the zone,
tiling-algorithm and workspace models they place against, placement state, the zone overlays and the engine runtime a
host embeds. It builds on phosphor-libs. PlasmaZones is its consumer today, and pins one commit of it. The libraries
were split out of phosphor-libs, with their history, in November 2026.

The rules every phosphor-works repository follows come from the workspace repository, imported below. This file adds
phosphor-engine-libs' own. The shared Claude Code tooling (the command guard, `phosphor-works:build`,
`phosphor-works:debug`, `phosphor-works:code-audit`, the shared reviewers, the lesson index) comes from the
`phosphor-works` plugin, which `.claude/settings.json` enables and `workspace/tools/setup` installs.

@../workspace/claude/conventions.md

## Layout

- Each library is `libs/phosphor-<name>/` with its own `CMakeLists.txt`, `project()`, `include/`, `src/`, `tests/`
  and find_package config, so it also configures alone. The root `CMakeLists.txt` adds them in dependency order (a
  library that `find_package()`s a sibling comes after it).
- Headers are `<PhosphorX/...>`, the target `PhosphorX::PhosphorX`, the export macro `PHOSPHORX_EXPORT` and the
  namespace `PhosphorX`. `phosphor-context-resolver` uses `<PhosphorContext/...>`, the namespace `PhosphorContext` and
  `PHOSPHORCONTEXTRESOLVER_EXPORT`. `phosphor-workspaces` also builds `PhosphorWorkspacesPlasma`
  (`PHOSPHORWORKSPACESPLASMA_EXPORT`, `PhosphorWorkspaces::Plasma`) when PlasmaActivities is found.
- QML module: `org.phosphor.zonerendering`. The engine runtime ships the overlay QML and a translation catalog.
- `data/schemas/layout.schema.json` is the layout file's schema. PlasmaZones' `scripts/validate-json-schemas.py`
  reads it from the pinned commit. The layout contract itself (`ILayoutSource`, `LayoutPreview`, the zone JSON keys)
  is `phosphor-layout-api` in phosphor-libs.

## Layering

- These libraries depend on phosphor-libs and Qt only. Nothing here links phosphor-shell-libs or an app, and no
  library names an app's data directories. The app passes in the layout, algorithm and scrolling-template subdirs.
- The engine QML firewall in the root `CMakeLists.txt` fails the configure when a target in
  `PHOSPHOR_ENGINE_TARGETS` (zones, the three engines, `PhosphorEngine`, workspaces, placement) reaches `Qt6::Qml` or
  `Qt6::Quick` anywhere in its link closure, and names the chain. It also fails for a list entry that matches no
  target. Add a new engine library to that list. Today only `phosphor-zone-rendering` and `phosphor-engine-runtime` link
  Qml or Quick.
- Everything here is `LGPL-2.1-or-later`, the tests included.

## Placement invariants

- A screen runs one mode at a time (snapping, tiling or scrolling), resolved per (screen, desktop, activity) through
  `phosphor-context-resolver`.
- Float is per mode. `WindowPlacement` (`libs/phosphor-engine/include/PhosphorEngine/WindowPlacement.h`) keeps one
  `EngineSlot` per engine, keyed by `IPlacementEngine::engineId()`, so floating in snap does not float in tiling. A
  verdict from one mode never gates another.
- The free/float geometry is shared across modes in `freeGeometryByScreen`, keyed per screen. It is written only from
  a real free frame and frozen while the window is snapped or tiled. A zone or tile rect never enters it.
- A new engine keys its own slot and needs no change to `WindowPlacement`. Core code compares an `EngineSlot::state`
  only for equality.
- A cross-cutting feature usually needs an arm in all three engines. Check each before you call it done.
- Luau algorithms run through `phosphor-tiles` on phosphor-libs' `phosphor-scripting`. A zero-size or negative rect
  from a script reaches the engine directly, so validate it at the boundary.

## How apps consume it

- PlasmaZones adds phosphor-libs first, then this tree, both with CPM from the commits
  `cmake/PlasmaZonesDependencies.cmake` pins. `phosphor_engine_libs_add_phosphor_libs()` sees the phosphor-libs targets
  already there and fetches nothing, so the app's phosphor-libs pin wins over the one here. Keep the two equal when
  you move either.
- After a change here merges, move PlasmaZones' pin: VERSION, URL, FILE and SHA256 together, and the same archive in
  its recipes. Build it against a local checkout with `-DCPM_phosphor-engine-libs_SOURCE=<absolute path>`, or use the
  `workspace` superbuild to change a library and the app together.

## As a subproject

- Only when `PHOSPHOR_ENGINE_LIBS_TOP_LEVEL` is on does the tree set C++20, unity builds, `ENABLE_PCH`, ccache, ECM,
  the KDE settings, `BUILD_TESTING` (default `OFF`), `QT_NO_CAST_FROM_ASCII` and clang-format.
- The Qt lookups run at this tree's top scope before phosphor-libs is added, so the IMPORTED private Qt targets are
  visible to the libraries here.
- The test helpers come from phosphor-libs through `PHOSPHOR_LIBS_CMAKE_DIR`, which is on `CMAKE_MODULE_PATH` here:
  `PhosphorTestIsolation`, `PhosphorLibTesting` and `PhosphorTestPch`. A test tree that cannot find
  `PhosphorTestIsolation.cmake` (a standalone configure of one library) falls back to a stand-in with no isolation.

## Build and test

- `cmake --workflow --preset dev` configures into `build/`, builds phosphor-libs and every library here, and tests.
  The other presets are `release` and `relwithdebinfo`. Follow `phosphor-works:build` for the memory cap, jobs, the
  warning count and the non-unity gate.
- The suite includes phosphor-libs' tests, because that tree builds as a subproject with tests on.
- CI is the shared C++ CI of the workspace repository (`cpp-ci.yml`): the `dev` preset on Arch and on Fedora 44 (older
  Qt), with a warning failing the build, clang-format and qmlformat on every tracked file, and `nix flake check`.
  Tests run with `QT_QPA_PLATFORM=offscreen`.
- This repository has no pre-commit hooks. Run `clang-format -i` and `qmlformat -i` on the files you touch.

## Pins and ABI

- `cmake/PhosphorEngineLibsDependencies.cmake` pins only the phosphor-libs commit (and CPM.cmake). Luau and jsoncons
  come from phosphor-libs' own pins at that commit. `nix/package.nix` repeats the phosphor-libs archive.
- `-DCPM_phosphor-libs_SOURCE=<dir>` builds against a local phosphor-libs, `-DPHOSPHOR_LIBS_GIT_REF=<ref>` clones a
  branch or commit, and `-DPHOSPHOR_DEPS_ARCHIVES=<dir>` builds offline (fill it with
  `cmake -DPHOSPHOR_DEPS_FETCH=<dir> -P cmake/PhosphorEngineLibsDependencies.cmake`).
- PlasmaActivities is optional. With it, `phosphor-workspaces` follows Plasma's activities.
- Each shared library has `SOVERSION 0`. There is no ABI check yet.

## Where things are

| Library | What it holds |
| --- | --- |
| `phosphor-engine` | Engine contracts (`IPlacementEngine`, `IPlacementState`), window registry, `WindowPlacement` |
| `phosphor-snap-engine` | Zone-based snap placement engine |
| `phosphor-tile-engine` | Autotile placement engine (`AutotileEngine`, config, navigation) |
| `phosphor-scroll-engine` | niri-style scrolling engine (`ScrollStrip` model of columns on an endless strip) |
| `phosphor-zones` | Zone and layout primitives, `LayoutManager`, zone detection |
| `phosphor-tiles` | Tiling-algorithm layer (`TilingAlgorithm`, `AlgorithmRegistry`, `SplitTree`, `LuauTileAlgorithm`) |
| `phosphor-workspaces` | Virtual desktops and workspaces, optional Plasma activities |
| `phosphor-placement` | Placement state tracking (window to zone, floating, auto-snap, resnap) |
| `phosphor-context-resolver` | Frozen per-screen snapshot of the effective mode and the disable and lock cascade |
| `phosphor-zone-rendering` | Zone overlays drawn with shader packs (`ZoneShaderItem`, `org.phosphor.zonerendering`) |
| `phosphor-engine-runtime` | Runtime a host embeds (settings, window tracking, services, overlays, `EngineRuntime`) |
