# Changelog

## Unreleased — development source update, 2026-10-27

- Many, many Lua and DLL optimizations reduce repeated discovery and lookups.
  This helps save many ms on callbacks and what not, but as far as overall FPS
  improvements I don't want to make any claims.
- Independent optional modules, which also makes customization easier. No more
  inter-Lua scheduling and coordination dependencies between optional modules.
  You can safely remove an optional module by removing its Lua. Keep
  AC8_VR-Core.lua; the other seven modules are optional.
- Fixed HUD Glow fx compatibility across OpenXR setups.
- Per-plane cockpit size adjustment, alongside camera position, HUD layout,
  and UI Size. (You can move the UEVR camera while in the cockpit using the
  controller if you hold right-trigger, or manipulate the values in the UEVR
  dialog. I usually try to line it up so the pilot's shoulders match roughly
  where mine are and then tweak from there.)
- Save global HUD and camera defaults without overwriting individual aircraft
  presets.
- Separate FPM alignment controls, while keeping it linked to ladder movement
  and scale.
- Presentation fixes: pause/retry input skipping, tutorial backgrounds,
  Aircraft Set/hangar cameras, and the occasional FPM flash.

Published v0.1.0 and its tag/release are unchanged. Fresh-install tester package validation is documented separately; release promotion awaits tester feedback.


## 0.1.0 — 2026-10-13

Initial packaged release candidate.

- Includes the cockpit HUD, target, camera, cinematic, loading and pause-navigation improvements listed in `features.txt`.
- Adds documented native/Lua source, installation and configuration guides.
- Removes public development captures/status reports and packages the 44 required runtime renderer assets.

Final headset validation of the cleaned build remains pending.
