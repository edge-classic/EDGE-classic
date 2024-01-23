##########################################
# fluidlite
##########################################

add_library(fluidlite
    src/fluid_chan.c
    src/fluid_chorus.c
    src/fluid_conv.c
    src/fluid_defsfont.c
    src/fluid_dsp_float.c
    src/fluid_gen.c
    src/fluid_hash.c
    src/fluid_list.c
    src/fluid_mod.c
    src/fluid_ramsfont.c
    src/fluid_rev.c
    src/fluid_settings.c
    src/fluid_synth.c
    src/fluid_sys.c
    src/fluid_tuning.c
    src/fluid_voice.c
)

target_include_directories(fluidlite PRIVATE src)
target_include_directories(fluidlite PRIVATE include)
