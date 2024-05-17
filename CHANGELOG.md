CHANGELOG for EDGE-Classic 1.38 (since EDGE-Classic 1.37)
====================================

New Features
------------
- COALHUDS/LUAHUDS: New commands 
	- hud.lookup_LDF(languageEntry) which will return the language.ldf entry
	- hud.game_skill() which will return a number from 0 to 4 reflecting the current game skill
	- hud.get_text_width(string,size) will return the width in pixels of the given string, taking into account the current font
- LUAHUDS-specific commands:
  - Several functions which will return a table containing useful information:
    - game.info()
    - map.info()
    - sector.info()
- UDMF: Linedef 'alpha' field now supported; governs translucency of associated sidedef surfaces
- UDMF: Sector 'alphafloor' and 'alphaceiling' fields now supported; governs translucency of associated planes


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
- Developers:
  - Support for exceptions/RTTI removed from program with compiler flags/options set accordingly
    - Primesynth swapped back to Fluidlite for MIDI playback due to Primesynth's heavy reliance on exceptions
  - Use of C++17 filesystem functions removed and replaced with Windows/POSIX specific functions
  - All uses of streams replaced with EPI file or memory I/O
  - Superfluous typedefs removed in favor of standard types (i.e., uint8_t instead of byte, etc)
  - Simple macros converted to either inline functions or constexpr values as appropriate
    - Remaining macros use consistent naming scheme (EDGE_*, DDF_*, EPI_*, etc)
  - Consistent code styling and formatting performed on all files under the /source_files project folder


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