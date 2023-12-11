set(SDL2_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")

# Support both 32 and 64 bit builds
if (MINGW)
  if (${CMAKE_SIZEOF_VOID_P} MATCHES 8)
    set(SDL2_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/lib/mingw/x64/libSDL2.dll.a;${CMAKE_CURRENT_LIST_DIR}/lib/mingw/x64/libSDL2main.a")
  else ()
    set(SDL2_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/lib/mingw/x86/libSDL2.dll.a;${CMAKE_CURRENT_LIST_DIR}/lib/mingw/x86/libSDL2main.a")
  endif ()
else()
  if (${CMAKE_SIZEOF_VOID_P} MATCHES 8)
    set(SDL2_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/lib/msvc/x64/SDL2.lib;${CMAKE_CURRENT_LIST_DIR}/lib/msvc/x64/SDL2main.lib")
  else ()
    set(SDL2_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/lib/msvc/x86/SDL2.lib;${CMAKE_CURRENT_LIST_DIR}/lib/msvc/x86/SDL2main.lib")
  endif ()
endif()

string(STRIP "${SDL2_LIBRARIES}" SDL2_LIBRARIES)