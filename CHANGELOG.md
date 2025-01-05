CHANGELOG for EDGE-Classic 1.39 (since EDGE-Classic 1.38)
====================================

New Features
------------
- Uncapped Framerate
- VWAD support
- MBF21 Dehacked support
- Add DLight image-based auto-coloring via AUTOCOLOUR ddf field
- Implement "extra_light_step" CVAR
- Emu de MIDI; remove ProtoSquare/ChipFreak soundfonts
- New states action: LUA_RUN_SCRIPT("lua_script_name")
- New states actions:  CLEAR_TARGET and FRIEND_LOOKOUT
- New BORE attack flag
- Add "Simple Skies" to Performance Options menu
- New states actions: GRAVITY and NO_GRAVITY
- New states action: SET_SCALE(float)
- New LUA function: mapobject.render_view(x, y, w, h, tid). See the world from the eyes of any mobj.


General Improvements/Changes
--------------------
- Remove Opal instrument selector; bake-in AIL instrument set
- Restore FMMIDI
- Default automap zoom halved.
- Automap zoom persists for whole play session.
- Allow negative percentages for DDFWEAPON bobbing
- Update Heretic/Blasphemer cheats
- Co-op bot improvements
- Sky drawing improvements
- Heretic/Blasphemer tweaks
- Consolidate GL state changes and render calls
- Autoscale intermission texts if there are too many lines to fit on the screen
- MLOOK_TURN() Weapons.ddf action has been renamed to FACE() with identical behaviour
 

Bugs fixed
----------
- Fixed negative patch Y offset issue with SKY1 and W105_1 in Doom(1)
- Fixed Glass linetypes not clearing the BlockSound flag after being shot
- Fix UDMF sidedef mismask offset loading
- Node follower CTD fix
- Fixed legacy bug: switch activation noises when some switch images were not cached and the activating line was not textured
- Flicker light fixes
- Boom line 242 support vastly improved
- Fix for patch atlas lookups for invalid characters
- Calling named RTS Tags via state action RTS_ENABLE_TAGGED did not work
- PNG textures/flats did not tile
- MLOOK_TURN() and MLOOK_FACE() thing.ddf actions were exactly the same. Now MLOOK_TURN() affects horizontal and MLOOK_FACE() affects vertical.
- Changed FACE() thing.ddf action to behave like it's horizontal equivalent TURN().


