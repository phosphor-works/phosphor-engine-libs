<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-engine-libs

phosphor-engine-libs is a set of LGPL Qt 6 libraries for window placement. It holds the snap, tile and scroll engines,
the zone, tiling-algorithm and workspace models that they place against, placement state, the zone overlays and the
engine runtime that a host embeds. They are the placement code of
[PlasmaZones](https://github.com/phosphor-works/PlasmaZones) and build on
[phosphor-libs](https://github.com/phosphor-works/phosphor-libs).

## Libraries

Each library is in `libs/<name>/` with its own `CMakeLists.txt`, headers, sources and tests. The table has one row
for each library, in three areas.

| Library | Area | Description |
| --- | --- | --- |
| [`phosphor-engine`](libs/phosphor-engine/README.md) | Engines | The placement-engine interface that a daemon dispatches through, and the shared interfaces that every engine uses. |
| [`phosphor-snap-engine`](libs/phosphor-snap-engine/README.md) | Engines | The manual engine that places windows in the zones of a layout that the user draws. |
| [`phosphor-tile-engine`](libs/phosphor-tile-engine/README.md) | Engines | The automatic-tiling engine that lays out the windows again when they open, close or change focus. |
| [`phosphor-scroll-engine`](libs/phosphor-scroll-engine/README.md) | Engines | The scrolling engine, after niri, that puts windows in columns on a strip with no end. |
| [`phosphor-zones`](libs/phosphor-zones/README.md) | Models | The zone data model, layout persistence, zone detection and the `ILayoutSource` adapter for manual layouts. |
| [`phosphor-tiles`](libs/phosphor-tiles/README.md) | Models | The Luau tiling algorithms, `TilingState`, the algorithm registry and the `ILayoutSource` adapter for autotile output. |
| [`phosphor-workspaces`](libs/phosphor-workspaces/README.md) | Models | Virtual desktop and activity tracking, and the dynamic-workspace model for each monitor. |
| [`phosphor-placement`](libs/phosphor-placement/README.md) | Models | Window placement state: zone assignments, floating state, auto-snap, resnap, rotation and empty-zone queries. |
| [`phosphor-context-resolver`](libs/phosphor-context-resolver/README.md) | Models | A frozen snapshot of the mode, desktop, activity, disable and lock state that applies to each screen. |
| [`phosphor-engine-runtime`](libs/phosphor-engine-runtime/README.md) | Hosting | The runtime that a host embeds: the host of the engines, with its services, overlays and settings. |
| [`phosphor-zone-rendering`](libs/phosphor-zone-rendering/README.md) | Hosting | `ZoneShaderItem`, the Qt Quick item that draws a zone-overlay shader pack over the zones of a layout. |

`phosphor-layout-api`, in phosphor-libs, is the layout contract that these libraries share with other readers of
layout files. It holds `ILayoutSource`, `LayoutPreview`, the zone JSON keys and the per-side gap keys.

The engine libraries hold the authoritative placement state and do not use the QML engine. The configure step fails
if one of them reaches `Qt6::Qml` or `Qt6::Quick` through its link closure. `CMakeLists.txt` has the check and the
reason for it.

## Build

The build needs what phosphor-libs needs: CMake 3.23 (3.25 for the presets below), Qt 6.10 with its private headers,
extra-cmake-modules, Wayland, and the Vulkan headers where Qt was built with Vulkan. PlasmaActivities is optional. If
it is present, `phosphor-workspaces` follows the activities of Plasma.

The first configure downloads the phosphor-libs commit that `cmake/PhosphorEngineLibsDependencies.cmake` pins. It also
downloads the archives that this commit pins. The downloads go into `.cache/cpm`.

```sh
cmake --workflow --preset dev
```

This command configures into `build/`, builds phosphor-libs and every library here, and runs the tests.

To build against a local phosphor-libs checkout, add `-DCPM_phosphor-libs_SOURCE=/path/to/phosphor-libs` to the
configure.

## Use from an app

An app adds phosphor-libs first and then this repository, both with [CPM](https://github.com/cpm-cmake/CPM.cmake).
The app pins each one to one commit by archive URL and SHA-256. The app's phosphor-libs pin is then the one in use,
and this tree fetches nothing of its own.

```cmake
CPMAddPackage(NAME phosphor-engine-libs
              URL "https://github.com/phosphor-works/phosphor-engine-libs/archive/<commit>.tar.gz"
              URL_HASH SHA256=<sha256>)
```

To build the app against a local checkout, configure it with
`-DCPM_phosphor-engine-libs_SOURCE=/path/to/phosphor-engine-libs`.

The libraries name no data directory of an app. The app gives each directory to the classes and functions that read
it: the layout, algorithm and scrolling-template subdirectories.

## Contributing

The suite conventions are in the [workspace](https://github.com/phosphor-works/workspace) repository, in
`claude/conventions.md`. `CLAUDE.md` gives the layout and the rules of this repository. `lefthook install` sets up
the Git hooks from `lefthook.yml`. The hooks format the staged files and check the Conventional Commits message.

## License

The libraries are LGPL-2.1-or-later. The text is in `COPYING.LESSER`.
