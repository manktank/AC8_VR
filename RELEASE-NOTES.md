# AC8-VR_MankTank_v0.3.4

This ZIP contains the complete AC8-VR profile for a clean install.

## What changed

- Fixed a HUD Glow freeze that could stop the picture mid-mission while audio continued.
- Restored the intended cinematic and hangar framing and the black loading cover across compatible UEVR builds using OpenXR.
- HUD Glow now clears old masks when the active HUD changes.

Thanks to @Zharay for diagnosing and contributing the freeze fix.

## Install

1. Close the game and back up your existing AceCombat8 profile, if you have one.
2. Move the old AceCombat8 folder out of %APPDATA%\UnrealVRMod, then extract AC8-VR_MankTank_v0.3.4.zip there. The ZIP includes the full profile and fresh default settings.
3. Use OpenXR and Native Stereo in UEVR, enable plugins, and inject again. See INSTALLATION.md for the full setup instructions.

Use the scripts included in this release if you previously installed temporary test scripts. Keep your backup if you want to restore your saved cockpit and HUD preferences later.

Restart the game and reinject UEVR after changing game or UEVR render resolution.
