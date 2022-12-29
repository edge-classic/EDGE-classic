
# COMPILING EDGE-Classic

## Windows Compilation using MSVC Build Tools and VSCode

1. Download the Visual Studio Build Tools Installer and install the 'Desktop Development with C++' Workload
   - Also select the "C++ CMake tools for Windows" optional component

2. Install VSCode as well as the C/C++ and CMake Tools Extensions

3. After opening the project folder in VSCode, select the 'Visual Studio Build Tools (version) Release - x86_amd64' kit for 64-bit, or the x86 kit for 32-bit

4. Select the Release CMake build variant

5. Click Build

## Windows Compilation using MSYS2

This section assumes that you have completed the steps at https://www.msys2.org/ and have a working basic MSYS2 install. Using the clang32/clang64 environment
is required due to the need to build against the Windows UCRT

From an MSYS prompt for your target architecture:

Install the following additional packages:
* `mingw-w64-clang-(i686 or x86_64)-toolchain` (if not already performed on initial setup)
* `mingw-w64-clang-(i686 or x86_64)-cmake`
* `mingw-w64-clang-(i686 or x86_64)-SDL2`

Then, after navigating to the project directory:

```
> cmake -B build -G "MSYS Makefiles" -DCMAKE_MAKE_PROGRAM=/(clang32 or clang64)/bin/mingw32-make.exe
> cmake --build build (-j# optional, with # being the number of threads/cores you'd like to use)
```

## Linux Compilation

This section assumes that you have a display server and graphical environment installed

Install the following packages with their dependencies (exact names may vary based on distribution):
* `cmake`
* `g++`
* `libsdl2-dev`

Then, after navigating to the project directory in a terminal:

```
> cmake -B build
> cmake --build build (-j# optional, with # being the number of threads/cores you'd like to use)
```

## BSD Compilation

This section assumes that you have a display server and graphical environment installed

Install the following packages with their dependencies (exact names may vary based on distribution):
* `cmake`
* `sdl2`

Then, after navigating to the project directory in a terminal:

```
> cmake -B build
> cmake --build build (-j# optional, with # being the number of threads/cores you'd like to use)
```

# Launching EDGE-Classic

In all cases, the executable will be copied to the root of the project folder upon success. You can either launch the program in place, or copy the following folders and files to a separate directory if desired:
* autoload
* edge_base
* edge_fixes
* soundfont
* edge-classic/edge-classic.exe (OS-dependent)
* edge-defs.wad
* libgcc_s_dw2-1.dll (Windows-only, if present; can appear with MSYS2 builds)
* libstdc++-6.dll (Windows-only, if present; can appear with MSYS2 builds)
* libwinpthread-1.dll (Windows-only, if present; can appear with MSYS2 builds)
* SDL2.dll (Windows-only)
