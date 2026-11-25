<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-zone-rendering

Zone overlays drawn with shader packs. `ZoneShaderItem` is the Qt Quick item that renders a zone-overlay pack over
the zones of a layout, through PhosphorRendering. The overlays of the engine runtime draw with it. The previews of a
settings app and the pack tooling (`plasmazones-shader-render`, the pack validator) also use it.

## Responsibility

- Render one zone-overlay pack over a set of zones through the shader effect of PhosphorRendering. The inputs are
  the zone uniforms, the label texture, the audio spectrum and the pack's own parameters.
- Assemble an entry-only pack (one that defines `pZone` or `pImage` in place of `main()`) the same way on the live
  load and on the warm bake. Thus the bake cache keys agree.
- Resolve the trusted overlay roots, the include paths that `#include <common.glsl>` searches, and the vertex stage
  of a pack. One place does this for both paths.

## Key types

| Type | Header | What it is |
| --- | --- | --- |
| `ZoneShaderItem` | `ZoneShaderItem.h` | The Qt Quick item, in the `org.phosphor.zonerendering` QML module |
| `ZoneShaderRenderer` | `qml/ZoneShaderRenderer.qml` | The QML wrapper the overlay and the previews share: one config object in, consistent bindings out |
| `assembleZoneEntrySource` | `ZoneEntryScaffold.h` | The entry scaffold for packs without their own `main()` |
| `expandShaderIncludePaths`, `resolveZoneVertexPath`, `warmShaderBakeCacheForPaths` | `ZoneShaderNodeRhi.h` | Include paths, the vertex stage and the warm bake |
| `ZoneLabelTextureBuilder` | `ZoneLabelTextureBuilder.h` | The zone-number label texture |
| `setOverlayPackDataSubdir` | `OverlayPackLocation.h` | Where the app's overlay packs install, for the trusted roots |

## Use

Link `PhosphorZoneRendering::PhosphorZoneRenderingQml` and `PhosphorZoneRenderingQmlplugin`. This is a static
module, in this tree only. Import the plugin one time:

```cpp
Q_IMPORT_QML_PLUGIN(org_phosphor_zonerenderingPlugin)
```

Then use the types from QML:

```qml
import org.phosphor.zonerendering

ZoneShaderRenderer {
    config: overlayConfig
}
```

## Design notes

- **One source of truth for include paths and the vertex stage.** `ZoneShaderItem` and the warm bake must compile
  against identical inputs, or their bake-cache keys are different. Thus neither one derives them by hand.
- **The app names its overlay pack directory** with `setOverlayPackDataSubdir()` (`OverlayPackLocation.h`) at
  startup. For the engine runtime, it is `ConfigDefaults::overlaysSubdir()`. Until the app names it, no installed
  overlay root is trusted. The library is below the engine runtime and does not link it.
- **Logging** is under `phosphor.zonerendering`.

## Dependencies

| Dependency | Why |
| --- | --- |
| Qt Core, Gui, Qml and Quick | The Qt Quick item and the QML module |
| PhosphorRendering | The shader effect and RHI node |
| PhosphorShaders | Entry points |
| PhosphorZones | Zone JSON keys and the zone defaults |
