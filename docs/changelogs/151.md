# CHANGELOG for EDGE-Classic 1.51 (since EDGE-Classic 1.50)

## New Features
- New LUA/COAL function: hud.kill_sound(SFXname) which will stop a sound if it is currently playing.

## General Improvements/Changes
- Episode menu item positioning updated to allow for up to ten visible episodes (projected UMAPINFO maximum)
- Restored support for C64 PSID/RSID music tracks
- Added "Indexed+Fake Contrast" to Lighting Mode choices in the Video Options menu
- Removed manual tiling and forced promotion of textures to power-of-two sizes
  - As a result, support for NPOT textures is required by the program; for legacy GL builds the GL loader has been updated appropriately
- "Maximum Dynamic Lights" performance menu option changed to "Max Dynamic Light Radius"
  - Better at helping performance without sacrificing intended aesthetics
- Added compile-time support for WebAssembly multithreading and SIMD support for Emscripten builds
- Removed "Simple Skies" performance menu option
  - Broke too many maps to be worth the potential improvement
- Added support for linedef id/arg0 parameter split in support of cross-port UDMF efforts
- Sound channels playing sfx from a valid mobj will now update their origin coordinates each tick
- Updated PL_MPEG to optimized fork with SIMD (SSE2/NEON) paths to reduce MPEG playback overhead
- Added current working directory, default XDG/Freedoom install directories (Unix), and default GOG/Steam installation paths (Windows) to list of locations searched for compatible IWADs
- SDL Dialog IWAD selector removed; replaced with the "Preferred Game" Options Menu item
  - The Preferred Game, if present in any search path, will be loaded on program start if the `-iwad` parameter is absent
    - EC standalone games (i.e., EPKs or WADs with the EDGEGAME lump) are an exception and will always take precedence in order to support releases that are bundled with an executable
    - If the Preferred Game is not detected, another IWAD will be selected. IWADs that are more likely to have community/external content will take precedence (for instance, Doom 2 before Doom 1, Doom 1 before Heretic, etc)

## Compatibility Fixes

### Vanilla
- Fixed Dehacked-modified chainsaws not using the S_SAW2 attack state missing the proper ENGAGED_SOUND
- Fixed weapons with a non-stock ammo type not granting the appopriate ammunition on pickup
- Fixed the special_lines_hit vector not being cleared when teleporting
- Fixed flat flooding emulation attempting to draw over midtextures on the same sidedef

### Boom
- Fixed destination heights for the Shortest Lower/Upper Texture lift targets
- Fixed ANIMATED entry name checks not being case-insensitive
- Adjusted movefactor for high-friction ("muddy") sectors to be closer to other ports

### MBF
- Altered behavior of A_Die codepointer to align with other ports
- Fixed A_Mushroom not spawning the correct mobj for its 'fireballs'
- Fixed mobjs with the BOUNCES flag not being marked as SHOOTABLE
  - This is a specific fix for Dehacked-altered mobjs; the original EDGE BOUNCES special is unchanged
- Fixed mobjs with the BOUNCES flag losing X/Y momentum after bouncing off of a floor/ceiling
  - This is a specific fix for Dehacked-altered mobjs; the original EDGE BOUNCES special is unchanged
- Fixed mobjs with BOUNCES+MISSILE not exploding when they contact a wall
  - This is a specific fix for Dehacked-altered mobjs; the original EDGE BOUNCES+MISSILE specials are unchanged

### MBF21
- Fixed the `MBF21 Flags` Dehacked field not being zero-initialized when allocating new states
- Fixed A_MonsterProjectile and A_WeaponProjectile not being able to reference mobjs that originate from stock DDFATK instead of DDFTHING
- Fixed `Splash group` not being applied to dynamic mobjs created from a new A_WeaponProjectile/A_MonsterProjectile DDFATK entry
- Fixed A_RadiusDamage checking the splash group of the calling mobj's source instead of the mobj itself
- Fixed Dehacked-altered weapons with an "Ammo per shot" value of 0 being converted to use the NOAMMO ammo type when not desired
- Removed attempt to scale height of new attack codepointers to their calling mobj's height; fixed height of 32 is used instead

### Other
- Fixed MUSINFO entry names with leading numbers being split into separate tokens when parsing
- Titlescreen graphics will now ignore offsets to align with most other ports' behavior
- Fixed DSDehacked things not being flagged as spawnable (indices beyond the DEHEXTRA range)
- Removed arbitrary hard cap on DSDehacked indices; converter now uses an associative map and only allocates new items on demand
- Fixed various UMAPINFOs being treated as invalid due to certain (incorrect) assumptions about map naming and ordering
- Raised loop limit for successive 0-tic states to 64 (formerly 8 for mobj thinking and 10 for weapon psprites)
  - This should accommodate more complex MBF21 mods while still allowing a 'safety hatch' if the loop runs away somehow
- Added 1 tic of duration for the last frame of states comprised completely of zero-tic states that do nothing
  - Reduces unnecessary looping during thinker functions
- Fixed all mobjs with the BOUNCES special incorrectly ending P_XYMovement once they bounce off of a wall

## General Bugfixes
- Fixed automap background not defaulting to black fill when not defined by the current AUTOMAP DDFSTYLE
- Fixed sound fx category assignment for FAILED_SFX
- Removed distance attenuation from BOSS category sound fx
- Added missing glReadPixels function pointer to Sokol internal GL loader (for Windows builds)
- Fixed improper tiling of textures-as-flats
- Added missing Green Key graphics
- Fixed level transition not killing all sound effects regardless of category
- Fixed moving planes with a RES_Impossible move result not reversing direction when appropriate
- Fixed legacy GL version check incorrectly flagging certain major/minor versions of GL as unsupported
- Removed invalid references that were still present when EDGE_PROFILING was enabled at build-time
- Fixed incorrect centering of menu items using the SELECTED text style
- Fixed divide-by-zero error in DoLaunchProjectile when the calling mobj has a height of zero
- Fixed stock LUAHUDs not drawing automap fully; leaving a gap if the status bar was hidden/disabled
- Fixed bullet puffs spawned close to ledges "jumping" up to the adjacent floor
- Fixed SDL GL context not being properly deleted on program shutdown (when applicable)
- Fixed web player not allocating a sufficiently large stack for heavy MPEG/audio processing
- Added global parameter reset for tracker music when a song repeats to fix certain problematic songs
- Expanded occlusion clipping scope to prevent artifacts at screen edges when viewing certain geometry
- Fixed depth map usage with various sky drawing routines
- Added missing 'fuzzy' check that caused artifacts when rendering MDL/MD2 mobjs with partial invisibility
- Fixed accidental disabling of Lua if COAL was detected at any point in the load order
  - Correct behavior is for the last loaded lump between LUAHUDS/COALHUDS to take precedence
- Fixed mobjs retaining "above/below" mobj references when not applicable, causing certain items to "hang" in the air
- Fixed certain menu option items (jumping, mouselook, etc) being overridden when loading a game and not appropriate, (i.e. no forcing of these via DDFLEVL/UMAPINFO)
- Fixed rendering artifacts when viewing mobjs through a camera portal whose view height is not 1:1 with the partner area
- Fixed vertex interpolation of MDL/MD2/MD3 model frames (fractional tics were previously not accounted for)