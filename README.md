# phosphor-engine-libs

LGPL Qt 6 libraries for window placement: the snap, tile and scroll engines, the zone, tiling-algorithm and workspace
models they place against, placement state, the zone overlays and the engine runtime a host embeds. They are the
placement code of [PlasmaZones](https://github.com/phosphor-works/PlasmaZones) and build on
[phosphor-libs](https://github.com/phosphor-works/phosphor-libs).

| Area | Libraries |
| --- | --- |
| Engines | `phosphor-engine`, `phosphor-snap-engine`, `phosphor-tile-engine`, `phosphor-scroll-engine` |
| Models | `phosphor-zones`, `phosphor-tiles`, `phosphor-workspaces`, `phosphor-placement`, `phosphor-context-resolver` |
| Hosting | `phosphor-engine-runtime`, `phosphor-zone-rendering` |

Each library lives in `libs/<name>/` with its own `CMakeLists.txt`, headers, sources and tests. The layout contract they
share with other readers of layout files (`ILayoutSource`, `LayoutPreview`, the zone JSON keys, the per-side gap keys)
is `phosphor-layout-api`, in phosphor-libs.

The engine libraries hold authoritative placement state and stay free of the QML engine: configuring fails if any of
them reaches `Qt6::Qml` or `Qt6::Quick` through its link closure. The check, and why it exists, is in `CMakeLists.txt`.

## Build

Needs what phosphor-libs needs (CMake 3.23, 3.25 for the presets below, Qt 6.10 with its private headers,
extra-cmake-modules, Wayland, the Vulkan headers where Qt was built with Vulkan). PlasmaActivities is optional: with it,
`phosphor-workspaces` follows Plasma's activities. The first configure downloads the phosphor-libs commit that
`cmake/PhosphorEngineLibsDependencies.cmake` pins, and the archives that commit pins, into `.cache/cpm`.

```sh
cmake --workflow --preset dev
```

That configures into `build/`, builds phosphor-libs and every library here, and runs the tests. To build against a
local phosphor-libs checkout, add `-DCPM_phosphor-libs_SOURCE=/path/to/phosphor-libs` to the configure.

## Use from an app

An app adds phosphor-libs first, then this repository, both with [CPM](https://github.com/cpm-cmake/CPM.cmake) and
pinned to one commit each by archive URL and SHA-256. The app's phosphor-libs pin is then the one in use, and this
tree fetches nothing of its own:

```cmake
CPMAddPackage(NAME phosphor-engine-libs
              URL "https://github.com/phosphor-works/phosphor-engine-libs/archive/<commit>.tar.gz"
              URL_HASH SHA256=<sha256>)
```

To build the app against a local checkout instead, configure it with
`-DCPM_phosphor-engine-libs_SOURCE=/path/to/phosphor-engine-libs`.

The libraries name no app's data directories. The app passes each one in: the layout, algorithm and
scrolling-template subdirectories to the classes and functions that read them.

## History

These libraries were split out of phosphor-libs, with their history, in November 2026.

## License

LGPL-2.1-or-later. See `COPYING.LESSER`.
