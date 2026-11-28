<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Changelog

This file records each release of phosphor-engine-libs, the placement engine libraries: zones, tiles, scrolling and snapping.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html), as the phosphor-works conventions say.

## [Unreleased]

### Added

- An app can embed the libraries (phosphor-libs' `PHOSPHOR_EMBED_APP`): the install then holds only the shared
  libraries that the app's programs link, in the app's own directory. With phosphor-tiles, the type definitions for
  tiling scripts (`pluau.d.luau`) go in `share/phosphor-works/<app>`. Every install rule goes through the functions of
  phosphor-libs' `cmake/PhosphorSuite.cmake`.
- `phosphor-tile-engine`: `AutotileEngine::toggleMaximizeToEdges` maximizes one tiled window to the screen's available
  area with no gaps. The window keeps its tile and its place in the order, and the other tiles keep their rects,
  because the engine applies the maximize after the algorithm. Every algorithm gets it with no change. The batch
  entry carries the `maximizedToEdges` flag, as a scrolling column does. The state ends when the window closes,
  leaves the engine, joins a monocle stack or moves to another desktop or screen. A float sets it aside until the
  window tiles again. `phosphor-engine-runtime`: `AutotileService::toggleMaximizeToEdges`
  is the verb behind it. ([workspace#28](https://github.com/phosphor-works/workspace/issues/28))
- `phosphor-engine-runtime`: own workspaces. With the `Workspaces.Behavior` option `FullscreenOwnWorkspace`,
  `MaximizedOwnWorkspace` or `BorderlessOwnWorkspace` on (all off by default), `WorkspaceController` moves a window that
  goes fullscreen, that the user maximizes, or that has no decoration and covers its whole screen, to a new workspace
  directly after the one it was on, and shows it. When the window leaves that state it moves back and the monitor shows
  the origin again. When it closes, the monitor shows the origin and the window last active there takes focus. A new
  ordinary window that opens on an own workspace goes to the origin, and its close shows the own workspace again with
  its owner focused. The records and the engines' holds are kept in `own-workspaces.json` in the app's state
  directory, so a daemon restart keeps them. ([PlasmaZones#1201](https://github.com/phosphor-works/PlasmaZones/issues/1201))
- `phosphor-engine`: `PlacementEngineBase::holdForOwnWorkspace` and `dropOwnWorkspaceHold`, and the
  `windowMaximizedToEdgesChanged` signal. `phosphor-tile-engine` and `phosphor-scroll-engine` implement the hold: the
  window keeps its maximize across the trip and comes back to its place in the order or its column slot.
  `ScrollEngine::isWindowedFullscreen` and `ScrollEngine::isMaximizedToEdges` answer for one window.
  ([PlasmaZones#1201](https://github.com/phosphor-works/PlasmaZones/issues/1201))
- `phosphor-workspaces`: `WorkspaceReconciler::reorderWorkspace` moves any workspace to a slot of its monitor's list.
- `phosphor-engine`: `EngineSlot::maximizedToEdges`, saved in the placement store. `phosphor-tile-engine` records a
  tile's maximize to edges there and gives it back on the window's first insert after a daemon restart.

### Changed

- The libraries' tests build only in this repository's own build, or in a library configured on its own. An app or
  the workspace superbuild that adds the tree with `BUILD_TESTING` on no longer builds them.
- Builds on phosphor-libs `bf8bf88b`, which installs the licence notices of the Wayland protocols that
  phosphor-wayland compiles in (phosphor-works/PlasmaZones#1193).

### Fixed

- `phosphor-tile-engine`: a tiling state made for a context that is not in view (a desktop the screen does not show,
  as after a daemon restart) starts with the screen's split ratio and master count: its override, or the default of the
  algorithm the screen is pinned to. It took the global values, so such a desktop came back with another ratio.
- `phosphor-engine-runtime`: the snap channel routes a window that opened before the daemon was ready. The daemon-ready sweep counts as a first placement for a window that no engine placed, which `WindowTracker::wasPlacedByAnyEngine` decides from engine slots, not from the geometry-only record. The tiling channel uses the same test. ([PlasmaZones#1315](https://github.com/phosphor-works/PlasmaZones/issues/1315))
- `phosphor-scroll-engine`: a saved strip tile outranks another window's floating record when a window opens. `ScrollEngine::stripStashHasTileFor` tells the open path that the stash has a tile for the window. ([PlasmaZones#1301](https://github.com/phosphor-works/PlasmaZones/issues/1301))
- `phosphor-scroll-engine`: the own-fullscreen hold (`setWindowFullscreenFloat`) keeps the tile in its column and announces no float, so no other column moves. An opening window does not join the held column, and committed-size and minimum-size reports wait for the end of the hold. `isFullscreenFloated` is renamed `isFullscreenHeld`. ([PlasmaZones#1266](https://github.com/phosphor-works/PlasmaZones/issues/1266))
- `phosphor-snap-engine`, `phosphor-tile-engine`: a directional move that takes a window to the next virtual desktop then activates it, so the compositor shows that desktop and the window keeps the focus. ([PlasmaZones#1313](https://github.com/phosphor-works/PlasmaZones/issues/1313))
- `phosphor-scroll-engine`: a window that the stash restore focuses as the strip's stashed focus keeps the focus and the stashed view when it is re-seated as a re-placement, such as at a monitor's return. The open path rewound the focus to the column seated first, which left the focused window parked off the screen. ([PlasmaZones#1301](https://github.com/phosphor-works/PlasmaZones/issues/1301))

## [0.1.0] - 2026-11-26

The first release. It has 11 libraries: `phosphor-context-resolver`, `phosphor-engine-runtime`, `phosphor-engine`, `phosphor-placement`, `phosphor-scroll-engine`, `phosphor-snap-engine`, `phosphor-tile-engine`, `phosphor-tiles`, `phosphor-workspaces`, `phosphor-zone-rendering`, `phosphor-zones`. The tag `v0.1.0` is commit `bc9c772c`, which PlasmaZones pins.

[Unreleased]: https://github.com/phosphor-works/phosphor-engine-libs/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/phosphor-works/phosphor-engine-libs/releases/tag/v0.1.0
