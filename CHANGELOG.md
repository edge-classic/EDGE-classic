# CHANGELOG for EDGE-Classic 1.52 (since EDGE-Classic 1.51)

## General Bugfixes
- Fixed certain sky rendering + level fog + draw culling combinations that prevented the sky from being drawn
- Fixed intermissions using a camera mobj in legacy GL not drawing the intermission statistics correctly
- Fixed detection of SSE on MSVC builds for PL_MPEG playback
- Fixed slight underapplication of friction in sectors with default friction