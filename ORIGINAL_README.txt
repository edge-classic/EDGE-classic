
EDGE 1.35 README.TXT
====================

Welcome to EDGE, the Enhanced Doom Game Engine.

Website: http://edge.sourceforge.net

Archive listing:
  * edge135.exe        - EDGE Executable
  * edge.wad           - EDGE WAD Information, version 6.1
  * demo-XXXX.wad      - Demos of various EDGE features
  * SDL.dll            - Simple DirectMedia Layer, version 1.2.13
  * Changelog-135.txt  - Changes since 1.34
  * Edge-Readme.txt    - Guess what you are reading?


System Requirements:
  - Pentium 500 MHz CPU or later.
  - 64MB RAM (128MB RAM recommended).
  - Hardware accelerated 3D card with OpenGL drivers.

  - Either a DOOM, DOOM II, or Final DOOM IWAD file.
    (See "Getting hold of game data" below).

  For Win32:
    - DirectX 7.0 or higher.

  For Linux:
    - SDL (Simple DirectMedia Layer) version 1.2.
    - glibc 2.


Getting hold of game data:
    You can get hold of FreeDoom from freedoom.sourceforge.net which 
    is _free_ and has been produced by fans of the game in their
    spare time, or you can purchase "The Ultimate Doom", "Doom II" or 
    "Final Doom" (Plutonia/TNT) from id software at www.idsoftware.com. 
    The shareware data will also work with EDGE. You need to copy the
    .WAD file you want to use into your EDGE directory. The wad names 
    and what game they belong to are listed below: 

    doom1.wad    -> Shareware Doom.
    doom.wad     -> Doom/Ultimate Doom.
    doom2.wad    -> Doom II
    plutonia.wad -> Final Doom (Plutonia)
    tnt.wad      -> Final Doom (TNT)


Useful Parameters:
  * Screen size: -width [screenwidth] -height [screenheight]
  * Colour depth: -bpp [depth]  (2 for 16-bit, 4 for 32-bit)
  * Windowed mode: -windowed
  * Fullscreen mode: -fullscreen
  * Pick IWAD file: -iwad [main wad file]
  * Play PWAD file: -file [addon wad file]
  * Jump to level: -warp [mapname]  (in the form of MAP01 or E2M9 etc..) 
  * External DDFs: -ddf [dirname]  (use external DDFs in 'dirname')
  * Show EDGE version: -version
  * Disable sound: -nosound
  * Disable music: -nomusic
  * Disable warnings: -nowarn


Notable EDGE features:
  - Extrafloors (3D floors), removing the original DOOM limitation
    which didn't allow rooms over rooms.  EDGE supports real 3D
    room-over-room, bridges, and liquids.

  - DDF (Data Definition Files), allow Mod/TC/PC creators to
    completely customise monsters, attacks, weapons, pickup and
    scenery items, linetypes, sectortypes, intermissions, ETC...
    using fairly simple to understand text files.

  - RTS (Radius Trigger Scripting), provides per-level scripting
    support, allowing custom scripts to run when the player enters
    certain parts of the map, or performs certain actions, or when
    certain monsters have been killed (to name a few possibilities).

  - Hardware accelerated 3D rendering (via OpenGL).

  - Hub system, like in Quake II or Hexen, which allows you to
    come back to a certain map and everything is the same as when
    you left it, plus you get your weapons and keys.

  - MD2 models for monsters, items and player weapons.

  - Jumping and Crouching.
  
  - Look up/down and Zooming.
  
  - Flying (JetPack) and Swimmable water.

  - Many limits and bugs from original DOOM removed.


