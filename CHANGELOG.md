# CHANGELOG for EDGE-Classic 1.52 (since EDGE-Classic 1.51)

## New Features
- New DDFTHING state: RESISTANCE_PAIN. Used simlarly to WEAK_PAIN, a creature with a Resistance class assigned will go into this state when it is hurt by a corresponding attack class.


## General Improvements/Changes
- Keyed doors on automap improved
  - shows correct key type (card or skull), with preference for card if either key will do. 
  - "Any Key" doors coloured purple.


## General Bugfixes
- Fixed certain sky rendering + level fog + draw culling combinations that prevented the sky from being drawn
- Fixed intermissions using a camera mobj in legacy GL not drawing the intermission statistics correctly
- Fixed detection of SSE on MSVC builds for PL_MPEG playback
- Fixed slight underapplication of friction in sectors with default friction