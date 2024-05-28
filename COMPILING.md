
# COMPILING EDGE-Classic

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

## Compilation for Windows XP using w64devkit

WARNING: w64devkit's bundled tools such as GNU Make can produce false positives in Windows Defender; please see https://github.com/skeeto/w64devkit/issues/79 for more information and steps to validate that the Make executable is valid. Any Windows Defender exceptions that are created to account for this are the responsibility of the user and the user alone, and are NOT recommended actions by the development team!

This section assumes that you have downloaded the `i686` release from https://github.com/skeeto/w64devkit/releases and extracted it to a folder of your choosing. You will also need to download the `SDL2-devel-<version>-mingw` package from https://github.com/libsdl-org/SDL/releases/latest and placed the contents of its `i686-w64-mingw32` folder into the `i686-w64-mingw32` folder of your w64devkit installation.

Launch w64devkit.exe from your extracted w64devkit folder.

Then, after navigating to the project directory:

```
> cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
> cmake --build build (-j# optional, with # being the number of threads/cores you'd like to use)
> strip edge-classic.exe (if desired)
```

## Linux Compilation

This section assumes that you have a display server and graphical environment installed

Install the following packages with their dependencies (exact names may vary based on distribution):
* `cmake`
* `g++`
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

*Please note, the web version is less mature than other platforms and there is ongoing work to better integrate it.*

In order to build for the web you'll need:

* `cmake`
* `python3` - (Required by Emscripten)
* `Emscripten` - Download and install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)  

Before building, copy an IWAD such as [freedoom2.wad](https://freedoom.github.io/download.html) to the example website template in the project's root ```/web/preload``` folder, if you are using an IWAD other than freedoom2.wad, edit ```/web/site/index.html``` accordingly.  

You can add other WAD files to the preload folder.  For example, to play  [Arctic Wolf: Revisited](https://www.moddb.com/mods/edge-classic-add-ons/downloads/arctic-wolf-revisited), copy the WAD and then modify the arguments list in ```index.html``` adding ```"-file", "arctre.wad"```

When adding files to the preload folder, building is required as the data is processed to be loaded by browser. 

Now, configure and build:

```
> cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -G "Unix Makefiles"
> cmake --build build (-j# optional, with # being the number of threads/cores you'd like to use)
```

Once the build is complete, all the required files should be in the ```/web/site``` folder, change directory to this folder and run ```python -m http.server```

Open a web browser, navigate to ```http://127.0.0.1:8000```, and play Edge Classic!   

# Launching EDGE-Classic

In all cases (barring the WebGL build per the previous section), the executable will be copied to the root of the project folder upon success. You can either launch the program in place, or copy the following folders and files to a separate directory if desired:
* autoload
* edge_base
* edge_fixes
* soundfont
* edge-classic/edge-classic.exe (OS-dependent)
* edge_defs.epk
* SDL2.dll (Windows-only, w64devkit builds will need to copy SDL2.dll from the /i686-w64-mingw32/bin of your w64devkit install to the directory containing edge-classic.exe)