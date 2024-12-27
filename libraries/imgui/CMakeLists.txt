
set (IMGUI_SOURCES
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui.cpp
)

add_library(imgui ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC ./)