project(
  libxmp-lite
  LANGUAGES C
  VERSION 0.1.0
)

set(CMAKE_CC_FLAGS "-O2 -DINLINE_G=inline -Wall")
if(WIN32 AND NOT MSYS)
  set(CMAKE_CC_FLAGS "${CMAKE_CC_FLAGS} /fp:fast /EHs")
else()
  set(CMAKE_CC_FLAGS "${CMAKE_CC_FLAGS} -ffast-math -fno-strict-aliasing")
endif()

add_compile_definitions(LIBXMP_CORE_PLAYER)
add_compile_definitions(LIBXMP_NO_DEPACKERS)

if(WIN32 AND NOT MSYS)
  add_compile_definitions(USE_LIBXMP_SNPRINTF)
endif()

add_library(
  libxmp-lite
  common.c
  control.c
  dataio.c
  effects.c
  filter.c
  format.c
  hio.c
  it_load.c
  itsex.c
  lfo.c
  load.c
  load_helpers.c
  md5.c
  memio.c
  misc.c
  mix_all.c
  mixer.c
  mod_load.c
  period.c
  player.c
  read_event.c
  s3m_load.c
  sample.c
  scan.c
  smix.c
  virtual.c
  win32.c
  xm_load.c  
)