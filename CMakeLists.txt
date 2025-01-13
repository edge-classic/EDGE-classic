##########################################
# Edge Classic - CMake Script
##########################################

cmake_minimum_required(VERSION 3.20)

project(
  edge-classic
  LANGUAGES C CXX
  VERSION 0.1.0
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Rendering Options

# Sokol Renderer
option(EDGE_SOKOL_GL "Sokol GL" OFF)
option(EDGE_SOKOL_GLES3 "Sokol GLES3" OFF)
option(EDGE_SOKOL_D3D11 "Sokol D3D11" OFF)

if (EDGE_SOKOL_GL OR EDGE_SOKOL_GLES3 OR EDGE_SOKOL_D3D11)
  set (EDGE_SOKOL ON)
endif()

# Optional Features
option(EDGE_COAL_SUPPORT "Enable support for COAL scripting" ON)
option(EDGE_DEHACKED_SUPPORT "Enable support for Dehacked patch conversion" ON)
option(EDGE_DOOM_SFX_SUPPORT "Enable support for Doom/PC Speaker sound format" ON)
option(EDGE_FLAC_SUPPORT "Enable support for FLAC music format" ON)
option(EDGE_IMF_SUPPORT "Enable support for IMF music format" ON)
option(EDGE_MP3_SUPPORT "Enable support for MP3 music format" ON)
option(EDGE_MUS_SUPPORT "Enable support for MUS MIDI format" ON)
option(EDGE_OGG_SUPPORT "Enable support for OGG music format" ON)
option(EDGE_SID_SUPPORT "Enable support for C64 SID music" ON)
option(EDGE_TRACKER_SUPPORT "Enable support for Tracker music (MOD/XM/S3M/IT)" ON)
option(EDGE_XMI_SUPPORT "Enable support for XMI MIDI format" ON)
option(EDGE_VWAD_SUPPORT "Enable support for k8vavoom VWAD archives" ON)
option(EDGE_WAV_SUPPORT "Enable support for WAV sound format" ON)

# Development 
option(EDGE_SANITIZE "Enable code sanitizing" OFF)
option(EDGE_PROFILING "Enable Profiling" OFF)

include("${CMAKE_SOURCE_DIR}/cmake/EDGEClassic.cmake")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
  set(CLANG true)
else()
  set(CLANG false)
endif()

if (EMSCRIPTEN)
  include("${CMAKE_SOURCE_DIR}/cmake/Emscripten.cmake")  
endif()

if (WIN32)
  add_definitions(-D_WIN32_WINNT=0x601)
endif()

if (MSVC)

  # Disable RTTI
  string(FIND "${CMAKE_CXX_FLAGS}" "/GR" MSVC_HAS_GR)
  if(MSVC_HAS_GR)
      string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
      add_compile_options(/GR-)
  endif()
  
  # Disable C++ Exceptions
  string(REGEX REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")    
  add_compile_options(/D_HAS_EXCEPTIONS=0)
  
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fp:fast")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast")

  if (NOT CLANG)
    # get the number of logical cores for parallel build
    cmake_host_system_information(RESULT LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
    math(EXPR COMPILE_CORES "${LOGICAL_CORES} - 1")  
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP${COMPILE_CORES}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP${COMPILE_CORES}")
  endif()

  # Disable some very noisy warnings from the MSVC build
  # CRT security and POSIX deprecation warnings
  add_definitions("-D_CRT_SECURE_NO_WARNINGS /wd4996")
  # Loss of precision/data on assignment, requires lots of explicit casting
  add_definitions("/wd4244 /wd4267")
  # Unreferenced formal parameter, and there are many of these
  add_definitions("/wd4100")

  # warning level for edge specific source files 
  set (EDGE_WARNING_LEVEL "/W4")

  # To use the sanitizer with MSVC, you will need to either have your Visual Studio
  # or Build Tools install in your PATH variable, or copy the appropriate DLL to the program
  # folder before launching. The paths and filenames can vary based on your setup,
  # but, as an example, for a 64-bit Debug build using MSVC 2022 Build Tools, the path would be
  # C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\<version number>\bin\Hostx64\x64
  # and the file would be clang_rt.asan_dbg_dynamic-x86_64.dll
  if (EDGE_SANITIZE AND MSVC_VERSION GREATER_EQUAL 1929)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address /Oy-")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address /Oy-")
  endif()

  if (CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat -Wno-c++98-compat-pedantic")
  endif()

  set(CMAKE_EXE_LINKER_FLAGS "/SUBSYSTEM:WINDOWS")
else()

  if (WIN32 AND CLANG)
    add_definitions("-D_CRT_SECURE_NO_WARNINGS")
  endif()

  # warning level for edge specific source files 
  set (EDGE_WARNING_LEVEL -Wextra)

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fno-exceptions -fno-strict-aliasing")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-exceptions -fno-rtti -fno-strict-aliasing")

  if (EDGE_SANITIZE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    if (NOT CLANG)
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libasan")
    endif()
  endif()

  if (MSYS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -mwindows")
  endif()

endif()

# set some directory values for various situations

if(${CMAKE_SYSTEM} MATCHES "BSD")
  include_directories("/usr/local/include")  
endif()

if (MSVC)
  set(SDL2_DIR "${CMAKE_SOURCE_DIR}/libraries/sdl2")
endif()

find_package(SDL2 REQUIRED)

# set certain definitions (if appropriate)

if (APPLE)
  include_directories(${SDL2_INCLUDE_DIR})  
  if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm64" AND APPLE)
    add_compile_definitions(APPLE_SILICON)
  elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64" AND APPLE)
    add_compile_definitions(NOT_APPLE_SILICON)
  endif()
endif()

if (EDGE_SOKOL)
  add_definitions(-DEDGE_SOKOL)
  if (EDGE_SOKOL_GL)  
    find_package(OpenGL REQUIRED)
  endif()
else()
  find_package(OpenGL REQUIRED)
endif()

if (EDGE_COAL_SUPPORT)
  add_definitions(-DEDGE_COAL_SUPPORT)
endif()

if (EDGE_DEHACKED_SUPPORT)
  add_definitions(-DEDGE_DEHACKED_SUPPORT)
endif()

if (EDGE_DOOM_SFX_SUPPORT)
  add_definitions(-DEDGE_DOOM_SFX_SUPPORT)
endif()

if (EDGE_FLAC_SUPPORT)
  add_definitions(-DEDGE_FLAC_SUPPORT)
endif()

if (EDGE_IMF_SUPPORT)
  add_definitions(-DEDGE_IMF_SUPPORT)
endif()

if (EDGE_MP3_SUPPORT)
  add_definitions(-DEDGE_MP3_SUPPORT)
endif()

if (EDGE_MUS_SUPPORT)
  add_definitions(-DEDGE_MUS_SUPPORT)
endif()

if (EDGE_OGG_SUPPORT)
  add_definitions(-DEDGE_OGG_SUPPORT)
endif()

if (EDGE_SID_SUPPORT)
  add_definitions(-DEDGE_SID_SUPPORT)
endif()

if (EDGE_TRACKER_SUPPORT)
  add_definitions(-DEDGE_TRACKER_SUPPORT)
endif()

if (EDGE_VWAD_SUPPORT)
  add_definitions(-DEDGE_VWAD_SUPPORT)
endif()

if (EDGE_XMI_SUPPORT)
  add_definitions(-DEDGE_XMI_SUPPORT)
endif()

if (EDGE_WAV_SUPPORT)
  add_definitions(-DEDGE_WAV_SUPPORT)
endif()

if (EDGE_PROFILING)
  # these must be defined for all source files
  add_compile_definitions(EDGE_PROFILING TRACY_ENABLE)
endif()

add_subdirectory(libraries)
add_subdirectory(source_files)