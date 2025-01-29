##########################################
# Edge Classic - CMake Script
##########################################

cmake_minimum_required(VERSION 3.27)

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
option(EDGE_LEGACY_GL "Legacy GL Renderer" OFF)

# If the legacy GL renderer has not been selected and
# a Sokol backend is not already specified by CMake params, 
# choose a default based on platform
if (NOT EDGE_LEGACY_GL)
  if (NOT EDGE_SOKOL_GL AND NOT EDGE_SOKOL_D3D11 AND NOT EDGE_SOKOL_GLES3)
    if (WIN32 OR MINGW)
      set (EDGE_SOKOL_D3D11 ON)
    elseif (EMSCRIPTEN)
      set (EDGE_SOKOL_GLES3 ON)
  # Unix; Apple will eventually have a Metal backend but it should support 
  # the level of GL in use by Sokol GFX/GL
    else ()
      set (EDGE_SOKOL_GL ON)
    endif()
  endif()
endif()

if (EDGE_SOKOL_GL OR EDGE_SOKOL_GLES3 OR EDGE_SOKOL_D3D11)
  set (EDGE_SOKOL ON)
endif()

# memory allocator
option(EDGE_MIMALLOC "Enable mimalloc" ON)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86|i[3456]86)$" OR CMAKE_GENERATOR_PLATFORM MATCHES "^(x86|Win32|win32)$")
  set(EDGE_MIMALLOC OFF)
  message(STATUS "Forcing mimalloc off for 32 bit build")
endif()

# Optional Features
option(EDGE_CLASSIC "Enable default features for EDGE-Classic" ON)

# Development 
option(EDGE_SANITIZE "Enable code sanitizing" OFF)
option(EDGE_PROFILING "Enable Profiling" OFF)

# Enables exhaustive memory checks, see mimalloc CMakeLists.txt for what is enabled
# Memory error logging goes to debug.txt
option(EDGE_MEMORY_CHECK "Enable Memory Checks" OFF)
# If fatal memory checks are enabled, any memory error will be fatal and provide a callstack
option(EDGE_MEMORY_CHECK_FATAL "Enable Fatal Memory Checks" OFF)

include("${CMAKE_SOURCE_DIR}/cmake/EDGEClassic.cmake")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
  set(CLANG true)
else()
  set(CLANG false)
endif()

if (EMSCRIPTEN)
  include("${CMAKE_SOURCE_DIR}/cmake/Emscripten.cmake")  
endif()

if (WIN32 OR MINGW)
  add_definitions(-D_WIN32_WINNT=0x601)
endif()

if(MSVC)
    # Use static C runtime, necessary for mimalloc and also means matching C runtime doesn't need to be on users box
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:Debug>")   
      if (EDGE_MIMALLOC)
      # Override memory allocations to use mimalloc
      add_definitions(/FI"${CMAKE_SOURCE_DIR}/libraries/mimalloc/include/mimalloc-override.h")
    endif()
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

  if (MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "-lmingw32 ${CMAKE_EXE_LINKER_FLAGS}")
  endif()

endif()

# set some directory values for various situations

if(${CMAKE_SYSTEM} MATCHES "BSD")
  include_directories("/usr/local/include")  
endif()

if(MINGW OR MSVC OR (WIN32 AND CLANG))
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
  elseif (EDGE_SOKOL_GLES3 AND NOT EMSCRIPTEN)
    find_package(OpenGL COMPONENTS GLES3 REQUIRED)
  endif()
else()
  find_package(OpenGL REQUIRED)
endif()

if (EDGE_CLASSIC)
  add_definitions(-DEDGE_CLASSIC)
endif()

if (EDGE_PROFILING)
  # these must be defined for all source files
  add_compile_definitions(EDGE_PROFILING TRACY_ENABLE)
endif()

if (EDGE_MEMORY_MIMALLOC)
  add_compile_definitions(EDGE_MIMALLOC)
  if (EDGE_MEMORY_CHECK)
    add_compile_definitions(EDGE_MEMORY_CHECK)
    if (EDGE_MEMORY_CHECK_FATAL)
      add_compile_definitions(EDGE_MEMORY_CHECK_FATAL)
    endif()
  endif()
endif()

add_subdirectory(libraries)
add_subdirectory(source_files)