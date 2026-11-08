# AC8-VR_MankTank_v0.3.2

**UEVR and OpenXR are required. Select OpenXR in the UEVR injector and use Native Stereo.** If you use SteamVR, set it as your active OpenXR runtime, then still choose OpenXR in UEVR.

AC8 VR is a community profile for playing Ace Combat 8 in VR. This build includes the optimized runtime and the latest cockpit HUD controls.

[Install the profile](INSTALLATION.md) · [FAQ](FAQ.md) · [Features](features.txt) · [Compatibility](docs/COMPATIBILITY.md)

## What you get

- A cockpit HUD you can move and resize in groups, including the ladder, radar, mission information and wingman status.
- Separate HUD layouts, cockpit camera positions and UI sizes for each aircraft, plus global defaults.
- HUD glow in flight, clearer world target labels and smaller off-screen indicators.
- Better framing in cinematics and the hangar, cleaner loading transitions, readable tutorial cards, and pause/retry navigation that advances one step per press.
- Optional modules so you can choose which enhancements to use.

## Set up your cockpit

1. Enter a mission in **cockpit view**. Open the UEVR menu, enable **Advanced Mode**, and open **Script UI**.
2. Under **Modify position of Grouped HUD Elements**, use the X, Y and scale controls for the groups you want to change. Under **Aircraft presets**, enable **Enable per-plane settings**, turn off **Use default HUD layout for all planes**, and click **Save HUD layout for current plane**. That save also records the current UEVR **UI Size**.
3. Adjust UEVR's camera offsets while in the cockpit. Under **Camera and Combined Saves**, click **Save cockpit camera for current plane**, or **Save HUD + cockpit camera for current plane** to save both. Cockpit size is also saved with the camera.

Use the global default buttons to set a fallback for other aircraft. Your individual presets stay saved. See the [configuration guide](docs/CONFIGURATION.md) for the module list and save options.

## Choose your modules

**Keep `AC8_VR-Core.lua` enabled.** The other seven scripts are optional. In UEVR's **Lua → Main → Known scripts**, uncheck an optional script to stop its Lua behavior. To disable the entire module, including any native work it enables, close the game, move that Lua file out of the profile's `scripts` folder and relaunch. For example, move `AC8_VR-Cinematics_Hangar_Presentation.lua` to turn off the cinematic and hangar changes.

## Restart after changing resolution

**After changing the game's resolution or UEVR's render resolution, close and relaunch the game, then inject again.** Glow and other rendering features can stop working until the game restarts.

[Source and build guide](https://github.com/manktank/AC8_VR/tree/main/source_code) · [Credits and notices](THIRD_PARTY_NOTICES.md)
