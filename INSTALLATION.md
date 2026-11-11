# Install AC8-VR_MankTank_v0.3.3

**Requires UEVR, OpenXR and Native Stereo.** Use Windows x64, DirectX 12, a working OpenXR runtime and the game's single-player EAC bypass setup. UEVR and the EAC bypass are separate prerequisites. See [UEVR](https://uevr.io/) and the [Flat2VR Modding Discord](https://discord.gg/flat2vr).

1. Close the game. Back up your entire `%APPDATA%\UnrealVRMod\AceCombat8` profile, then move the old folder out of `UnrealVRMod`.
2. Extract `AC8-VR_MankTank_v0.3.3.zip` into `%APPDATA%\UnrealVRMod`. The ZIP creates an `AceCombat8` folder with `scripts`, `data`, `plugins`, `config.txt` and `cvars_data.txt`.
3. Connect your headset and start its PC VR runtime. Launch the game with your EAC bypass setup. Open UEVR, select `AceCombat8.exe`, choose **OpenXR**, enable plugins, and inject. Use **Native Stereo**.
4. Enter a mission and check the HUD. Open **Script UI** for layout and aircraft saves. See the [FAQ](FAQ.md) for the controls.

Use the supplied profile as a set. The [file manifest](PROFILE-FILES.txt) lists the runtime files for a manual install from source. Keep the loader, feature payload, OpenXR bridge, manifest and shader assets together in the profile. Keep `AC8_VR-Core.lua` enabled. The seven other scripts are optional.

**Restart the game and inject again after changing game or UEVR render resolution.** Glow and other rendering features can fail after a live resolution change.

When upgrading, keep your old profile backup and carry over your own compatible `AC8_VR-AircraftPresets.json` and `AC8_VR-HUDDefaults.json` preferences as needed. Older HUD layouts may use a different coordinate format and need to be adjusted and saved again. To undo the install, close the game and restore your backed-up profile.
