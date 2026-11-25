<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-engine-runtime

The placement engine runtime that a host embeds: the host of the snap, tile and scroll engines, its services, its
overlays and its settings. The PlasmaZones daemon (`plasmazonesd`) hosts it beside KWin.

## Responsibility

- The settings schema that declares every engine setting (keys, defaults, types, coercion, choices). Also the
  versioned migrations that bring an older config file up to that schema.
- `Settings`, the store-backed model that the runtime reads and writes. It has a NOTIFY for each key, and announces
  changes on reload, discard and reset.
- The host settings extension. Through it, a host adds its own keys to the same file, names the keys that belong to
  one machine, and labels its token keys.
- The name of the host's config directory. Every config and data path is composed from it.
- The core:
  - the overlay interface that the runtime presents through (`IOverlayService`)
  - the screen-mode router, and the cross-surface and geometry resolvers
  - the geometry and layout-list helpers
  - the zone-selector and strip-selector layouts
  - the animation profile bootstrap and the zone-overlay shader registry
  - the logging categories of the runtime and its translation catalog (`RuntimeI18n`, context
    `PhosphorEngineRuntime`)
- Window tracking (`WindowTracker`): the placement of every managed window, its float state for each mode,
  persistence across restarts, evacuation when an output leaves, and the rules applied as windows open.
- The drag resolver (`DragResolver`): what a drag with a trigger held does, from zone highlighting to the drop, and
  the drop proxy.
- The services that a host binds to its transport (plasmazonesd binds them to D-Bus): layouts, settings, snapping,
  tiling, autotile, scrolling, rules, shaders, overlays, zone queries and the convenience control API.
- The host interfaces that the runtime asks the host through:
  - `IWorkspaceHost`: the desktops of each output and the desktop shortcuts of the compositor
  - `ISystemOsd`: the text OSD of the desktop
  - `IHostApps`: the settings app and the layout editor of the host
- The overlays (`OverlayService`): the zone overlay, the zone selector, snap assist, the layout picker, the OSDs, the
  cheatsheet and the scroll drop indicator. They are slots of one passive layer-shell surface for each screen, with
  their QML (`qrc:/phosphor-engine-runtime/overlay/`). The host presents them through a layer-shell transport and an
  `ISurfacePresentationHost` (the render backend and any GPU thumbnail import). Their layer-shell namespaces are the
  runtime's own, `phosphor-<family>` (`phosphor-osd`), under every host.
- The hub, `EngineRuntime`. It builds the settings, screens, engines, tracker, services, overlays and controllers. It
  sends each window to the engine that its screen runs, and connects all the parts.
- The controllers:
  - global shortcuts (`ShortcutManager`, over the PhosphorShortcuts backend of the host)
  - dynamic per-monitor workspaces (`WorkspaceController`)
  - the unified layout list and mode toggle (`UnifiedLayoutController`)
  - the construction of the engines (`createEngines`) and the adapters of the context resolver

## Key types

