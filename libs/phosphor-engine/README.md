<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-engine

The common placement-engine interface that a daemon dispatches through. It also holds the shared interfaces that
every engine uses: the window registry, window tracking, virtual desktops and geometry settings.

## Responsibility

The manual snap engine ([`phosphor-snap-engine`](../phosphor-snap-engine/README.md)) and the automatic autotile
engine ([`phosphor-tile-engine`](../phosphor-tile-engine/README.md)) do the same kinds of work when a user presses a
shortcut or a window opens or closes. They move focus, swap windows, assign a window to a zone, and react to the
window event. Without a shared interface, a daemon must branch on the current mode for each event.
`phosphor-engine` names each of those operations as a user *intent*. Each engine fulfills the intent in its own
terms, so the hot path of the daemon is one polymorphic call.

The library also owns the **shared service contracts** that every engine reads. These are the canonical
window-tracking facade, the window-id canonicalizer, the virtual-desktop poll and the per-screen geometry settings.
The engines do not keep their own copies, and the daemon connects them one time.

## Key types

| Type | Purpose |
|------|---------|
| `PhosphorEngine::IPlacementEngine`       | Intent dispatcher. Every snap, move, focus, swap and assign call goes through it. |
| `PhosphorEngine::IPlacementState`        | Read-only per-screen state contract. Persistence and D-Bus use it and do not need to know which engine made it. |
| `PhosphorEngine::PlacementEngineBase`    | Base class with the shared engine plumbing (settings injection, pruning of stale windows, common signals). Per-window placement state is in the engines and in the unified `WindowPlacementStore`. Each engine adds only its mode-specific logic. |
| `PhosphorEngine::NavigationContext`      | `(windowId, screenId)` target for an intent. It can be empty for a shortcut early in startup. |
| `PhosphorEngine::IWindowRegistry`        | Window-id canonicalizer and `appId` lookup from an instance. |
| `PhosphorEngine::IWindowTrackingService` | Store that all engines share for zone assignments, pre-tile geometries and floating state. |
| `PhosphorEngine::IVirtualDesktopManager` | Tells which virtual desktop is current. A minimal interface with one method. |
| `PhosphorEngine::IGeometrySettings`      | Per-screen padding, outer-gap and per-side gap settings. |
| `PhosphorEngine::TilingStateKey`         | `(screenId, desktop, activity)` composite key for per-context state. |
| `PhosphorEngine::PerScreenKeys`          | JSON key constants for the per-screen overrides on disk. |
| `PhosphorEngine::JsonKeys`               | JSON key constants for the state serialization round trip. |

## Use

The host gets the engine of a screen (for example from `ScreenModeRouter::engineFor` in phosphor-engine-runtime) and
sends the intent to it. The call is the same in every mode:

```cpp
#include <PhosphorEngine/IPlacementEngine.h>
#include <PhosphorEngine/NavigationContext.h>

using namespace PhosphorEngine;

void focusLeft(IPlacementEngine* engine, const QString& windowId, const QString& screenId)
{
    const NavigationContext ctx{windowId, screenId};
    engine->focusInDirection(QStringLiteral("left"), ctx);
}
```

## Design notes

- **The interface names intents, not steps.** "Move focused window left" has a different meaning in tile-swap mode
  and in zone-snap mode. The interface names the request of the user, and each engine does what it needs.
- **Idempotent on an empty context.** Each method accepts a `NavigationContext` whose fields can be empty. This
  occurs for a shortcut very early in startup, or when no window has focus. Each engine then emits navigation
  feedback with a reason code, and does not fail.
- **Mutation stays engine-specific.** `IPlacementState` is read-only, with serialization. Mutation goes through
  engine-specific APIs such as `SnapState::assignWindowToZone` and `TilingState::addWindow`, because the meanings are
  different.
- **`PlacementEngineBase` is a thin shared base.** It gives engine-settings injection and a default prune hook that
  does nothing. Per-window placement state is in the state objects of the engines and in the unified
  `WindowPlacementStore`. The concrete engines implement the mode-specific intents.
- **The settings interfaces are here, not in the engine libraries.** `ISnapSettings` and `IAutotileSettings` are
  declared in the `PhosphorEngine` namespace, but the engine libraries ship them. Thus the daemon can give the same
  settings adaptor to the active engine with no engine-specific casts.

## Dependencies

- `Qt6::Core`
- [`phosphor-geometry`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-geometry/README.md) (public)
- `PhosphorProtocol::Types` (public)
- [`phosphor-identity`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-identity/README.md) (private)

## See also

- [`phosphor-snap-engine`](../phosphor-snap-engine/README.md): manual zone snapping. `SnapState` implements
  `IPlacementState`.
- [`phosphor-tile-engine`](../phosphor-tile-engine/README.md): autotile. `TilingState` implements `IPlacementState`.
