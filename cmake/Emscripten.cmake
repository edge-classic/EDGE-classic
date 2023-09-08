
# https://github.com/emscripten-core/emscripten/blob/main/src/settings.js

set(EDGE_GL_ES2 ON)

set(EDGE_EMSC_COMMON_FLAGS "-sUSE_SDL=2")
set(EDGE_EMSC_COMPILER_FLAGS "-DEDGE_WEB=1")
set(EDGE_EMSC_LINKER_FLAGS "-sENVIRONMENT=web -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=512Mb -sERROR_ON_UNDEFINED_SYMBOLS=1 -sMAX_WEBGL_VERSION=2 -sFULL_ES2=1 -sGL_MAX_TEMP_BUFFER_SIZE=16777216 -sLZ4=1 -lidbfs.js -sEXPORT_ES6=1 -sUSE_ES6_IMPORT_META=0 -sMODULARIZE=1 -sEXPORT_NAME=\"createEdgeModule\"")

# Enable sourcemap support
set(EDGE_EMSC_LINKER_FLAGS "${EDGE_EMSC_LINKER_FLAGS} -gsource-map --source-map-base=/")

# force file system, see: https://github.com/emscripten-core/emscripten/blob/main/tools/file_packager.py
set(EMPACKAGER "${EMSCRIPTEN_ROOT_PATH}/tools/file_packager.py")
set(EDGE_EMSC_LINKER_FLAGS "${EDGE_EMSC_LINKER_FLAGS} -sFORCE_FILESYSTEM=1")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(EDGE_EMSC_COMPILER_FLAGS "${EDGE_EMSC_COMPILER_FLAGS} -O0")

  # Add -sSAFE_HEAP=1 or -sSAFE_HEAP=2 to test heap and alignment, though slows things down a lot
  set(EDGE_EMSC_LINKER_FLAGS "${EDGE_EMSC_LINKER_FLAGS} -sASSERTIONS=1")
else()
  #note disabled -flto due to https://github.com/emscripten-core/emscripten/issues/19781
  #set(EDGE_EMSC_COMMON_FLAGS "${EDGE_EMSC_COMMON_FLAGS} -flto")
  set(EDGE_EMSC_COMPILER_FLAGS "${EDGE_EMSC_COMPILER_FLAGS} -O3")
  set(EDGE_EMSC_LINKER_FLAGS "${EDGE_EMSC_LINKER_FLAGS} -sASYNCIFY=1")
endif()

# message("${EDGE_EMSC_COMMON_FLAGS} ${EDGE_EMSC_COMPILER_FLAGS}")
# message("${EDGE_EMSC_COMMON_FLAGS} ${EDGE_EMSC_LINKER_FLAGS}")

add_compile_options("SHELL: ${EDGE_EMSC_COMMON_FLAGS} ${EDGE_EMSC_COMPILER_FLAGS}")
add_link_options("SHELL: ${EDGE_EMSC_COMMON_FLAGS} ${EDGE_EMSC_LINKER_FLAGS}")
