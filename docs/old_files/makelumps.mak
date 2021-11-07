
all:	raw/ddfanim.lmp raw/ddfatk.lmp raw/ddfcolm.lmp \
	raw/ddffont.lmp raw/ddfgame.lmp raw/ddfimage.lmp \
	raw/ddflang.lmp raw/ddflevl.lmp raw/ddfline.lmp \
	raw/ddfplay.lmp raw/ddfsect.lmp raw/ddfsfx.lmp \
	raw/ddfstyle.lmp raw/ddfswth.lmp raw/ddfthing.lmp \
	raw/ddfweap.lmp \
	raw/plutlang.lmp raw/tntlang.lmp \
	raw/rscript.lmp \
	raw/coaldef0.lmp raw/coalhud0.lmp 

raw/ddfanim.lmp:  doom/anims.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddfatk.lmp:   doom/attacks.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddfcolm.lmp:  doom/colmap.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddffont.lmp:  doom/fonts.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddfgame.lmp:  doom/games.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddfimage.lmp: doom/images.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddflang.lmp:  doom/language.ldf
	awk -f unix2dos.awk $^ > $@

raw/ddflevl.lmp: doom/levels.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddfline.lmp:  doom/lines.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddfplay.lmp:  doom/playlist.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddfsect.lmp:  doom/sectors.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddfsfx.lmp:   doom/sounds.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddfstyle.lmp: doom/styles.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddfswth.lmp:  doom/switch.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddfthing.lmp: doom/things.ddf
	awk -f unix2dos.awk $^ > $@

raw/ddfweap.lmp:  doom/weapons.ddf
	awk -f unix2dos.awk $^ > $@

raw/rscript.lmp:  doom/edge.scr
	awk -f unix2dos.awk $^ > $@

raw/plutlang.lmp: doom/lang_plut.ldf
	awk -f unix2dos.awk $^ > $@

raw/tntlang.lmp:  doom/lang_tnt.ldf
	awk -f unix2dos.awk $^ > $@

raw/coaldef0.lmp:  doom/coal_api.ec
	awk -f unix2dos.awk $^ > $@

raw/coalhud0.lmp:  doom/coal_hud.ec
	awk -f unix2dos.awk $^ > $@

## raw/edgever.lmp:  doom/edge_wad.txt
##	awk -f unix2dos.awk $^ > $@

.PHONY: all

#--- editor settings ------------
# vi:ts=8:sw=8:noexpandtab
