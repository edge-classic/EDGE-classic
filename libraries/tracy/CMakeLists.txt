##########################################
# edge_tracy
##########################################

add_library(edge_tracy INTERFACE)

if (EDGE_PROFILING)
  include(FetchContent)
  FetchContent_Declare (
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG c556831ddc6fe26d2fce01c14c97205a9dad46d5
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
  )
  option (TRACY_ON_DEMAND "" OFF)
  option (TRACY_CALLSTACK "" ON)
  FetchContent_MakeAvailable (tracy)  
endif()

target_include_directories(edge_tracy INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR})