![circle_logo5](https://user-images.githubusercontent.com/58537100/146055272-0deb8163-5828-4f2f-b6e3-34b48f53ea10.png)


**EDGE-Classic** is a Doom source port that provides advanced features, ease of modding, and attractive visuals while keeping hardware requirements very modest. It is a revival of the [EDGE 1.35](http://edge.sourceforge.net/) codebase for modern systems.

[EMUS](http://firstgen.no-ip.info/emus/about.htm), the EDGE Modder's Utility Suite, has also been updated to stay current with EDGE-Classic's new features, and is an excellent all-in-one tool for both the beginning and experienced modder.

Here are some mods that showcase EDGE-Classic's flexibility and power:

## Operation: Arctic Wolf Revisited

Operation: Arctic Wolf Revisited is an update/extension of Laz Rojas' classic DOOM 2 mod with improved gameplay, all new weapons, enemies, new mechanics, more interactivity, more immersion, scripting, updated textures, level fixes, etc. making it essentially a whole new game.

![arcticre](https://user-images.githubusercontent.com/58537100/187059072-dd93b206-d3cd-4c2f-bf3e-3200684d33f6.png)

Download from ModDB [here.](https://www.moddb.com/mods/edge-classic-add-ons/downloads/arctic-wolf-revisited)

## Astral Pathfinder

![pathfinder](https://user-images.githubusercontent.com/58537100/187058724-a7601685-b22b-4128-ae4e-eaefc23bcd84.png)

Astral Pathfinder by RunSaber/Chutzcraft is an EDGE exclusive partial conversion of Doom II, with an otherworldy setting and Quake-style weaponry. Three vast levels packed with exploring, secret-hunting and of course, hellish spawn from several dimensions, including three boss fights for the player to contend against.

Download from ModDB [here.](https://www.moddb.com/mods/edge-classic-add-ons/downloads/astral-pathfinder1)

## Heathen

![heathen (2)](https://user-images.githubusercontent.com/58537100/187059362-d27be9f8-ebb5-466a-9239-942516e2120e.png)

A Hexen/Heretic-inspired gameplay mod for Doom. New enemies, weapons, items, and attacks.

Download from ModDB [here.](https://www.moddb.com/mods/edge-classic-add-ons/addons/heathen)

## Duke it out in DOOM

![dukeitout](https://user-images.githubusercontent.com/58537100/187059563-2f6df105-4d54-4295-a664-df16e523227b.png)

An action-packed gameplay mod that puts you in the boots of the political incorrect but lovable ass-kicking macho-man Duke Nukem. Crammed full of of 80's/90's action movie throwbacks and pop culture references. Featuring an impressive arsenal of super deadly, sneaky and devastating weapons and a colorful and humorous collection of power-ups and pickups. Full of witty one-liners from Duke himself and plenty of blood, guts and gore.

Download from ModDB [here.](https://www.moddb.com/mods/duke-it-out-in-doom)

## Aliens: Stranded

![aem](https://user-images.githubusercontent.com/58537100/187060010-143ac59d-dcea-4e7b-a02a-f34a85b01000.png)

A Total Conversion themed on the Aliens movies. New weapons, enemies, music, textures etc. Heavy use of 3D models.

Download from ModDB [here.](https://www.moddb.com/mods/edge-classic-add-ons/downloads/aliens-stranded) The load order of the included WADs is D3tex.wad, AEM.wad, and AEM_Mapset.wad.

## DarkForces Doom

![darkforces](https://user-images.githubusercontent.com/58537100/187060442-3db18f29-f1c4-4b6b-a793-29f6b38270b0.png)

A Star Wars/Dark Forces themed gameplay mod by CeeJay for EDGE-Classic. Weapons, items, enemies and new gameplay mechanics.

Download from ModDB [here.](https://www.moddb.com/mods/edge-classic-add-ons/addons/darkforces-doom)

An optional texture pack to enhance the experience can be found [here.](https://www.moddb.com/mods/edge-classic-add-ons/addons/darkforces-doom-texture-pack)

# Notable Improvements over EDGE 1.35

- Uncapped framerate
- Updated renderer with Direct3D11, GLES3, and GL 3.3 paths in addition to the legacy GL 1.3 renderer
- Multithreaded BSP traversal (when using new renderer)
- Support for Dehacked code pointers up to and including MBF21
- DEHEXTRA compatibility
- DSDehacked compatibility
- Improved compatibility with Boom behavior and rendering (physics, height sectors, etc)
- UDMF map support
- Improved sound mixer with floating-point samples and OpenAL spatialization/attenuation models
- A soundfont-capable MIDI player (SF2/SF3 support)
- OPL emulation with external OP2/AIL/TMB/WOPL instrument support
- Many more music and sound formats (PC Speaker, IMF, MP3, OGG, FLAC, IT/S3M/XM/MOD/FT, C64 PSID/RSID)
- UMAPINFO compatibility
- Migration from SDL1 to SDL2 (longevity and improved gamepad support)
- Lua as the primary scripting language, with a COAL compatibility layer for ease of migration
- Expanded DDF, RTS, and COAL features
- Widescreen statusbar, intermission, and title screens
- Replaced GLBSP with AJBSP as the internal nodebuilder
- Inventory system
- Autoload folder
- New Liquid animation (SMMU, SMMU+Swirl and Parallax)
- Optional Dynamic Sound Reverb
- Optional Pistol Start feature
- Support for image/spritesheet and TrueType fonts
- A load of longstanding bugs fixed (see full changelog)

Bugfixes and detailed changes can be found in CHANGELOG.txt

# Currently Supported Platforms

- Windows 7 or later
- [Web Browsers Supporting WebGL2](https://edge-classic.github.io/play.html)
- Linux (x86 and ARM architecture - ARM tested with Raspberry Pi OS on a Pi 4B)
- BSD (x86 and ARM64 architecture - tested with FreeBSD 13.1; ARM64 tested on a Pi 4B but not recommended unless proper video acceleration is available
                 as playable framerates are too difficult to achieve otherwise)
- Mac (x86 and M1 architecture)

# Currently Supported IWADs

- The Ultimate Doom/Doom 1/Freedoom Phase 1
- Doom 2/Freedoom Phase 2
- Chex Quest 1
- Chex Quest 3: Vanilla (Regular and Modding Edition)
- Heretic/Blasphemer
- HacX 1.2
- Harmony (Original and Compatible Releases)
- REKKR

To learn more about EDGE-Classic, DDF, RTS, or COAL, please visit [our wiki](https://github.com/dashodanger/EDGE-classic/wiki).
