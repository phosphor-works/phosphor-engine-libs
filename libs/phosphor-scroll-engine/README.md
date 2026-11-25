<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team
     SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-scroll-engine

> The scrolling placement engine, after niri. It implements `IPlacementEngine` for screens that run in scrolling
> mode: the windows are columns on a strip without an end, and the screen shows a part of the strip.

## Responsibility

The host sends each window event and shortcut through `PhosphorEngine::IPlacementEngine`, whatever the mode of the
screen. On a screen in scrolling mode, `ScrollEngine` decides where a new window goes, how focus moves, how columns
change width, and what float, maximize and fullscreen do.

The rule that the whole engine keeps: a new window adds a column and changes the size of no other column. Only the
view moves.

## The model

| Type | What it is |
| --- | --- |
| `ScrollStrip` | The pure model: the ordered columns, their widths and tiles, and the view offset. It uses Qt Core value types only, so its tests need no engine and no compositor |
| `StripAxis` | The mapping of one strip direction. The main axis is the strip, along which the view scrolls. The cross axis is the stack of tiles in a column. A strip runs horizontally or vertically |
| `ScrollState` | The state of one context (screen, virtual desktop, activity): one strip, and the windows floated out of it. `ScrollEngine` owns one for each context |
| `ScrollEngine` | The `IPlacementEngine` implementation: it turns events into strip operations and sends the geometry out in the `windowsTiled` contract that the other engines use |
| `ScrollStashTypes.h` | What the engine keeps when a screen leaves scrolling mode, so that the strip comes back in the same order, with the same focus and view |

A floated window leaves the strip and its column closes. An unfloat puts the window back where it was: the same
stack, column, width and display. Only when nothing of that place is left does the window get a new column beside
the focused one.

## What it reads

The engine owns none of these services:

- `PhosphorEngine::IWindowTrackingService`, the store of window state that all engines share.
- `PhosphorEngine::WindowRegistry` and the screen manager, for windows and work areas.
- `IScrollSettings`, the settings of scrolling mode. The host injects it. Its gap values are the shared tiling gaps,
  so scrolling and tiling screens use the same gaps.
- The per-screen overrides in `ScrollTypes.h` (`ScrollEngine::applyPerScreenConfig`): the strip axis of a screen
  and its other per-screen values. The host and the engine use the same keys from that header.

## Tests

`tests/` has the strip model's tests and the engine's behavior tests: boundaries, drag insert, evacuees, float,
fullscreen, grouping, maximize, persistence and per-screen settings. They use stub settings and a stub tracking
service, and need no display.
