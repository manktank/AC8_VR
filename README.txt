AC8 VR — Community profile
27 October 2026

FRESH INSTALL

This profile was designed for Native Stereo in UEVR. Use OpenXR for the glow effect.

1. Close the game. Back up your existing AceCombat8 profile first: copy the entire
   %APPDATA%\UnrealVRMod\AceCombat8 folder somewhere safe.
2. Move the old AceCombat8 folder out of %APPDATA%\UnrealVRMod after backing it up.
3. Extract this ZIP into %APPDATA%\UnrealVRMod so it contains the new AceCombat8
   folder. Use a clean install; do not mix in scripts or Lua files from older
   profiles or other mods. Keep the supplied folders and plugin/assets intact.
4. Launch the game, inject UEVR and use Native Stereo.

WHAT'S NEW

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

HUD AND AIRCRAFT SETTINGS

Enable Advanced settings in UEVR, then open ScriptUI. Use the grouped controls
to rearrange the HUD. Adjust the cockpit camera and UEVR UI Size, then use the
save buttons for the current plane. HUD saves for a plane include its UI Size;
there is also a separate Save UI Size for current plane button.

A few planes include Colin's preferred settings. Planes without a saved preset
use the global defaults. You can save your own defaults without overwriting
individual aircraft presets.

Uncheck Enable per-plane settings to turn all per-plane overrides off. Your
saved presets are retained, and the general HUD controls remain available.

OPTIONAL MODULES

Keep AC8_VR-Core.lua. The other seven Lua modules are optional and do not require
each other. With the game closed, move an optional Lua out of the scripts folder
to disable its feature, then restart. Keep the supplied plugins and assets.

Your backup is the easiest way to return to your previous profile.
