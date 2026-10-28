# Settings and optional modules

Core is required. The other seven files are independently optional: removing one with the game closed disables its work next launch. General HUD controls work without AircraftPresets; aircraft camera/size controls work without HUD Layout. HUD-preset controls naturally apply to the HUD feature when both are selected.

| Script | Responsibility |
|---|---|
| `AC8_VR-Core.lua` | Required shared services, pause/retry navigation, native tutorial card/blur policy and removed-feature recovery. |
| `AC8_VR-HUD_Layout.lua` | General HUD layout/group controls, linked ladder/FPM presentation, shared/global HUD defaults and first-launch UI setup. |
| `AC8_VR-AircraftPresets.lua` | Per-aircraft HUD, camera, UI Size and cockpit-size preferences; save/reset and master-enable controls. |
| `AC8_VR-HUD_Glow.lua` | Gameplay admission and widget policy for native per-eye glow. |
| `AC8_VR-Targets.lua` | World-target presentation, labels, material adjustments and indicator layout. |
| `AC8_VR-PlaneCamera.lua` | General camera-animation stabilization, independent of per-plane storage. |
| `AC8_VR-Loading.lua` | Mission-loading cover and transition handling. |
| `AC8_VR-Cinematics_Hangar_Presentation.lua` | Movie/hangar camera and UI ownership, framing and scoped hangar clipping. |

## Aircraft and global defaults

`data/AC8_VR-AircraftPresets.json` is the single aircraft data set. Entries can contain camera offsets, HUD layout, flight UI Size and cockpit-size/world-scale preferences. Unset fields use their default/fallback. Only a few aircraft are pre-tuned.

`data/AC8_VR-HUDDefaults.json` stores default/shared HUD layouts separately. **Save current HUD as global default** updates the fallback/shared layouts; **Save current camera as global default** changes the default position. Neither overwrites per-aircraft presets.

**Enable per-plane settings** disables all aircraft overrides without deleting data. **Use default HUD layout for all planes** selects the separate shared layout and does not overwrite custom aircraft layouts. The reset button reloads the layout selected by the current mode and its fallback.

Adjust UI Size in UEVR and save it for the plane, either with its separate save button or a HUD save. Flight UI Size includes pause/retry; other modes use the user's normal size. Cockpit size applies only to cockpit view and changes perceived scale, not headset FOV. Its save is included with cockpit-camera saves.

The FPM follows ladder movement/scale and retains native velocity motion. Separate FPM alignment controls provide an additional trim. The rejected cockpit FOV experiment and combined radar/wingman/mission mover are not included.

## First launch and runtime state

The editable `HUD_PRESENTATION` block in HUD Layout supplies initial distance 1m, size 1 and curvature 113.625 degrees. The generated `AC8_VR-HUDPresentationSetup.json` marker admits this once; later saved UEVR changes are respected. It must be absent from a fresh-install archive and retained for an existing user's profile.

Other supplied `AC8_VR-*Controls.json` files contain feature settings. Changes made directly on disk generally require a script/game restart; prefer the exposed controls. Recovery journals and baseline/cache files are generated for functional restoration and should not be copied into fresh packages. Back up the profile before changing files.

All scripts/data/plugin names use `AC8_VR-`; `ac8-hud-assets` and every file inside it are intentionally exempt. Standard UEVR config/folder names remain standard. Personal UEVR `cameras.txt` slots are separate and are not supplied.
