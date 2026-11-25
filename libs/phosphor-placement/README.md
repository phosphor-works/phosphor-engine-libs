<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-placement

The state of window placement: zone assignments, floating state, auto-snap, resnap, rotation and empty-zone queries.

## Responsibility

This library holds the state of window and zone management. It records which windows are in which zones and
handles float and unfloat. It auto-snaps a new window to the zone it used last, and resnaps when layouts or screens
change. It rotates windows between zones, and gives empty-zone queries for snap assist.

The daemon owns a `WindowTrackingService` instance and makes it available over D-Bus. Compositor plugins get the
resulting geometry through that surface. They do not link this library directly.

## Key types

| Type | Purpose |
|------|---------|
| `WindowTrackingService` | The main service: zone assignments, floating, auto-snap, resnap, rotation |
| `IGeometryResolver` | Interface for gap and padding resolution (the consumer implements it) |
| `PlacementConfig` | Value struct for the runtime configuration (in place of an ISettings dependency) |
| `SnapStateResolver` | Injectable seam that maps a window or screen to the `SnapState` that owns it |

## Use

```cpp
#include <PhosphorPlacement/WindowTrackingService.h>
#include <PhosphorPlacement/IGeometryResolver.h>
#include <PhosphorPlacement/PlacementConfig.h>

class MyResolver : public PhosphorPlacement::IGeometryResolver {
    int resolveInnerGap(PhosphorZones::Layout* layout, const QString& screenId) const override {
        return 8;
    }
    PhosphorLayout::EdgeGaps resolveOuterGaps(PhosphorZones::Layout* layout, const QString& screenId) const override {
        return PhosphorLayout::EdgeGaps::uniform(8);
    }
    int defaultBorderWidth() const override { return 2; }
    int defaultBorderRadius() const override { return 0; }
    int snapBorderInset() const override { return 0; }
};

MyResolver resolver;
PhosphorPlacement::WindowTrackingService wts(
    layoutManager, screenManager, vdm, &resolver);
wts.setSnapState(snapState);
wts.setWindowRegistry(registry);
```

## Snap and float state

Snap state is kept for each (screen, desktop, activity). Thus production code connects a `SnapStateResolver`
through `setSnapStateResolver`. The `setSnapState` overload in the example above is the convenience shim for one
store. It builds a resolver whose every arm goes to that one store. It suits unit tests and consumers with one store.

Each engine owns its float state. This service does not. A window floated in snapping mode is not floated for the
autotile or scrolling engine. Never read the float verdict of one engine to gate the placement of another.

## Dependencies

- `QtCore`, `QtGui`
- [`phosphor-engine`](../phosphor-engine/README.md), [`phosphor-snap-engine`](../phosphor-snap-engine/README.md), [`phosphor-zones`](../phosphor-zones/README.md), [`phosphor-protocol`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-protocol/README.md), [`phosphor-screens`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-screens/README.md), [`phosphor-workspaces`](../phosphor-workspaces/README.md), [`phosphor-identity`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-identity/README.md), [`phosphor-layout-api`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-layout-api/README.md)
