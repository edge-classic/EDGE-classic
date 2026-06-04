# CHANGELOG for EDGE-Classic 1.53 (since EDGE-Classic 1.52)

## New Features


## General Improvements/Changes

- Removed Tracy profiler integration; retained basic rendering status (set debug_fps to '3' to view)
- Removed "EDGE_CLASSIC" CMake option; all features are unconditionally compiled and included
- Removed Direct3D11 Sokol target; Sokol builds are now either GLES3 (Emscripten) or Core GL 3.3 (other platforms)

## General Bugfixes

- Fixed dynamic light and floor/sector glow rendering artifacts where certain parameters conflicted with the "Max Dynamic Light Radius" performance setting
