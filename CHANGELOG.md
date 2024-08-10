CHANGELOG for EDGE-Classic 1.38 (since EDGE-Classic 1.37)
====================================

New Features
------------
+ IWAD support added for Chex Quest 3: Vanilla and Chex Quest 3: Modding (both can be found at https://melodic-spaceship.neocities.org/chex3v/downloads)
  - MBF dog sprites from CQ3 Vanilla also added to original Chex Quest 1 base
+ IWAD support added for Harmony Compatible (https://www.doomworld.com/idgames/levels/doom2/Ports/g-i/harmonyc)
  - Support for the original Harmony release remains unchanged
+ Console: New commands
  - cd : Allows switching directory contexts for the ls/dir, exec, pwd, and cat/type commands. Valid options are 'game' and 'home'
    - Note that when running in portable mode these will resolve to the same path.
  - pwd : Print real path to either home or game directory
    - As above, in portable mode these are the same thing.
+ COALHUDS/LUAHUDS: New commands 
	- hud.lookup_LDF(languageEntry) which will return the language.ldf entry
	- hud.game_skill() which will return a number from 0 to 4 reflecting the current game skill
	- hud.get_text_width(string,size) will return the width in pixels of the given string, taking into account the current font
+ LUAHUDS-specific commands:
  - Several functions which will return a table containing useful information:
    - game.info()
    - map.info()
    - sector.info()
+ UDMF: Linedef 'alpha' field now supported; governs translucency of associated sidedef surfaces
+ UDMF: Sector 'alphafloor' and 'alphaceiling' fields now supported; governs translucency of associated planes
+ GAMES.DDF: New command 
	- DEFAULT_DAMAGE_FLASH = (hex) which will set damage flashes from all sources to this color unless specified otherwise in an individual DDFATK entry.
+ THINGS.DDF: 
  - New Special flag: TRIGGER_TELEPORTS which will allow this thing to use teleports even if NO_TRIGGER_LINES is set. i.e. you want a dog that cannot open a door, but can teleport for example.
+ New CVARs/params:
  - "use_menu_backdrop" which governs the use of the auto-generated monochrome backdrop for option menus
  - "fliplevels" allows you to play a mirrored version of the map. All textures will also be mirrored.
    - New "flipme" cheat code also allows this to be toggled during gameplay
+ STYLES.DDF: able to add a dropshadow effect to TEXT, ALT, TITLE, HELP, HEADER or SELECTED strings via 2 new sub-commands
  - .DROPSHADOW_COLOURMAP = [colourmap];  //"TEXT_BLACK" colmap generally looks good with everything
  - .DROPSHADOW_OFFSET = [float]; //smaller values like 1 or 2 are best
+ STYLES.DDF: ENTRY_ALIGNMENT will now work with the MESSAGE style (pickup messages)
  - Values other than "LEFT" or "RIGHT" will center the messages as usual
  - Only X alignment can be altered; pickup messages will always be at the top of the screen



General Improvements/Changes
--------------------
- Optimized GL state caching and other rendering-related factors to improve performance with the current renderer
- Reduced CPU load when frame limiting (i.e., regular 35/70FPS modes)
- Reduced renderer near clipping distance to prevent graphical oddities when traversing in and out of deep water
- Removed voxel loader and rendering functions due to performance concerns
- Improved sector fogwall appearance/coverage when adjacent to detail slopes
- Added one-time startup message/warning to user if loading a configuration file from a previous version of the program
- Disabled default support for Strife (it is now a compile-time option for developers)
- Limited fallback IWAD selection dialog to 8 options, even if more valid games are found.
  - This is to account for a hardcoded SDL limitation for some backends
- Tweaked MIDI/OPL default gain levels to be more consistent with each other
- FIRST and LAST fields in anims.ddf now work with TX textures
- Autoswitch to a new weapon works even if the new weapon's clip is not full
- Restore mipmapping as independent menu option
- Stacked pickup messages now allow multiple messages on screen simultaneously. Number of simultaneous messages can be changed from options menu
- Keylocked doors and Cheat messages moved to middle of the screen (same as "Found a Secret" message)
- Chex Quest
- Updated Blasphemer base files to support the latest Catoptromancy fork (https://github.com/Catoptromancy/blasphemer/releases/tag/006 as of 1.38)
- Developers:
  - Support for exceptions/RTTI removed from program with compiler flags/options set accordingly
    - Primesynth swapped back to Fluidlite for MIDI playback due to Primesynth's heavy reliance on exceptions
  - Use of C++17 filesystem functions removed and replaced with Windows/POSIX specific functions
  - All uses of streams replaced with EPI file or memory I/O
  - Superfluous typedefs removed in favor of standard types (i.e., uint8_t instead of byte, etc)
  - Simple macros converted to either inline functions or constexpr values as appropriate
    - Remaining macros use consistent naming scheme (EDGE_*, DDF_*, EPI_*, etc)
  - Consistent code styling and formatting performed on all files under the /source_files project folder
  - Removed support for absolute paths and path traversal with '..' for the exec, ls/dir, and cat/type console commands
  - Removed support for path traversal with '..' when searching for or opening pack files
 


Bugs fixed
----------
- Fixed sector fogwall check producing black 0% alpha fog walls for otherwise unfogged sectors
- Fixed Autoscale texture linetypes(855,856,857) and Brightwall(850) being USEable (repeatedly) by the player.
- Fixed DDFLEVEL fog density using wrong multiplier when being applied to sectors
- Fixed DDFLANG entries not being applied to Option Menu entry names
- Fixed several factors preventing standalone games using the EDGEGAME lump/file from loading
- Fixed BOOM Colourmaps with lump name collisions potentially having assertion errors on load
- Fixed MBF dog replacement sound lumps not overriding the stock OGG files
- Added entries for multiple missing BEX strings
- Fixed Boom all-key door failure messages not distinguishing between whether or not 3 or 6 keys are required
- Fixed Dehacked code pointer entries with trailing spaces not matching defined actions
- Fixed frames with a valid '0' rotation sprite still using other rotations if found elsewhere in the load order
- Fixed UDMF 'ypanningfloor' and 'ypanningceiling' using inverted values.
- Fixed COAL/Lua functions hud.get_image_width() and hud.get_image_height() not returning 0 if image does not exist.
- Fixed multiple RTS scripts with the same tag not running simultaneously: we were only running the first one basically.
- Fixed bot automap arrows being drawn in deathmatch; reinstated disabling of automap cheats when in deathmatch
- Fixed raw flats exported from SLADE (.raw extension) from being recognized and loaded properly when in an EPK/folder