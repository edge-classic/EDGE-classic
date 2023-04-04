##########################################
# gl4es
##########################################

if (EMSCRIPTEN)
    set(NOX11 ON)
    set(NO_GBM ON)
    set(NO_EGL ON)
    set(STATICLIB ON)
    set(NO_INIT_CONSTRUCTOR ON)
endif()


if(CMAKE_SYSTEM_NAME MATCHES "Windows" OR CMAKE_SYSTEM_NAME MATCHES "MSYS")
    set(NOX11 ON)
    set(NO_GBM ON)
    set(WIN32_PLATFORM ON)
    set(STATICLIB ON)
    set(NO_INIT_CONSTRUCTOR ON)
    if(CMAKE_C_COMPILER_ID MATCHES "MSVC" OR "x${CMAKE_C_SIMULATE_ID}" STREQUAL "xMSVC")
        set(WIN32_MSVC ON)  #msvc or icl or clang-cl
    endif()
else()
    set(NOX11 ON)
    set(NO_GBM ON)
    set(NO_EGL ON)
    set(STATICLIB ON)
    set(NO_INIT_CONSTRUCTOR ON)
endif()

# Pandora
if(PANDORA)
    add_definitions(-DPANDORA)
    add_definitions(-DTEXSTREAM)
    add_definitions(-mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp -ftree-vectorize -fsingle-precision-constant -ffast-math)
    set(NO_GBM ON)
endif()

if((NOT GBM) OR PANDORA)
    set(NO_GBM ON)
else()
    set(NO_GBM OFF)
endif()

set(DEFAULT_ES 2)

if(STATICLIB)
    add_definitions(-DSTATICLIB)
endif(STATICLIB)

#NOX11
if(NOX11)
    add_definitions(-DEGL_NO_X11)
    add_definitions(-DNOX11)
endif()

#NOEGL
if(NOEGL)
    add_definitions(-DNOEGL)
    add_definitions(-DNOX11)
endif()

if(NO_GBM)
    add_definitions(-DNO_GBM)
endif()

#NOX11
if(USE_CLOCK)
    add_definitions(-DUSE_CLOCK)
endif()

if(NO_LOADER)
    add_definitions(-DNO_LOADER)
endif()

if(NO_INIT_CONSTRUCTOR)
    add_definitions(-DNO_INIT_CONSTRUCTOR)
endif()

if(GLX_STUBS)
    add_definitions(-DGLX_STUBS)
endif()

#DEFAULT_ES=2
if(DEFAULT_ES EQUAL 2)
    add_definitions(-DDEFAULT_ES=2)
endif()
if(DEFAULT_ES EQUAL 1)
    add_definitions(-DDEFAULT_ES=1)
endif()

if(NOT WIN32_MSVC)
    add_definitions(-std=gnu11 -funwind-tables)
    if(NOT WIN32_PLATFORM)
       add_definitions(-fvisibility=hidden)
    else()
       set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--exclude-all-symbols,--kill-at")
    endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_definitions(-Wno-deprecated-declarations) #strdup
    add_definitions(-Wno-unused-function -Wno-unused-variable -Wno-dangling-else)
    add_definitions(-Wno-implicit-const-int-float-conversion)
    add_definitions(-Wno-visibility)
else()
    if(CMAKE_C_COMPILER_ID MATCHES "Intel")
        add_definitions(-wd1786 -wd589 -wd537 -wd118 -wd2722)
    else()
        add_definitions(-wd4996 -wd4244 -wd4267 -wd4098 -wd4018)
    endif()
    add_definitions("-Dinline=__inline" "-D__func__=__FUNCTION__") # for VC<=13
endif()

if (CMAKE_CC_COMPILER_ID MATCHES "Clang" OR CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    add_definitions(-Wno-pointer-sign -Wno-dangling-else)
endif()

include_directories(include)
add_subdirectory(src)

