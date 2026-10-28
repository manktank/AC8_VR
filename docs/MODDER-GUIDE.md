# Modder guide

The [maintainer guide](../source_code/MAINTAINERS.md) describes current architecture, build outputs and runtime invariants; [configuration](CONFIGURATION.md) maps the modules and persistence files.

Most work in this source update reduces repeated Lua/native discovery, reflection lookups and stable work while retaining current visibility, geometry, animation and lifetime reads. Global object scans, pointer walks and layout-affine validation must be distinguished when measuring cost. Do not infer overall FPS gains from callback time alone.

Core remains required; the seven optional Lua files work in any selection. Keep feature-native admission tied to actual file presence and avoid adding cross-module scheduling requirements. The aircraft-preset owner is optional; removing it must not remove the general HUD controls.

No automatic profiling or capture system ships. Functional restoration journals, object lifetime checks, current-frame image ownership and queue/fence validation are not disposable debug code. A temporary investigation should state exactly what it records, how it is triggered, and how it will be removed.

The optional [offline shader utility](../tools/shader-rebuild/README.md) rebuilds portable bytecode, not driver caches. It does not repair incompatible native hooks, layer ownership or invalid current geometry. Use the matching source and assets together.
