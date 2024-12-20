##########################################
# libRAD
##########################################

set (LIBRAD_SOURCES opal.cpp)

if (EDGE_RAD_SUPPORT)
  set (LIBRAD_SOURCES ${LIBRAD_SOURCES} radplay.cpp)
endif()

add_library(
  libRAD
  ${LIBRAD_SOURCES}
)

target_include_directories(libRAD PUBLIC ./)