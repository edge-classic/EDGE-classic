##########################################
# m4p
##########################################

add_library(
  m4p
  m4p.c
  src/sb16_m.c
  src/sb16.c
  src/zerovol.c
  src/mmcmp.c
  src/it.c
  src/s3m.c
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

target_include_directories(m4p PUBLIC .)