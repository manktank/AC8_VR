# AC8 VR FAQ

### What do I need?

**UEVR with OpenXR and Native Stereo**, a working OpenXR runtime, and Ace Combat 8 on Windows with DirectX 12. See the [installation guide](INSTALLATION.md) for prerequisites and setup. SteamVR can serve as your OpenXR runtime. Choose **OpenXR** in UEVR.

### Which scripts can I disable?

**Keep `AC8_VR-Core.lua` enabled.** Cinematics and Hangar Presentation, HUD Layout, HUD Glow, Aircraft Presets, Loading, Plane Camera and Targets are independently optional.

- For a quick Lua-only toggle, open **Lua → Main → Known scripts** in UEVR and uncheck the optional script.
- For a full disable, close the game, move its `.lua` file out of the profile's `scripts` folder, and relaunch. Native features check which files are on disk at startup. Move the file back before a later launch to re-enable it.

Keep the supplied plugin files and assets together. For per-plane HUD saves, keep both **HUD Layout** and **Aircraft Presets**. For per-plane cockpit camera saves, keep **Aircraft Presets**. [All modules and what they do](docs/CONFIGURATION.md)

### Glow disappeared after I changed resolution. How do I get it back?

**Restart the game and inject UEVR again** after changing the game resolution or UEVR render resolution. Glow and other rendering features can fail after a resolution change until the game restarts. Reloading Lua scripts alone won't rebuild those resources.

### How do I save a HUD layout for one plane?

Fly that aircraft in cockpit view. In **UEVR → Script UI**, adjust the grouped HUD sliders. In **Aircraft presets**, enable **Enable per-plane settings**, turn off **Use default HUD layout for all planes**, then click **Save HUD layout for current plane**. The HUD save includes UEVR's current **UI Size**.

To use a common layout, use **Save HUD layout for ALL planes** and enable **Use default HUD layout for all planes**. Use **Save current HUD as global default** to update the fallback and shared layout.

### How do I save where I sit in the cockpit?

While in the cockpit, adjust the camera offsets in UEVR. You can also open the UEVR menu, hold the right trigger on an Xbox-style controller, and use the sticks to move the camera. In **Script UI → Aircraft presets → Camera and Combined Saves**, click **Save cockpit camera for current plane**. Use **Save HUD + cockpit camera for current plane** if you adjusted both. **Save current camera as global default** sets the fallback for other planes.

### Do I need OFXR Bridge?

OFXR Bridge is optional. Use the normal OpenXR setup for this build. If you encounter a freeze with OFXR enabled, fully close the game, disable OFXR, then retry.
