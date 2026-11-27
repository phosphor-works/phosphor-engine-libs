<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Changelog

This file records each release of phosphor-engine-libs, the placement engine libraries: zones, tiles, scrolling and snapping.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html), as the phosphor-works conventions say.

## [Unreleased]

### Changed

- Builds on phosphor-libs `bf8bf88b`, which installs the licence notices of the Wayland protocols that
  phosphor-wayland compiles in (phosphor-works/PlasmaZones#1193).

### Fixed

- `phosphor-engine-runtime`: the snap channel routes a window that opened before the daemon was ready. The daemon-ready sweep counts as a first placement for a window that no engine placed, which `WindowTracker::wasPlacedByAnyEngine` decides from engine slots, not from the geometry-only record. The tiling channel uses the same test. ([PlasmaZones#1315](https://github.com/phosphor-works/PlasmaZones/issues/1315))
- `phosphor-scroll-engine`: a saved strip tile outranks another window's floating record when a window opens. `ScrollEngine::stripStashHasTileFor` tells the open path that the stash has a tile for the window. ([PlasmaZones#1301](https://github.com/phosphor-works/PlasmaZones/issues/1301))
- `phosphor-scroll-engine`: the own-fullscreen hold (`setWindowFullscreenFloat`) keeps the tile in its column and announces no float, so no other column moves. An opening window does not join the held column, and committed-size and minimum-size reports wait for the end of the hold. `isFullscreenFloated` is renamed `isFullscreenHeld`. ([PlasmaZones#1266](https://github.com/phosphor-works/PlasmaZones/issues/1266))
- `phosphor-snap-engine`, `phosphor-tile-engine`: a directional move that takes a window to the next virtual desktop then activates it, so the compositor shows that desktop and the window keeps the focus. ([PlasmaZones#1313](https://github.com/phosphor-works/PlasmaZones/issues/1313))
- `phosphor-scroll-engine`: a window that the stash restore focuses as the strip's stashed focus keeps the focus and the stashed view when it is re-seated as a re-placement, such as at a monitor's return. The open path rewound the focus to the column seated first, which left the focused window parked off the screen. ([PlasmaZones#1301](https://github.com/phosphor-works/PlasmaZones/issues/1301))

## [0.1.0] - 2026-11-26

The first release. It has 11 libraries: `phosphor-context-resolver`, `phosphor-engine-runtime`, `phosphor-engine`, `phosphor-placement`, `phosphor-scroll-engine`, `phosphor-snap-engine`, `phosphor-tile-engine`, `phosphor-tiles`, `phosphor-workspaces`, `phosphor-zone-rendering`, `phosphor-zones`. The tag `v0.1.0` is commit `bc9c772c`, which PlasmaZones pins.

[Unreleased]: https://github.com/phosphor-works/phosphor-engine-libs/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/phosphor-works/phosphor-engine-libs/releases/tag/v0.1.0
