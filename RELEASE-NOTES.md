# AC8-VR_MankTank_v0.3.3

This is a complete, standalone release for a clean install. You do not need an older version installed first.

## What changed

- Fixed a freeze that could occur when entering a mission with HUD glow enabled. In the reported case, the picture stopped updating while the game audio continued.
- The freeze came from two parts of the rendering system waiting on each other. This update removes that conflict so the game can continue drawing frames.
- The player who reported the freeze confirmed that the v0.3.3 candidate works on their setup.

## Install

1. Close the game and back up your existing AceCombat8 profile, if you have one.
2. Move the old AceCombat8 folder out of %APPDATA%\UnrealVRMod, then extract AC8-VR_MankTank_v0.3.3.zip there. The ZIP includes the full profile and fresh default settings.
3. Use OpenXR and Native Stereo in UEVR, enable plugins, and inject again. See INSTALLATION.md for the full setup instructions.

Use the scripts included in this release if you previously installed temporary test scripts. Keep your backup if you want to restore your saved cockpit and HUD preferences later.

This fix was confirmed for the reported freeze; it does not establish compatibility with every headset, runtime, or graphics card.
