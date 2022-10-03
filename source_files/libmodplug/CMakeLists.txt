cmake_minimum_required(VERSION 3.12..3.20)

project(libmodplug)

set(VERSION "0.8.9.1")

include (CheckFunctionExists)
include (CheckIncludeFile)
include (CheckCCompilerFlag)
include (CheckCSourceCompiles)
include (TestBigEndian)

TEST_BIG_ENDIAN(WORDS_BIGENDIAN)
if(WORDS_BIGENDIAN)
  add_compile_definitions(WORDS_BIGENDIAN 1)
endif()

set(CMAKE_CXX_FLAGS "-O2 -Wall")
if(WIN32 AND NOT MSYS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /EHs")
  set(CMAKE_EXE_LINKER_FLAGS "/SUBSYSTEM:WINDOWS")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -fno-strict-aliasing")
endif()

include_directories(AFTER
  src
  src/libmodplug
  ${PROJECT_BINARY_DIR}
)

if(UNIX AND NOT APPLE)
  find_library(MATH_LIB m)
  if(MATH_LIB)
    set(CMAKE_REQUIRED_LIBRARIES m)
  endif()
endif()

check_include_file("stdint.h" HAVE_STDINT_H)
if (HAVE_STDINT_H)
  add_compile_definitions(HAVE_STDINT_H)
endif()

check_include_file("strings.h" HAVE_STRINGS_H)
if (HAVE_STRINGS_H)
  add_compile_definitions(HAVE_STRINGS_H)
endif()

check_function_exists("sinf" HAVE_SINF)
if(HAVE_SINF)
  add_compile_definitions(HAVE_SINF)
endif()

add_library(modplug STATIC
  src/libmodplug/it_defs.h
  src/libmodplug/sndfile.h
  src/libmodplug/stdafx.h

  src/fastmix.cpp
  src/load_669.cpp
  src/load_amf.cpp
  src/load_ams.cpp
  src/load_dbm.cpp
  src/load_dmf.cpp
  src/load_dsm.cpp
  src/load_far.cpp
  src/load_it.cpp
  src/load_j2b.cpp
  src/load_mdl.cpp
  src/load_med.cpp
  src/load_mod.cpp
  src/load_mt2.cpp
  src/load_mtm.cpp
  src/load_okt.cpp
  src/load_psm.cpp
  src/load_ptm.cpp
  src/load_s3m.cpp
  src/load_stm.cpp
  src/load_ult.cpp
  src/load_umx.cpp
  src/load_wav.cpp
  src/load_xm.cpp
  src/mmcmp.cpp
  src/modplug.cpp
  src/modplug.h
  src/snd_dsp.cpp
  src/snd_flt.cpp
  src/snd_fx.cpp
  src/sndfile.cpp
  src/sndmix.cpp
  src/tables.h
)
