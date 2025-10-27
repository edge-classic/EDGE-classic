
# COMPILING EDGE-Classic

## Build Options

The following options can be passed to CMake to control certain features:

- EDGE_SOKOL_GL (default OFF): Will force use of the Sokol GL 3.3 render path
- EDGE_SOKOL_GLES3 (default OFF): Will force use of the Sokol GLES3 render path
- EDGE_SOKOL_D3D11 (default OFF): Will force use of the Sokol Direct3D 11 render path
- EDGE_LEGACY_GL (default OFF): Will force use of the original GL 1.3 render path. BSP traversal will not be threaded with this render path.
  - If none of the renderer options are selected, a default will be chosen based on platform. This means Sokol D3D11 for Windows, Sokol GLES3 for Emscripten, and Sokol GL 3.3 for all other targets.
- EDGE_CLASSIC (default ON): Will build all of EDGE-Classic's default features. Disabling this will build a smaller core engine that is still capable of playing many Doom mapsets but will remove support for the following:
  - COAL scripting language
  - Dehacked patch parsing and application
  - Tracker music (S3M/MOD/XM/IT/FT) playback
  - C64 SID playback
  - OPL emulation for MIDI playback
  - MUS to MIDI track conversion
  - IMF music playback
  - Doom format sound effect playback
  - PC speaker sound effect playback
  - MUSINFO-based music changers
  - UMAPINFO parsing and application
- EDGE_SANITIZE (default OFF): Will build with AddressSanitizer support. This option is mutually exclusive with EDGE_SANITIZE_THREADS and EDGE_SANITIZE_UB. Suppressions can be found in ASanSuppress.txt.
- EDGE_SANITIZE_THREADS (default OFF): Will build with ThreadSanitizer support. Suppressions can be found in TSanSuppress.txt. This option is mutually exclusive with EDGE_SANITIZE and EDGE_SANITIZE_UB and only works with non-MSVC builds.
- EDGE_SANITIZE_UB (default OFF): Will build with UndefinedBehaviorSanitizer support. This option is mutually exclusive with EDGE_SANITIZE and EDGE_SANITIZE_THREADS and only works with non-MSVC builds.
- EDGE_PROFILING (default OFF): Will build with support for the Tracy profiler.
- EDGE_EXTRA_CHECKS (default OFF): Will perform extra validation checks when launching/running the program for development purposes.

These options are specific to Emscripten builds; although they offer a substantial improvement in performance, they are disabled by default for compatibility with the widest range of web browsers:

- EDGE_WEB_SIMD (default OFF): Enable support for WebAssembly SIMD and SSE2 instruction compatibility.
- EDGE_WEB_MULTITHREADED (default OFF): Enable support for audio worklets and multithreaded BSP traversal.
  - The multithreaded web player requires cross-origin isolation; see https://web.dev/articles/coop-coep for more details

## Windows Compilation using MSVC Build Tools and VSCode

Download the Visual Studio Build Tools Installer and install the 'Desktop Development with C++' Workload
  - Also select the "C++ CMake tools for Windows" optional component

Install VSCode as well as the C/C++ and CMake Tools Extensions

After opening the project folder in VSCode, select the 'Visual Studio Build Tools (version) Release - x86_amd64' kit for 64-bit, or the x86 kit for 32-bit

Select the Release CMake build variant

Click Build

## Windows Compilation using MSYS2

This section assumes that you have completed the steps at https://www.msys2.org/ and have a working basic MSYS2 install

From an MSYS prompt for your target architecture:

Install the following additional packages:
* `base-devel` (if not performed during initial MSYS2 install/setup)
* `mingw-w64-(arch)-toolchain` (if not performed during initial MSYS2 install/setup)
* `mingw-w64-(arch)-cmake`
* `mingw-w64-(arch)-SDL2`

Then, after navigating to the project directory:

```
> cmake -B build -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release
> cmake --build build (-j# optional, with # being the number of threads/cores you'd like to use)
> strip edge-classic.exe (if desired)
```

## Windows Compilation from Linux using MinGW

Install the following packages with their dependencies (exact names may vary based on distribution):
* `cmake`
* `build-essential`
* `mingw-w64`

Then, after navigating to the project directory in a terminal:

```
> cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-mingw32.cmake (32-bit build, 64-bit builds can use Toolchain-mingw64.cmake)
> cmake --build build (-j# optional, with # being the number of threads/cores you'd like to use)
> strip edge-classic.exe (if desired)
```

## Linux Compilation

This section assumes that you have a display server and graphical environment installed

Install the following packages with their dependencies (exact names may vary based on distribution):
* `cmake`
* `build-essential`
* `libsdl2-dev`

Then, after navigating to the project directory in a terminal:

```
> cmake -B build -DCMAKE_BUILD_TYPE=Release
> cmake --build build (-j# optional, with # being the number of threads/cores you'd like to use)
```

## BSD Compilation

This section assumes that you have a display server and graphical environment installed

Install the following packages with their dependencies (exact names may vary based on distribution):
* `cmake`
* `sdl2`

Then, after navigating to the project directory in a terminal:

```
> cmake -B build -DCMAKE_BUILD_TYPE=Release
> cmake --build build (-j# optional, with # being the number of threads/cores you'd like to use)
```
## WebGL Compilation

In order to build for the web you'll need:

* `cmake`
* `python3` - (Required by Emscripten)
* `Emscripten` - Download and install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)  

Before building, copy an IWAD such as [freedoom2.wad](https://freedoom.github.io/download.html) to the example website template in the project's root ```/web/preload``` folder, if you are using an IWAD other than freedoom2.wad, edit ```/web/site/index.html``` accordingly.  

You can add other WAD files to the preload folder.  For example, to play  [Arctic Wolf: Revisited](https://www.moddb.com/mods/edge-classic-add-ons/downloads/arctic-wolf-revisited), copy the WAD and then modify the arguments list in ```index.html``` adding ```"-file", "arctre.wad"```

When adding files to the preload folder, building is required as the data is processed to be loaded by browser. 

Now, configure and build:

```
> cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=<path to Emscripten SDK folder>/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -G "Unix Makefiles"
> cmake --build build (-j# optional, with # being the number of threads/cores you'd like to use)
```

Once the build is complete, all the required files should be in the ```/web/site``` folder, change directory to this folder and run ```python webplayer.py``` (webplayer.py is a small Python script that ensures correct CORS handling if testing the multithreaded player)

Open a web browser, navigate to ```http://localhost:8000```, and play Edge Classic!

# Launching EDGE-Classic

In all cases (barring the WebGL build per the previous section), the executable will be copied to the root of the project folder upon success. You can either launch the program in place, or copy the following folders and files to a separate directory if desired:
* autoload
* crosshairs
* edge_base
* edge_fixes
* overlays
* soundfont
* edge-classic/edge-classic.exe (OS-dependent)
* edge_defs.epk
* SDL2.dll (MSVC/MinGW builds)