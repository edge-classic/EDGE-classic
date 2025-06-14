# CHANGELOG for EDGE-Classic 1.5 RC2 (since EDGE-Classic 1.5 RC1)

## New Features

- Added FLIP and INVERT DDFIMAGE specials to allow easier creation of mirrored images, etc

## General Bugfixes

- Moved bot catch-up spawn closer to the player to prevent getting stuck in level geometry
- Fixed CTD when using the 'endoom' console command with no valid ENDOOM lump present
- Fixed HUD messages with newline characters not formatting properly
- Removed interpolation when performing the RTS MOVE_SECTOR action to prevent visual artifacts
- Fixed plane movers using the sector's current floor/ceiling height as targets not updating their destination heights properly
- Fixed CTD when loading MDL format models in the Sokol renderer (MD2/3s were not affected)
- Fixed rendering height of things when active portals were in view (mirrors were not affected)

# CHANGELOG for EDGE-Classic 1.5 RC1 (since EDGE-Classic 1.38)

## New Features

- Uncapped framerate
  - 'framerate_limit' CVAR can be used to cap framerate maximum; default 500
- New Sokol GFX Renderer
  - Leverages platform-specific APIs (Direct3D 11 on Windows, WebGL2 on Emscripten, and GL Core 3.3 on other platforms)
    - Original GL 1.3 render path requires recompilation; see COMPILING.md for details
  - Multithreaded BSP traversal/geometry creation (with new renderer)
- Improved sound mixer
  - Provides threading, floating point samples, and OpenAL attenuation/spatialization models
  - Improved Freeverb-based reverb model used and exposed to modders via DDFVERB
  - Underwater/reverb/vacuum effect nodes remove the need to cache premixed sounds
- Dynamic light image-based coloring via AUTOCOLOUR DDF field
  - Will use the average RGB value of the specified image as the light color
- Implement "extra_light_step" CVAR
  - Controls how much the light level is raised by actions such as A_Light1/2
- New state action: LUA_RUN_SCRIPT("\<lua_script_name\>")
  - Will pass a table containing the calling mobj's information as a parameter
- New states actions:  CLEAR_TARGET and FRIEND_LOOKOUT
- New BORE attack flag
  - Can damage the same mobj multiple times as it is passing through, versus the once-per-mobj of the existing TUNNEL special
- New states actions: GRAVITY and NO_GRAVITY
- New state action: SET_SCALE(float)
  - Changes the objects visual scale. This does not affect the actual collision box and is mainly intended for special effects things, such as a puff of smoke gradually dissipating by expansion (in combination with TRANS_FADE) or shrinking and disappearing.
- New DDFTHING special: ASSIGN_TID
  - Will assign a unique ID to a mobj when it spawns, managed separately from tags
- New LUA functions: mapobject.render_view_tag(x, y, w, h, tag) and mapobject.render_view_tid(x, y, w, h, tid). See the world from the eyes of any mobj.
  - Tags/TIDs used for this function should be unique; no guarantees regarding which mobj is returned if shared between multiple mobjs
- New LUA functions: mapobject.tagged_info(tag) and mapobject.tid_info(tid).
  - Will return a table of all active map objects with the matching tag/tid for iteration and scripting.


## General Improvements/Changes

- Co-op bot improvements
  - Bots and players (non-voodoo) will no longer clip or telefrag each other
  - Bots will now follow behind the player instead of potentially obstructing their view
  - Up to 15 bots can be spawned regardless of the number of single-player/co-op starts that exist
- Faux skies (not real skyboxes) can use DDFANIM images sequences, allowing for animated skies
- Implemented "idtakeall" cheat; removes current player loadout and gives them their initial benefits
  - Also used for Heretic "idkfa" behavior, although it deviates somewhat
- Add "Simple Skies" to Performance Options menu
  - Can improve performance at the cost of breaking sky flooding, sky floors, and other behavior
- Gameplay will be paused and the cursor released when the console is active
- Consolidated SoundFont and OPL Instrument selection in Sound Menu
  - Now a single option named "MIDI Instrument Set"
  - Lumps named SNDFONT or pack files named SNDFONT.sf2/sf3 will override the "Default" soundfont option
  - OPL emulation will use a GENMIDI lump/pack file if present, otherwise will fall back to a built-in instrument set
- Default automap zoom halved.
- Automap zoom persists for whole play session.
- Allow negative percentages for DDFWEAPON bobbing
- More accurate Heretic/Blasphemer cheats
- Heretic/Blasphemer tweaks
- Autoscale intermission texts if there are too many lines to fit on the screen
- MLOOK_TURN() Weapons.ddf action has been renamed to FACE() with identical behaviour
- "Exclusive Fullscreen" and its functionality removed from Screen Options menu
- Removed support for SID and RAD v2 music formats
 

## Compatibility Fixes

### Vanilla
- Mikoportal support expanded to include all mobjs, not just voodoo dolls
- Things can achieve peccaportal flight under the correct conditions
  - Unlike true vanilla peccaportal flight, things retain a small degree of air control
- Fixes for fading light level cycling
- Fixed negative patch Y offset issue with SKY1 and W105_1 in Doom(1)
- Fixed CheckRelThing callback for missiles missing some collisions that should have occurred
- Improved scaling for intermission text that would not normally fit on the screen
- Reduced MAXMOVE value to 30 to align with expected behavior

### Boom
- Boom Line 242 sector rendering vastly improved
- Carry scroller forces are now additive instead of averaged
- Changed method of determining the sectors a thing is touching (for carry purposes/etc) from
a BSP-based to a linedef-based method to be more in line with Boom behavior

### MBF
- A_Mushroom codepointer revised to be in line with original behavior
- Sky transfers now support offsets, animated skies and scrolling

### MBF21
- All MBF21 Dehacked code pointers implemented

## General Bugfixes

- Replaced PRNG as the previous ranlux24-based implementation was producing odd patterns/bias in regular gameplay
- Fixed Glass linetypes not clearing the BlockSound flag after being shot
- Fix UDMF sidedef mismask offset loading
- Node follower CTD fix
- Fixed legacy bug: switch activation noises when some switch images were not cached and the activating line was not textured
- Flicker light fixes
- Fix for patch atlas lookups for invalid characters
- Calling named RTS Tags via state action RTS_ENABLE_TAGGED did not work
- PNG textures/flats did not tile
- MLOOK_TURN() and MLOOK_FACE() thing.ddf actions were exactly the same. Now MLOOK_TURN() affects horizontal and MLOOK_FACE() affects vertical.
- Changed FACE() thing.ddf action to behave like it's horizontal equivalent TURN().
- Stopped looping SFX still playing on intermission screen
- Fixed player being able to build momentum by running against a wall/closed door

