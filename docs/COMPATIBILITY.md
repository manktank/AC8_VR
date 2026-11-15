# Compatibility

The development profile targets Windows x64, DirectX 12, OpenXR and UEVR Native Stereo. Headset testing was performed on Quest 3. Broader OpenXR connection support is implemented; that does not establish every headset/runtime combination as tested.

## Glow connection

The portable `AC8_VR-OpenXRBridge.dll` and `AC8_VR-OpenXRLayer/AC8_VR-Glow.json` join the current loader chain through the public OpenXR negotiation interface. Bootstrap changes are process-local; they do not register a system-wide layer. The glow connection has no private UEVR SHA/RVA fallback.

When active OFXR recognizes the current instance, the glow adapter obtains its complete application-facing dispatch through public loader negotiation. This keeps OFXR's private frame/image operations outside the game's glow transaction. Current and legacy OFXR layer identities are supported without build-number or binary-offset checks; an incomplete dispatch is not mixed with another provider. OFXR is optional and its DLLs are not bundled.

Ordinary OpenXR with OFXR disabled completed the latest in-game test. OFXR v681 froze during mission entry on the tested VDXR setup with both TSR and DLSS. OFXR troubleshooting is deferred for this build.

## Remaining native boundaries

Other game-specific and camera/projection hooks still validate the supported game/backend signatures, PE metadata and reflected layouts. UEVR API-version compatibility alone is not sufficient for those hooks. Unsupported evidence leaves the affected enhancement inactive; retain the checks.

The renderer validates current scene/HUD geometry, surface formats and acquired/waited image ownership. The authored glow graph has fixed internal pass dimensions; the HUD input and current per-eye mapping are validated separately, not inferred from a display resolution or widget class name.

Automated validation covers 24 Lua suites, 128 optional-module combinations, 24 native fixtures, and actual OpenXR loader negotiation. These checks are distinct from headset appearance or performance measurements. The assembled fresh-install ZIP still needs a final headset launch before release. No overall FPS improvement is claimed.

After changing game resolution or UEVR render resolution, restart the game and inject again. Live changes can invalidate the glow and other rendering resources for that session.

## Crash and freeze diagnostics

A crash or freeze alone does not establish that this profile caused it. Game code, UEVR, GPU drivers, OpenXR runtimes and other layers can fail in the same process. In particular, OFXR v681 froze during mission entry on the tested VDXR setup; test with OFXR disabled first.

Builds before v0.3.4 could freeze (picture stops, audio continues) when HUD glow discovery ran while another thread delivered a UEVR Lua event, for example the CheekyFoveatedDLSS plugin during present or the UEVR menu. Native replies to Lua requests are now delivered on a later engine tick.

### Logs

Install the updated Core script and the matching rebuilt native package, then restart the game. The feature payload writes these files in the **active UEVR game-profile root** (the folder containing `scripts`, `plugins` and `data`):

- `AC8_VR-Runtime.log`: current session/tail.
- `AC8_VR-Runtime.previous.log`: previous session or previous size segment.

Logging is automatic and local; nothing is uploaded. Each file is capped at 8 MiB. Starting another game session rotates the current file, and reaching the size cap also rotates it. **Copy both files immediately after a failure, before relaunching.** Files can be copied while the game is frozen.

Records include UTC time, uptime in milliseconds, process/thread IDs, native build time, UEVR commit, admitted modules, native/Lua initialization, Lua callback error tracebacks, changed runtime errors, glow lifecycle and OpenXR connection status, renderer state/fence values and device-removal HRESULTs. Lua and renderer heartbeats appear about every five seconds when their callbacks are running. Repeated identical callback errors are limited to one report per callback every five seconds, with occurrence counts since the previous report. Logs intentionally do not enumerate every failed reflection probe or record every native hook.

Writes go directly to Windows rather than a buffered Lua/C stream, so already-written records survive an ordinary game-process crash or termination. This is not a power-loss guarantee. Logging failures are reported through UEVR's normal logger and the Windows debug output; retain UEVR's own log too. If the payload never loads, no new profile log will appear: check its session timestamp and UEVR's log for loader/injection errors. Lua startup/errors also go to UEVR's Lua output if the native logger is unavailable.

For a reproducible freeze with no useful normal-log error:

1. Close the game.
2. Create an empty `data/AC8_VR-TraceCallbacks.flag` in the active profile.
3. Relaunch, reproduce briefly, and copy both log files.
4. Close the game and remove the flag before the next normal run.

This adds begin/end records for Core-dispatched Lua callbacks and the native glow renderer's engine-tick, present, frame-composition and device-reset callbacks. It is intentionally expensive and may change timing or rotate logs quickly. It does not instrument every game hook, host call or OpenXR runtime call. A missing end record is a lead, not proof: other threads may keep writing, and process termination can interrupt a callback anywhere.

### Freeze dumps and crash reports

For a freeze, leave the game running and use **Task Manager > Details > AceCombat8.exe > Create memory dump file** (wording varies by Windows version). Wait for completion and retain the displayed dump path before ending the game. If practical, collect a second dump about 10 seconds later; unchanged thread stacks help distinguish a deadlock/wait from slow work. Logs alone cannot reconstruct blocked threads.

For a crash, retain any game/UEVR crash report and dump. Also record the Windows Event Viewer **Windows Logs > Application** error's faulting module and exception code. This project does not replace the game's crash handler or automatically generate dumps. Native dumps need the **exact matching build's PDB symbols** for useful source-level analysis; see [maintainer build notes](../source_code/MAINTAINERS.md#building).

Include the game and UEVR versions, OpenXR runtime, headset/GPU/driver, OFXR/other layers, enabled scripts, mission/menu and last action, approximate time to failure, and whether it is a freeze or an exit. Review logs before sharing. Dumps can contain paths, account information and other process memory: keep them private and do not post them in public issues.

### Narrowing the cause

Back up the profile. Reproduce the same mission/actions with OFXR and other optional OpenXR layers disabled. Next test without HUD Glow by moving `AC8_VR-HUD_Glow.lua` completely outside `scripts` **while the game is closed**, then relaunching; merely unchecking a Lua script does not remove its startup-selected native hooks. Test other optional scripts one at a time the same way, keeping Core. Compare with plain UEVR and no AC8 profile scripts/plugins as a separate clean-profile test, preserving the original profile and settings. A repeatable difference narrows the suspect path; analyze logs/dumps before attributing the crash.
