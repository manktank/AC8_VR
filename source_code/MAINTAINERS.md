# Maintainer guide

## Architecture

Core is required; seven sibling Lua features are independently optional. It registers section factories and supplies bounded shared services and callback phases. Optional features do not require another optional feature to be installed. Preserve this when adding code: no new cross-Lua deadlines, timing assumptions or mandatory sibling sequencing.

Lua owns verified object discovery, presentation intent, settings and HUD placement. Object access requires UObject lifetime membership; cached metadata does not make a destroyed/reused object valid. Shared context facts are lazy and scoped to the current callback phase. Reuse widget geometry only after validating its current affine/visibility/clipping/owner evidence.

`AircraftPresets` owns per-aircraft application and one JSON preference data set. Core retains generic recovery and shared HUD defaults; general HUD controls and camera stabilization remain useful without the preset owner. UI buttons queue their own operations onto an engine callback; they do not perform UObject operations in the UI draw callback.

## Building

From this directory run `build.ps1` using x64 MSVC C++ Build Tools and a Windows SDK with fxc. The script builds with C++17, `/O2 /W4 /WX /MT`; it includes vendored UEVR C headers. `-SkipShaders` retains supplied shader bytecode when source is unchanged.

`build-native.ps1` builds six native modules plus `src/plugin.cpp` into an intermediate payload. `build.ps1` places it at `build/package/plugins/AC8_VR-Features.bin`, builds `loader/loader.cpp` as `AC8_VR-Runtime.dll`, and builds `portable-layer.cpp` as `AC8_VR-OpenXRBridge.dll`. It copies the relative manifest and the 44 assets in `runtime-assets.txt` into that output tree. Never substitute the intermediate native DLL for the thin loader or load the payload a second time as a DLL plugin.

| Native source | Responsibility |
|---|---|
| `src/plugin.cpp`, `src/module-selection.h` | Select native features from actual Lua-file presence; absent modules register no feature work. |
| `src/cinematic/cinematic-game-hooks.cpp` | Scoped camera/projection, pause/retry, loading and target/bomb presentation support. |
| `src/hud/world-targets.cpp` | Reflected world/escort billboard layout. |
| `src/renderer/hud-renderer.cpp` | D3D12 HUD observation, authored glow generation and current per-eye composition. |
| `src/hud/hud-tick-guards.cpp` | Suppress only verified duplicate viewport updates. |
| `src/hud/offscreen-indicators.cpp` | Scoped native indicator/text corrections. |
| `src/hud/target-lock-reticle.cpp` | Guarded seeker artwork sizing. |

## Runtime invariants

- Preserve module removal, including recovery of any temporarily owned UEVR setting. No resolution, headset model or GPU vendor may be a required assumption.
- Presentation saves/restores settings transactionally; retain recovery state until restoration succeeds. Aircraft settings defer to presentation ownership. Outside their admitted scopes, restore ordinary values.
- Selected first-person views use absolute saved camera offsets. Cockpit perceived-size/world-scale changes are cockpit-only; no headset FOV modification. Keep manual tuning between actual scope/aircraft changes.
- UI Size uses the selected plane in flight, including pause/retry; non-flight modes use the user's normal value. Master-off retains every preset.
- Tutorial uses its native card fill, with the extra rectangle absent and tutorial blur disabled. Pause/retry advance once per real input.
- Keep ladder/FPM presentation linkage and independent alignment trim separate from native flight-vector motion and world/bomb projection math.
- Loading admission follows current live ownership, not just a surviving flight HUD or persistent icon. Do not replace it with fixed delays.
- Native metrics/materials restore after scoped changes; never compound previous applied scale or broaden corrections to unrelated objects/pixels.
- Geometry and image-write permission are separate. Require current-frame acquired/waited tokens, matching session/generation, valid layers/subrects and queue/fence ownership. Retain resources on failure/reset; never modify released images.
- The portable glow layer observes the current loader chain. If OFXR is active, select its complete public application-facing dispatch; do not conflate its private presentation calls with the game's transaction. No partial provider mix or private offset fallback.
- Other native game hooks keep their binary/reflection guards. Unknown/mixed/stale evidence forwards safely.

## State, source and validation

Player builds contain no capture hotkeys, profiling collectors or diagnostic dashboards. Functional error handling, ownership/state validation, restoration journals, caches and control flags remain. Record any temporary diagnostic added during development and remove it before packaging. Private tests/captures stay outside public exports; inert compile-time fixture seams may remain.

Preserve inherited/community and vendor attribution. Do not publish personal paths, game archives, saves, keys/extraction tools, logs or backups. Keep all custom runtime names prefixed `AC8_VR-`, except `ac8-hud-assets` and its contents.

The private validation run passed 24 Lua suites, all 128 optional-module combinations, 24 native fixtures and real OpenXR loader tests with no OFXR, V116 and v681. User headset checks accepted the feature batches and corrected glow. Fresh-install ZIP headset testing and OFXR stability are separate follow-ups; no large FPS claim is supported.

## Release workflow

Current main is an unreleased source snapshot. `VERSION` and tag v0.1.0 retain the last published version until the owner chooses the next release. Do not alter an existing release, tag or archive as part of a source-only update. Before the next release, align the selected version, changelog and public artifact names.

Build a fresh profile from `PROFILE-FILES.txt`, with no prior Lua set. Omit first-setup markers, backup JSONs, caches, recovery journals, captures, logs and personal cameras.txt; include all 44 assets. Preserve existing users' setup marker/settings when testing or upgrading. Test fresh launch, module removal, plane switches/saves/fallback/master-off, HUD movement/FPM, pause/retry/tutorial, loading, hangar/Aircraft Set, glow and mission exit before publication.