Timidity Music Support:
    Timidity is a program for playing MIDI music, and EDGE includes
    has a built-in version of it.  For Linux users this is the only
    way to hear DOOM format music, but Windows users can use it too
    (see the Music Device entry in the Sound Options menu). 

    Timidity requires "GUS Patch" files to work, which are special
    sound data for musical instruments.  Under Linux you can simply
    install the 'freepats' package and it should work.  Otherwise
    a good quality patchset can be found in the idgames archive:
    search for "8mbgmpat", download the zip, create a folder where
    EDGE is installed called "8mbgmpat" and unpack the files into
    that folder.  Then run EDGE and the following messages should
    appear in the console:

    >  Loading Timidity config: ./8mbgmpat/TIMIDITY.CFG
    >  I_StartupMusic: Timidity Init OK


NOTE: SoundFonts (SF2 files) are different from GUS patch files
      and are NOT supported by EDGE.


------------------------------------------------------------------------

Major Changes in EDGE 1.35:
  + widescreen support
  + DDF inheritance
  + weapon and automap key bindings can be changed 
  + support textures between TX_START/TX_END

Bugs fixed in 1.35:
  - fixed serious bug with COAL scripts
  - fixed music on MAP32 of DOOM II
  - fixed the marks on the automap
  - fixed GL_COLOUR feature of COLORMAPS.DDF
  - E2M8, E3M8 (etc) are playable without any bosses


Known Issues:
  - Multiplayer (networking) does not work.  The type of networking
    that DOOM used back in 1994 is obsolete today, and EDGE doesn't
    have a suitable replacement yet.

  - Drag-and-drop does not work in Linux.

  - Levels and TCs designed for very early EDGE versions can produce
    a crap-load of warnings when starting up.  These warnings are
    mostly about DDF features that have been removed or changed.
    Hence some older stuff will not work 100%.  Use the -nowarn
    option to turn off these warnings.

  - Some sprites are designed to look sunk into the floor, typically
    corpses, and can look like they are "floating" in EDGE.  This is a
    limitation of using OpenGL rendering (if the sprites are lowered,
    they get clipped and this looks a lot worse).

    Certain other DOOM tricks, e.g. flat-flooding, sometimes don't
    work properly with EDGE's renderer.

  - Colourmaps are another software rendering technique that often
    cannot be emulated in OpenGL hardware rendering.  Old methods
    of doing Fog do not work anymore.

  - Bullets can sometimes pass through solid extrafloors.


Troubleshooting:
  - If EDGE crashes, or otherwise misbehaves, please help us by
    reporting the problem.  To do this check out the EDGE website at
    http://edge.sourceforge.net.  We appreciate input because if you
    don't tell us about a bug, we can't deal with it.

  - Post the EDGE.LOG file which EDGE creates along with your bug
    report, as this contains lots of useful information to help us
    track down the problem.

  For Windows:
    - Ensure you are running with virtual memory ENABLED.  This will
      cause some real problems if it disabled.

    - You can check the DirectX version by running the DirectX
      diagnostic tool.  Click the Start button, then Run..., and 
      type: dxdiag and press ENTER.  If that doesn't work, try looking
      for DXDIAG.EXE in the following directories:

         C:\windows\system
         C:\program files\directx\setup\
         C:\winnt\system32

    - For EDGE to work, you need a 3D card *plus* OpenGL drivers for
      it installed on your computer.  A really low framerate (1 FPS)
      usually means that the *software* OpenGL driver is being used (not
      what we want, of course ;).  Go to the website of your card's
      manufacturer and look for a recent 3D driver.  Another place to
      look is on Microsoft's site.  Basically, if Quake 3 (or some
      other OpenGL game) works, then EDGE should work too.

    - SHOULD YOU HAVE ANY PROBLEMS WITH GRAPHICAL PROBLEMS OR WITH
      THE ENGINE LOCKING UP PLEASE ENSURE YOU HAVE THE LATEST VIDEO
      AND OPENGL DRIVERS FOR YOUR MACHINE.

      Users of the ATI Rage series are recommended to upgrade to the 
      latest drivers since some older drivers (circa 2001) have been 
      known to cause a lockup during startup.

  For Linux:
    - SDL 1.2 can be found at http://www.libsdl.org/.

    - You can check whether hardware 3D rendering is available by
      running the 'glxinfo' program in an terminal, and look for the
      line which says "direct rendering: Yes".


