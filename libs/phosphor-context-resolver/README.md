<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-context-resolver

A facade with frozen snapshots over the per-screen cascade of mode, desktop, activity, disable and lock. The daemon's
navigation, start and OSD paths and the three D-Bus adaptors (SnapAdaptor, WindowDragAdaptor, WindowTrackingAdaptor)
use one resolver call in place of their own cascade chains. Every daemon consumer, OverlayService included, goes
through the resolver. The KWin effect does not use this library, because the state of the effect has no disable or
lock cascade.

## Responsibility

A gate in the Phosphor stack asks five questions. What is the mode of the screen? What is the current virtual
desktop? What is the current activity? Does the disable list turn off that tuple? Does the lock store lock that
tuple? Each question reads a different state source. When a site asks them inline, a virtual-desktop switch during
the handler can separate the mode from the gate.

This library makes that cascade one primitive. `ContextHandle` freezes `(screenId, virtualDesktop, activity, mode)`
when it is constructed. The resolver answers gate questions against that frozen tuple, and does not read the
workspace again for each question. The cascade order (monitor > desktop > activity) and the
`desktop = 0 means "pinned across all desktops"` sentinel are the same as in the `contextDisabledReason` policy.

The library owns these items:

| Item | What it is |
| --- | --- |
| The handle value type | `ContextHandle` carries the frozen tuple between call sites with no new resolution. |
| The gate primitive | `IContextResolver` gives `disabledReason()`, `isLocked()`, `isDisabled()` and `isGated()`. Each gate question is one method call. |
| The adapter contracts | `IWorkspaceState`, `IModeProvider` and `IContextGateSource` are three small interfaces. A consumer connects them to its own state sources. The daemon has concrete adapters that forward to `VirtualDesktopManager`, `ActivityManager`, `ScreenModeRouter` and `ISettings`. |
| The concrete resolver | `ContextResolver` borrows three adapter pointers and composes the cascade. It owns no state. |

## Key types

| Type | Purpose |
|------|---------|
| `PhosphorContext::ContextHandle`         | Frozen `(screenId, virtualDesktop, activity, mode)` snapshot that a gate call receives. |
| `PhosphorContext::DisabledReason`        | Why a context is disabled: `MonitorDisabled`, `DesktopDisabled`, `ActivityDisabled`, or `NotDisabled`. |
| `PhosphorContext::IContextResolver`      | The facade: `handleFor()`, `globalHandle()`, `handleForMode()`, `handleForPersisted()`, `disabledReason()`, `isLocked()`, `isGated()`. |
| `PhosphorContext::ContextResolver`       | Concrete `IContextResolver` over the three adapter interfaces. |
| `PhosphorContext::IWorkspaceState`       | Adapter that tells which desktop and activity the user is on. |
| `PhosphorContext::IModeProvider`         | Adapter that tells the mode of a screen. |
| `PhosphorContext::IContextGateSource`    | Adapter that tells if a `(mode, screen, desktop, activity)` tuple is disabled or locked. |

## Use

```cpp
#include <PhosphorContext/ContextResolver.h>

using namespace PhosphorContext;

IContextResolver *resolver = /* injected — see ContextResolverWiring */;

// Snapshot once; every downstream gate sees the same tuple.
const ContextHandle ctx = resolver->handleFor(screenId);
if (resolver->isGated(ctx)) {
    return; // disabled or locked — silently no-op
}

// Override the mode axis when a caller already knows which mode to query
// (e.g. autotile-only shortcut handlers). The desktop/activity axes
// stay frozen at the original snapshot.
const ContextHandle autotileCtx =
    resolver->handleForMode(screenId, AssignmentEntry::Autotile);
const auto reason = resolver->disabledReason(autotileCtx);
```

## Design notes

- **Snapshot, not live reads.** `handleFor()` captures `(desktop, activity, mode)` one time. The handle then goes
  through every gate after it. The raw workspace readers (`currentVirtualDesktop()`, `currentActivity()`) read the
  workspace state again on each call, and their documentation says so. Use the field of the handle when the value
  must agree with an earlier snapshot.
- **Empty-screen contract.** `globalHandle()` returns a handle with an empty `screenId`. The mode comes from
  `IModeProvider::modeFor("")`, as the adapter contract documents. An empty `desktop` (`<= 0`) and an empty
  `activity` stop early inside the daemon's `IContextGateSource` adapter. Thus a handle with a sentinel axis never
  matches a disable entry for one axis. An empty `screenId` is handled before the adapter, by
  `ScreenIdentity::variantsFor`. It returns an empty list for the empty input, so `Settings::isMonitorDisabled`
  finds no candidate to match.
- **Adapters are non-owning.** Each interface implementation is connected with a raw pointer that the resolver does
  not own. The composition root manages the lifetime. The concrete `ContextResolver` calls `qFatal` on a null
  adapter at construction, so a wiring mistake is loud.
- **No mutation surface.** The resolver is read-only. A change to a disable list, a lock or the active mode goes
  through the original settings writer.
- **No engine routing.** `ScreenModeRouter::engineFor(screenId)` continues to dispatch placement engines. The
  resolver only uses `IModeProvider::modeFor()`, because the gate primitives are scoped to a mode.
- **LGPL boundary for GPL consumers.** The daemon, the D-Bus adaptors and the KWin effect are GPL-3. They link this
  library through standard LGPL→GPL linking. Because the resolver stays LGPL, a third-party plugin or headless tool
  can link it and not inherit the GPL.

## Dependencies

- `Qt6::Core`
- [`phosphor-zones`](../phosphor-zones/README.md): `AssignmentEntry::Mode` is the mode wire type that
  `ContextHandle` carries (public link).

The library uses no `Qt6::Gui`, no QObjects, no QML and no D-Bus.

## See also

- [`phosphor-rules`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-rules/README.md): rule
  evaluation. The resolver does not copy its priority math.
- [`phosphor-zones`](../phosphor-zones/README.md): owns `AssignmentEntry::Mode` and the wire vocabulary that the
  handle carries.
