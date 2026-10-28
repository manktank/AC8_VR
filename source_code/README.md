# AC8 VR source

See [MAINTAINERS.md](MAINTAINERS.md) for architecture and invariants. `modules.json` lists the six translation units in the feature payload. `src/plugin.cpp` selects native work from the installed optional Lua files.

Build with Visual Studio C++ Build Tools and a Windows SDK:

```powershell
.\build.ps1
```

Use `-SkipShaders` when shader sources are unchanged and you are retaining the supplied bytecode. Output is `build/package/plugins`, containing the thin UEVR loader, feature payload, portable OpenXR bridge, manifest and 44 runtime assets. Build output is ignored by Git. Players do not need to compile anything.

The low-level `build-native.ps1` creates an intermediate module DLL. **Do not install that intermediate as `AC8_VR-Runtime.dll`**: the wrapper places it in `AC8_VR-Features.bin` and separately builds the real loader.

Lua and release defaults live in `../scripts` and `../data`. Runtime assets are in `src/renderer/hud-route-assets`; `runtime-assets.txt` selects the 44 shipping inputs. Preserve the vendor notices under `vendor/uevr`. Private tests, captures, extraction tools and generated state are excluded.