Contact points:
  * For the latest news and developments visit the EDGE Website
    at http://edge.sourceforge.net

  * For Forums, Bug Reports and SVN Access visit the EDGE Project
    site at http://sourceforge.net/projects/edge

  * For more information on "GL Nodes", see the glBSP homepage at
    http://glbsp.sourceforge.net


Compiling EDGE:
  Please read the file "INSTALL" in the source package.


Command line options:
  * Note that "(no)" here means that the option switch can optionally
    be prefixed with the word "no", e.g. -monsters OR -nomonsters.  The
    plain option enables the feature, the one with "no" disables it.

  -version             Show version of EDGE and exit.

  -width  <wd>         Select video mode's width.
  -height <ht>         Select video mode's height.
  -res  <wd> <ht>      Select video mode (width AND height).
  -bpp  <depth>        Video depth: 1 for 8-bit, 2 for 16-bit.
  -lang  <language>    Language for game messages.

  -iwad  <file>        Select IWAD file (e.g. DOOM.WAD or TNT.WAD)
  -file  <file> ...    Select PWAD (add-on) wad file(s) to play.
  -home  <dir>         Home dir, can hold IWAD and EDGE.WAD.
  -game  <dir>         Game dir, for PARMS, DDF, RTS, WADs, etc.
  -ddf   <dir>         Load external DDF files from the directory.
  -script  <file>      Load external RTS script from the file.
  -deh     <file> ...  Load external DeHackEd/BEX patch file(s).
  -config  <file>      Config file (normally EDGE.CFG).
  -debug               Enable debug output to file (DEBUG.TXT).
  -nolog               Disable creating the log file (EDGE.LOG).

  -warp  <map>         Warp to map (use full name e.g. E1M1 or MAP01).
  -loadgame  <slot>    Warp, loading game from slot.
  -skill  <num>        Skill level for warp (1 to 5).
  -screenshot  <rate>  Movie mode! Takes regular screenshots.
  -turbo  <num>        Running speed, 100 is normal.

  -players  <num>      Number of real players.
  -bots  <num>         Number of bots.
  -deathmatch          Deathmatch game (otherwise COOP).
  -altdeath            Alternate deathmatch mode.
  -timer  <mins>       Time limited games, and specifies how long.
  -avg                 Andy Virtual Gaming (same as -timer 20).
  -respawn             Monster respawning.
  -newnmrespawn        Monsters respawn as if resurrected.
  -(no)itemrespawn     Item respawning.

  -videodriver <name>  Specify what SDL video driver to use.
  -directx             shortcut for "-videodriver directx"
  -gdi                 shortcut for "-videodriver gdi" (default)
  -windowed            Run inside a window.
  -fullscreen          Run fullscreen.

  -audiodriver <name>  Specify what SDL audio driver to use.
  -waveout             shortcut for "-audiodriver waveout"
  -dsound              shortcut for "-audiodriver dsound" (default)
  -sound16             Use 16-bit sound output.
  -sound8              Use 8-bit sound output.

  -(no)sound           Enable/disable sound effects.
  -(no)music           Enable/disable music output.

  -strict              Errors: be as strict as possible.
  -lax                 Errors: be as forgiving as possible.
  -(no)warn            Enable/disable all warning messages.

  -(no)smoothing       Smoothing for graphic images (mainly GL).
  -(no)mipmap          Mipmapping enable/disable.
  -(no)dlights         Dynamic lighting enable/disable.
  -(no)jumping         Whether player jumping is allowed.
  -(no)crouching       Whether player crouching is allowed.
  -(no)mlook           Whether Mouse-look up/down is allowed.
  -(no)blood           More blood which lasts longer.
  -(no)kick            Weapon kick effect.

  -(no)monsters        Enable/disable monsters within the game.
  -(no)fast            Fast monsters.
  -(no)cheats          Cheat codes enabled or disabled.
  -(no)rotatemap       Automap rotation.
  -(no)showstats       Shows on-screen statistics/info.
  -(no)hom             HOM (Hall Of Mirrors) detection.

  -(no)extras          Extra items (e.g. rain) appearing in levels.
  -(no)true3d          Objects can pass above/below other objects.

  -trilinear           Trilinear mipmapping.
  -blockmap            Force EDGE to generate its own blockmap.
  -nograb              Don't grab the mouse cursor.


