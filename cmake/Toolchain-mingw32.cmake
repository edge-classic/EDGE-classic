# the name of the target operating system
set(CMAKE_SYSTEM_NAME Windows)

# which tools to use
set(CMAKE_C_COMPILER /usr/bin/i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER /usr/bin/i686-w64-mingw32-windres)
# these next two are defined because when doing the compiler check the linker flags screw it up
# not sure if there is a better way to handle this - Dasho
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_EXE_LINKER_FLAGS
    "-static -lmingw32 -L${CMAKE_SOURCE_DIR}/libraries/sdl2/lib/mingw/x86 -lSDL2main -lSDL2.dll -mwindows"
)

# here is where the target environment located
set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)

# adjust the default behaviour of the FIND_XXX() commands: search programs in
# the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# search headers and libraries in the target environment,
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)