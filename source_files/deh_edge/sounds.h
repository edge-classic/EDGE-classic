//------------------------------------------------------------------------
//  SOUND Definitions
//------------------------------------------------------------------------
// 
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
// 
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __SOUNDS_HDR__
#define __SOUNDS_HDR__

namespace Deh_Edge
{

//
// MusicInfo struct.
//
typedef struct
{
    // up to 6-character name
    const char *orig_name;

	int ddf_num;

	// changed name (NULL if not modified).  Space for 6 non-NUL characters.
	char *new_name;
}
musicinfo_t;

//
// Identifiers for all music in game.
//

typedef enum
{
    mus_None,

    mus_e1m1,   mus_e1m2,   mus_e1m3,   mus_e1m4,   mus_e1m5,
    mus_e1m6,   mus_e1m7,   mus_e1m8,   mus_e1m9,   mus_e2m1,
    mus_e2m2,   mus_e2m3,   mus_e2m4,   mus_e2m5,   mus_e2m6,
    mus_e2m7,   mus_e2m8,   mus_e2m9,   mus_e3m1,   mus_e3m2,
    mus_e3m3,   mus_e3m4,   mus_e3m5,   mus_e3m6,   mus_e3m7,
    mus_e3m8,   mus_e3m9,

    mus_inter,  mus_intro,  mus_bunny,  mus_victor, mus_introa,
    mus_runnin, mus_stalks, mus_countd, mus_betwee, mus_doom,
    mus_the_da, mus_shawn,  mus_ddtblu, mus_in_cit, mus_dead,
    mus_stlks2, mus_theda2, mus_doom2,  mus_ddtbl2, mus_runni2,
    mus_dead2,  mus_stlks3, mus_romero, mus_shawn2, mus_messag,
    mus_count2, mus_ddtbl3, mus_ampie,  mus_theda3, mus_adrian,
    mus_messg2, mus_romer2, mus_tense,  mus_shawn3, mus_openin,
    mus_evil,   mus_ultima, mus_read_m, mus_dm2ttl, mus_dm2int,

    NUMMUSIC
}
musictype_e;

// the complete set of music
extern musicinfo_t S_music[NUMMUSIC];


//------------------------------------------------------------------------

//
// SoundFX struct.
//

typedef struct
{
    // up to 6-character name
    const char *orig_name;

    // Sfx singularity (only one at a time)
    int singularity;

    // Sfx priority
    int priority;

    // referenced sound if a link
    int link;

    // pitch if a link
    int pitch;

    // volume if a link
    int volume;

	// changed name (NULL if not modified).  Space for 6 non-NUL characters.
	char *new_name;
}
sfxinfo_t;

//
// Identifiers for all sfx in game.
//

typedef enum
{
    sfx_None,

    sfx_pistol, sfx_shotgn, sfx_sgcock, sfx_dshtgn, sfx_dbopn,
    sfx_dbcls,  sfx_dbload, sfx_plasma, sfx_bfg,    sfx_sawup,
    sfx_sawidl, sfx_sawful, sfx_sawhit, sfx_rlaunc, sfx_rxplod,
    sfx_firsht, sfx_firxpl, sfx_pstart, sfx_pstop,  sfx_doropn,
    sfx_dorcls, sfx_stnmov, sfx_swtchn, sfx_swtchx, sfx_plpain,
    sfx_dmpain, sfx_popain, sfx_vipain, sfx_mnpain, sfx_pepain,
    sfx_slop,   sfx_itemup, sfx_wpnup,  sfx_oof,    sfx_telept,
    sfx_posit1, sfx_posit2, sfx_posit3, sfx_bgsit1, sfx_bgsit2,
    sfx_sgtsit, sfx_cacsit, sfx_brssit, sfx_cybsit, sfx_spisit,
    sfx_bspsit, sfx_kntsit, sfx_vilsit, sfx_mansit, sfx_pesit,
    sfx_sklatk, sfx_sgtatk, sfx_skepch, sfx_vilatk, sfx_claw,
    sfx_skeswg, sfx_pldeth, sfx_pdiehi, sfx_podth1, sfx_podth2,
    sfx_podth3, sfx_bgdth1, sfx_bgdth2, sfx_sgtdth, sfx_cacdth,
    sfx_skldth, sfx_brsdth, sfx_cybdth, sfx_spidth, sfx_bspdth,
    sfx_vildth, sfx_kntdth, sfx_pedth,  sfx_skedth, sfx_posact,
    sfx_bgact,  sfx_dmact,  sfx_bspact, sfx_bspwlk, sfx_vilact,
    sfx_noway,  sfx_barexp, sfx_punch,  sfx_hoof,   sfx_metal,
    sfx_chgun,  sfx_tink,   sfx_bdopn,  sfx_bdcls,  sfx_itmbk,
    sfx_flame,  sfx_flamst, sfx_getpow, sfx_bospit, sfx_boscub,
    sfx_bossit, sfx_bospn,  sfx_bosdth, sfx_manatk, sfx_mandth,
    sfx_sssit,  sfx_ssdth,  sfx_keenpn, sfx_keendt, sfx_skeact,
    sfx_skesit, sfx_skeatk, sfx_radio,

    NUMSFX,

	// BOOM and MBF sounds:
#define sfx_dgsit  NUMSFX
	sfx_dgatk, sfx_dgact, sfx_dgdth, sfx_dgpain,

	NUMSFX_BEX
}
sfxtype_e;

// the complete set of sound effects
extern sfxinfo_t S_sfx[NUMSFX_BEX];


namespace Sounds
{
	void Startup(void);

	// this returns true if the string was found.
	bool ReplaceSound(const char *before, const char *after);
	bool ReplaceMusic(const char *before, const char *after);

	void AlterBexSound(const char *new_val);
	void AlterBexMusic(const char *new_val);

	void MarkSound(int s_num);
	void AlterSound(int new_val);

	const char *GetSound(int sound_id);

	void ConvertSFX(void);
	void ConvertMUS(void);
}

}  // Deh_Edge

#endif  /* __SOUNDS_HDR__ */
