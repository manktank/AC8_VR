# Fresh installation

This is the unreleased development snapshot. The existing v0.1.0 release is unchanged.

Use UEVR **Native Stereo**, Windows x64 and DirectX 12. Glow uses OpenXR. UEVR and the **Bypass EAC mod** are separate prerequisites. EAC bypass setup is documented in the [Flat2VR Modding Discord](https://discord.gg/flat2vr); neither prerequisite is included here.

1. Close the game. **Back up the entire `%APPDATA%\UnrealVRMod\AceCombat8` folder** by copying it somewhere safe.
2. Move the old profile out of that location. Use a fresh AceCombat8 folder; do not merge old or third-party Lua scripts into this set.
3. Extract the community ZIP into `%APPDATA%\UnrealVRMod`. It should create `AceCombat8` containing `scripts`, `data`, `plugins`, `config.txt` and `cvars_data.txt`.
4. Launch the game with your EAC bypass, connect your headset, then inject UEVR with OpenXR and Native Stereo. Keep the supplied plugins, manifest and renderer assets together.

The repository root also contains the current runtime files; `PROFILE-FILES.txt` lists exactly which files belong in the profile. Source, development guides and build output do not go in UEVR's scripts directory.

## Controls

Enable Advanced settings in UEVR and open **ScriptUI**. Rearrange HUD groups and save layouts, cockpit camera position and UI Size for the current plane. The per-plane checkbox disables all aircraft overrides without deleting saved presets. General HUD controls remain available.

**Keep `AC8_VR-Core.lua`.** Any of the other seven Lua modules can be moved out of `scripts` with the game closed to disable that feature. Restart afterward; keep the plugin files and assets installed.

Initial UI distance, size and curvature are applied once for a fresh profile. Save later UEVR menu changes with **Save Config**. The generated `AC8_VR-HUDPresentationSetup.json` marker prevents repeated initial setup; retain it for an existing installation, but omit it from fresh-install packages.

To undo the installation, close the game and restore your complete backup. Keep your saved `AC8_VR-AircraftPresets.json` and `AC8_VR-HUDDefaults.json` backed up as you customize them.