| Type | Header | What it is |
| --- | --- | --- |
| `Settings` | `Settings.h` | The store-backed settings model (`ISettings` and the engines' settings interfaces) |
| `ISettings` | `ISettings.h` | The interface the runtime and its services read settings through |
| `ConfigDefaults` | `ConfigDefaults.h` | Default values and the config group and key accessors |
| `HostSettingsExtension` | `SettingsSchema.h` | A host's own keys, machine-scoped keys and labels |
| `ConfigMigration` | `ConfigMigration.h` | The schema-version migration chain run at startup |
| `PerScreenResolver` | `PerScreenResolver.h` | Per-screen overrides resolved over the global values |
| `IOverlayService` | `IOverlayService.h` | The overlays, zone selector, snap assist, OSDs and pickers the runtime shows |
| `ScreenModeRouter` | `ScreenModeRouter.h` | Which placement mode each screen runs, per desktop and activity |
| `AnimationBootstrap` | `AnimationBootstrap.h` | Loads the curve and motion-profile registries the overlays animate with |
| `ShaderRegistry` | `ShaderRegistry.h` | The zone-overlay shader packs |
| `RuntimeI18n` | `RuntimeI18n.h` | The runtime's translation context and `loadRuntimeTranslations` |
| `WindowTracker` | `WindowTracker.h` | Window placement tracking, float state, persistence and evacuation |
| `DragResolver` | `DragResolver.h` | A drag's activation, zone highlight and drop |
| `WindowLifecycleRelay` | `WindowLifecycleRelay.h` | The window lifecycle signals the services listen to |
| `LayoutService`, `SettingsService`, `SnapService`, ... | `<Name>Service.h` | The services a host binds to its transport |
| `IWorkspaceHost`, `ISystemOsd`, `IHostApps`, `IShellCheatsheet`, `IIdleSource` | `IWorkspaceHost.h`, `ISystemOsd.h`, `IHostApps.h`, `IShellCheatsheet.h`, `IIdleSource.h` | What the runtime needs from its host |
| `EngineRuntime` | `EngineRuntime.h` | The hub a host constructs and starts |
| `OverlayService` | `OverlayService.h` | The overlays, their surfaces, animation and QML |
| `ISurfacePresentationHost` | `ISurfacePresentationHost.h` | The host's render backend and GPU thumbnail import |
| `ShortcutManager` | `ShortcutManager.h` | The runtime's global shortcuts and the cheatsheet model |
| `WorkspaceController` | `WorkspaceController.h` | Dynamic per-monitor workspaces |
| `UnifiedLayoutController` | `UnifiedLayoutController.h` | The unified layout list and the mode toggle |

## Use

```cpp
// At the top of main(), before anything reads a path or builds a Settings.
PhosphorPaths::setAppDirectoryName(QStringLiteral("myhost"));
PhosphorEngineRuntime::setHostSettingsExtension({&appendMyHostSchema, &isMyMachineScopedKey, &myHostLabels});

PhosphorEngineRuntime::Settings settings;
settings.setWorkspacesEnabled(true);
settings.save();
```

The host's own keys go through `Settings::hostValue` and `setHostValue`. `hostValueChanged` announces each change,
whatever its cause. PlasmaZones serves its KWin choices and its rendering pin that way (`HostSettings`).

To start the runtime, a host follows these steps:

1. Construct `EngineRuntime` with an `EngineRuntime::Platform` (the layer-shell transport, panels, desktops,
   activities, shortcut backends, surface presentation and idle source).
2. Give it the host interfaces.
3. Bind the services to the host's transport.
4. Call `init()`.
5. Call `start()`.

## Design notes

- **Everything is under `phosphor-works`.** The data that the apps share is in common trees in each XDG data root:
  `phosphor-works/layouts`, `/algorithms`, `/scrolling-templates`, `/curves`, `/profiles`, and the motion,
  decoration and overlay sets. Every shader family is under `phosphor-works/shaders/` (`animations`, `overlays`,
  `pointer`, `presets`, and the surface packs in `surface`, which the host names). The runtime's translations are in
  `share/phosphor-works/translations`. What belongs to one app is in its folder, `phosphor-works/<app>`: config
  files (`~/.config/phosphor-works/<app>/config.json`), state (workspaces) and cache.
- **No default directory name.** If code reads the app directory name before a host sets it, the process aborts. It
  does not give out a path that every app shares.
- **The 3.x config and data move one time.** The first start that finds no `config.json` in the app's folder moves
  the 3.x `<config root>/<app>/` and the user's data trees under `<data root>/<app>/` into the new places. The
  migration chain then brings the config up to date. The runtime does not read the old places again.
- **Install the extension first.** A store built without the extension deletes the host's keys as undeclared on its
  next save. Thus the host installs the extension before the first `Settings`.
- **Logging** is under `phosphor.engineruntime.*`: `.config` for the model, `.core`, `.daemon` and its `.overlay` and
  `.shortcuts`, and `.dbus.*` for the services, by the interface that each one serves.

## Dependencies

| Dependency | Why |
| --- | --- |
| Qt Core, Gui, Qml and Quick | The runtime and its overlays |
| PhosphorPaths | Where the config and data live, named by the host's `PhosphorPaths::setAppDirectoryName` |
| PhosphorConfig | The store |
| PhosphorAnimation | Motion profiles |
| PhosphorSurface and PhosphorRegistry | Decoration and overlay trees |
| PhosphorRules | Per-mode disable rules |
| PhosphorZones, PhosphorScreens, PhosphorEngine and the three engines' settings interfaces, PhosphorTiles, PhosphorShaders, PhosphorCompositor, PhosphorIdentity, PhosphorLayoutApi and PhosphorProtocol | Their types are in the settings |
| PhosphorGeometry, PhosphorPlacement, PhosphorWorkspaces and PhosphorAudio | The core |
| PhosphorContextResolver and PhosphorShortcuts | Tracking, drag and the shortcuts |
| PhosphorLayer, PhosphorShellPatterns, PhosphorOverlay, and privately PhosphorSurfaces, PhosphorRendering and PhosphorZoneRendering | The overlays |
| PhosphorSurfaceQuick | The shader warm-up of the hub |

At run time, the overlay QML also imports org.phosphor.zonerendering, org.phosphor.animation, org.phosphor.surface,
Phosphor.Components (phosphor-components) and Kirigami. The host process provides them.
