##########################################
# m4p
##########################################

add_library(
  m4p
  src/it2drivers/sb16_m.c
  src/it2drivers/sb16.c
  src/it2drivers/zerovol.c
  src/loaders/mmcmp/mmcmp.c
  src/loaders/it.c
  src/loaders/s3m.c
  src/m4p.c
  src/ft_tables.c
  src/it_d_rm.c
  src/it_m_eff.c
  src/it_music.c
  src/it_tables.c
  src/pmp_main.c
  src/pmp_mix.c
  src/pmplay.c
  src/snd_masm.c
)

target_include_directories(m4p PUBLIC ./src)