<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Changelog

This file records each release of phosphor-engine-libs, the placement engine libraries: zones, tiles, scrolling and snapping.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html), as the phosphor-works conventions say.

## [Unreleased]

### Added

- `PHOSPHOR_USE_INSTALLED_LIBS`: with this CMake option on, the build uses the installed `PhosphorLibs` package
  instead of the pinned phosphor-libs archive, and fetches nothing. It is off by default.

## [0.1.0] - 2026-11-26

The first release. It has 11 libraries: `phosphor-context-resolver`, `phosphor-engine-runtime`, `phosphor-engine`, `phosphor-placement`, `phosphor-scroll-engine`, `phosphor-snap-engine`, `phosphor-tile-engine`, `phosphor-tiles`, `phosphor-workspaces`, `phosphor-zone-rendering`, `phosphor-zones`. The tag `v0.1.0` is commit `bc9c772c`, which PlasmaZones pins.

[Unreleased]: https://github.com/phosphor-works/phosphor-engine-libs/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/phosphor-works/phosphor-engine-libs/releases/tag/v0.1.0