Console commands:
   args  ...              Just prints the arguments (for testing)
   crc   <lump>           Computes the CRC value of a wad lump
   dir  [<path> <mask>]   Display contents of a directory     
   exec  <filename>       Executes console commands from a file
   help                   Prints a summary of console usage
   map   <mapname>        Jump to a new map (like IDCLEV cheat)
   playsound  <sound>     Plays the sound
   resetvars              Reset all cvars and settings
   showcmds               Show all console commands
   showvars  [-l]         Show all console variables              
   showjoysticks          Show all available joysticks
   showfiles              Show all loaded files
   showlumps  <file-idx>  Show all lumps in a wad file
   type  <filename>       Displays the contents of a text file
   version                Show the EDGE version
   quit                   Quit EDGE (pops up a query message)

   <varname>              Show value of a console variable
   <varname>  <value>     Set the value of a console variable


Console variables:
   language               Current language setting

   aggression             monster aggression (EDGE feature)
   goobers                flatten out levels (EDGE feature)
 
   ddf_strict             Errors: be as strict as possible
   ddf_lax                Errors: be as forgiving as possible
   ddf_quiet              Disables all warning messages
 
   in_grab                Grab the mouse cursor
   in_keypad              Enables the numeric keypad
   in_stageturn           Two-stage turning (keyboard & joystick)
   in_warpmouse           Warp the cursor to get relative motion
 
   joy_dead               Joystick dead zone  (0.0 - 0.8)
   joy_peak               Joystick peak point (0.4 - 1.0)
   joy_tuning             Joystick fine tune  (0.2 - 5.0, normally 1)
 
   m_diskicon             Enables the flashing disk icon
   m_busywait             Smoother gameplay vs less CPU utilisation
 
   am_smoothing           Enables smoother lines on the automap
   r_fadepower            Powerup effects smoothly fade out
 
   r_crosshair            Crosshair to draw (1-9, 0 for none)
   r_crosscolor           Crosshair color (0-7)
   r_crosssize            Crosshair size (16 is normal)
   r_crossbright          Crosshair brightness (0.1 - 1.0)

   r_nearclip             OpenGL near distance (default 4)
   r_farclip              OpenGL far distance  (default 64000)
 
   debug_fullbright       Debugging: draw everything full-bright
   debug_hom              Debugging: show missing textures
   debug_joyaxis          Debugging: print joystick axis events
   debug_mouse            Debugging: print mouse events
   debug_pos              Debugging: show player's location
   debug_fps              Debugging: show frames-per-second


**** CREDITS ****

Current EDGE Team:
    Andrew Apted (aka "andrewj")     : programming
    Luke Brennan (aka "Lobo")        : support & documentation

Former EDGE Members:
    Andrew Baker (aka "Darkknight")  : programming
    Marc Pullen  (aka "Fanatic")     : support & music
    Erik Sandberg                    : programming
    David Leatherdale                : support & programming

Patches:
    Darren Salt  (aka "dsalt")       : programming

DOSDoom Contributors:
    Kester Maddock                   : programming
    Martin Howe                      : programming, cat lover
    John Cole, Rasem Brsiq           : programming
    Captain Mellow, CowMonster       : support, graphics
    Ziggy Gnarly & Sidearm Joe       : support, graphics
    Matt Cooke, Eric Simpson         : support

Special Thanks:
    Chi Hoang                        : DOSDoom Author
    id Software                      : The Original Doom Engine


--- END OF README.TXT ---

