##########################################
# edge_tracy
##########################################

add_library(edge_tracy INTERFACE)

if (EDGE_PROFILING)
  add_definitions("-DEDGE_PROFILING" "-DTRACY_ENABLE")
  include(FetchContent)
  FetchContent_Declare (
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
  )
  option (TRACY_ON_DEMAND "" OFF)
  option (TRACY_CALLSTACK "" ON)
  FetchContent_MakeAvailable (tracy)  
endif()

target_include_directories(edge_tracy INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR})