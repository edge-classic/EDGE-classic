##########################################
# Edge Classic - CMake Script
##########################################

cmake_minimum_required(VERSION 3.20)

project(
  edge-classic
  LANGUAGES C CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(EDGE_GL_ES2 "Enable GLES2 Rendering Backend" OFF)
option(EDGE_SANITIZE "Enable code sanitizing (Clang/GCC) " OFF)

include("cmake/EDGEClassic.cmake")  

if (WIN32)
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(WIN32_CLANG true)
  else()
    set(WIN32_CLANG false)
  endif()
endif()

if (EMSCRIPTEN)
  include("cmake/Emscripten.cmake")  
endif()

if (MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fp:fast")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /EHs")
  
  # Disable some very noisy warnings from the MSVC build
  # CRT security and POSIX deprecation warnings
  add_definitions("-D_CRT_SECURE_NO_WARNINGS /wd4996")
  # Loss of precision/data on assignment, requires lots of explicit casting
  add_definitions("/wd4244 /wd4267")
  # Unreferenced formal parameter, and there are many of these
  add_definitions("/wd4100")
  
  set(CMAKE_EXE_LINKER_FLAGS "/SUBSYSTEM:WINDOWS")
else()
  if (WIN32_CLANG)
    add_definitions("-D_CRT_SECURE_NO_WARNINGS")
  endif()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffast-math -fno-strict-aliasing -Wall")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -fno-strict-aliasing -Wall")

  if (EDGE_SANITIZE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=address -static-libasan")
  endif()

endif()


if (WIN32)
  set(CMAKE_MODULE_PATH cmake)
  set(SDL2_DIR "${CMAKE_SOURCE_DIR}/source_files/sdl2")
endif()

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm64" AND APPLE)
	add_compile_definitions(APPLE_SILICON)
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64" AND APPLE)
	add_compile_definitions(NOT_APPLE_SILICON)
endif()
if(APPLE OR ${CMAKE_SYSTEM} MATCHES "BSD")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I /usr/local/include")
endif()

find_package(SDL2 REQUIRED)
if (NOT EDGE_GL_ES2)
  find_package(OpenGL REQUIRED)
else()
  add_definitions(-DEDGE_GL_ES2)
endif()

add_subdirectory(source_files/ajbsp)
add_subdirectory(source_files/coal)
add_subdirectory(source_files/crsid)
add_subdirectory(source_files/ddf)
add_subdirectory(source_files/dehacked)
add_subdirectory(source_files/ec_voxelib)
add_subdirectory(source_files/epi)
if (NOT EDGE_GL_ES2)
  add_subdirectory(source_files/glad)
else()
  add_subdirectory(source_files/gl4es)
endif()
add_subdirectory(source_files/libgme)
add_subdirectory(source_files/libprimesynth)
add_subdirectory(source_files/libymfm)
add_subdirectory(source_files/m4p)
add_subdirectory(source_files/miniz)
add_subdirectory(source_files/ymfmidi)
add_subdirectory(source_files/edge)
