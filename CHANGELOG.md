<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Changelog

This file records each release of phosphor-engine-libs, the placement engine libraries: zones, tiles, scrolling and snapping.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html), as the phosphor-works conventions say.

## [Unreleased]

### Fixed

- `phosphor-engine-runtime`: the snap channel routes a window that opened before the daemon was ready. The daemon-ready sweep counts as a first placement for a window that no engine placed, which `WindowTracker::wasPlacedByAnyEngine` decides from engine slots, not from the geometry-only record. The tiling channel uses the same test. ([PlasmaZones#1315](https://github.com/phosphor-works/PlasmaZones/issues/1315))
- `phosphor-snap-engine`, `phosphor-tile-engine`: a directional move that takes a window to the next virtual desktop then activates it, so the compositor shows that desktop and the window keeps the focus. ([PlasmaZones#1313](https://github.com/phosphor-works/PlasmaZones/issues/1313))

## [0.1.0] - 2026-11-26

The first release. It has 11 libraries: `phosphor-context-resolver`, `phosphor-engine-runtime`, `phosphor-engine`, `phosphor-placement`, `phosphor-scroll-engine`, `phosphor-snap-engine`, `phosphor-tile-engine`, `phosphor-tiles`, `phosphor-workspaces`, `phosphor-zone-rendering`, `phosphor-zones`. The tag `v0.1.0` is commit `bc9c772c`, which PlasmaZones pins.

[Unreleased]: https://github.com/phosphor-works/phosphor-engine-libs/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/phosphor-works/phosphor-engine-libs/releases/tag/v0.1.0
