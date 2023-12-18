# I use this with w64devkit; not sure if it is useful for other MinGW builds - Dasho

# the name of the target operating system
set(CMAKE_SYSTEM_NAME Windows)

set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_EXE_LINKER_FLAGS
    "-static -lmingw32 -L${CMAKE_SOURCE_DIR}/libraries/sdl2/lib/mingw/x86 -lSDL2main -lSDL2.dll -mwindows"
)