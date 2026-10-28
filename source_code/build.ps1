param([switch]$SkipShaders)
$ErrorActionPreference='Stop'
$ac8SourceRoot=$PSScriptRoot
# The native module DLL is a payload, not the UEVR loader plugin.
. "$ac8SourceRoot/build-native.ps1" -DisableGlowReporting -SkipShaders:$SkipShaders
if($LASTEXITCODE -ne 0){throw 'Native payload build failed'}
$ac8PluginOutput=Join-Path $ac8SourceRoot 'build/package/plugins'
New-Item -ItemType Directory -Path $ac8PluginOutput -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $ac8SourceRoot 'build/release/AC8_VR-Runtime.dll') -Destination (Join-Path $ac8PluginOutput 'AC8_VR-Features.bin')
Push-Location -LiteralPath $ac8SourceRoot
try {
 & $compiler /nologo /std:c++17 /EHsc /W4 /WX /O2 /MT /LD src/renderer/portable-layer.cpp /Fo:build/portable-layer.obj "/Fe:$ac8PluginOutput/AC8_VR-OpenXRBridge.dll" /link /INCREMENTAL:NO /IMPLIB:build/openxr-bridge.lib user32.lib
 if($LASTEXITCODE -ne 0){throw 'Portable OpenXR bridge build failed'}
 & $compiler /nologo /std:c++17 /EHsc /W4 /WX /O2 /MT /LD loader/loader.cpp /Fo:build/loader.obj "/Fe:$ac8PluginOutput/AC8_VR-Runtime.dll" /link /INCREMENTAL:NO /IMPLIB:build/loader.lib
 if($LASTEXITCODE -ne 0){throw 'UEVR loader build failed'}
 $ac8LayerOutput=Join-Path $ac8PluginOutput 'AC8_VR-OpenXRLayer'
 New-Item -ItemType Directory -Path $ac8LayerOutput -Force | Out-Null
 Copy-Item -LiteralPath 'AC8_VR-OpenXRLayer/AC8_VR-Glow.json' -Destination $ac8LayerOutput
 $ac8AssetOutput=Join-Path $ac8PluginOutput 'ac8-hud-assets'
 New-Item -ItemType Directory -Path $ac8AssetOutput -Force | Out-Null
 foreach($ac8Asset in (Get-Content -LiteralPath 'runtime-assets.txt')){
  if($ac8Asset){Copy-Item -LiteralPath (Join-Path 'src/renderer/hud-route-assets' $ac8Asset) -Destination $ac8AssetOutput}
 }
 Write-Output "Built matching loader, payload, bridge, manifest and 44 assets: $ac8PluginOutput"
}finally{Pop-Location}
