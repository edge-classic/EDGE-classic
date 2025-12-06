# CHANGELOG for EDGE-Classic 1.52 (since EDGE-Classic 1.51)

## New Features
- New DDFTHING state: RESISTANCE_PAIN. Similar to WEAK_PAIN, a creature with a Resistance class assigned *MAY* go into this state when it is hurt by a corresponding attack class. RESISTANCE_PAINCHANCE controls the probability of going into this state (100% always, 0% never).
- New DDFTHING state: IMMUNITY_HIT. A creature with an IMMUNITY class assigned will *ALWAYS* go into this state when it is hit by a corresponding attack class.

## General Improvements/Changes
- Keyed doors on automap improved
  - shows correct key type (card or skull), with preference for card if either key will do. 
  - "Any Key" doors coloured purple.


## General Bugfixes
- Fixed certain sky rendering + level fog + draw culling combinations that prevented the sky from being drawn
- Fixed intermissions using a camera mobj in legacy GL not drawing the intermission statistics correctly
- Fixed detection of SSE on MSVC builds for PL_MPEG playback
- Fixed slight underapplication of friction in sectors with default friction