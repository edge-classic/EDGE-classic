cmake_minimum_required(VERSION 3.2.2)

project(
  m4p
  LANGUAGES C
  VERSION 0.1.0
)

set(CMAKE_CC_FLAGS "-O2 -Wall")

add_library(
  m4p
  it2drivers/sb16_m.c
  it2drivers/sb16.c
  it2drivers/zerovol.c
  loaders/mmcmp/mmcmp.c
  loaders/it.c
  loaders/s3m.c
  m4p.c
  ft_tables.c
  it_d_rm.c
  it_m_eff.c
  it_music.c
  it_structs.c
  it_tables.c
  pmp_main.c
  pmp_mix.c
  pmplay.c
  snd_masm.c
)
