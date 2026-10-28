# Compatibility

The development profile targets Windows x64, DirectX 12, OpenXR and UEVR Native Stereo. Headset testing was performed on Quest 3. Broader OpenXR connection support is implemented; that does not establish every headset/runtime combination as tested.

## Glow connection

The portable `AC8_VR-OpenXRBridge.dll` and `AC8_VR-OpenXRLayer/AC8_VR-Glow.json` join the current loader chain through the public OpenXR negotiation interface. Bootstrap changes are process-local; they do not register a system-wide layer. The glow connection has no private UEVR SHA/RVA fallback.

When active OFXR recognizes the current instance, the glow adapter obtains its complete application-facing dispatch through public loader negotiation. This keeps OFXR's private frame/image operations outside the game's glow transaction. Current and legacy OFXR layer identities are supported without build-number or binary-offset checks; an incomplete dispatch is not mixed with another provider. OFXR is optional and its DLLs are not bundled.

Connection tests passed with ordinary OpenXR and the actual OFXR V116 and v681 DLLs. Glow was accepted in game using OFXR; OFXR crash/stability investigation is deferred. This is not a claim that all future OFXR versions or their frame-generation modes have been validated.

## Remaining native boundaries

Other game-specific and camera/projection hooks still validate the supported game/backend signatures, PE metadata and reflected layouts. UEVR API-version compatibility alone is not sufficient for those hooks. Unsupported evidence leaves the affected enhancement inactive; retain the checks.

The renderer validates current scene/HUD geometry, surface formats and acquired/waited image ownership. The authored glow graph has fixed internal pass dimensions; the HUD input and current per-eye mapping are validated separately, not inferred from a display resolution or widget class name.

Automated validation covers 24 Lua suites, 128 optional-module combinations, 24 native fixtures, and actual OpenXR loader negotiation. These checks are distinct from headset appearance or performance measurements. The assembled fresh-install ZIP still needs a final headset launch before publication. No overall FPS improvement is claimed.
