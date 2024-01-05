CHANGELOG for EDGE-Classic 1.38 (since EDGE-Classic 1.37)
====================================

New Features
------------


General Improvements/Changes
--------------------
- Optimized GL state caching and other rendering-related factors to improve performance with the current renderer
- Reduced CPU load when frame limiting (i.e., regular 35/70FPS modes)
- Removed voxel loader and rendering functions
- Removed SID playback library

Bugs fixed
----------
- Fixed sector fogwall check producing black 0% alpha fog walls for otherwise unfogged sectors