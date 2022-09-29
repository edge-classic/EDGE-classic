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

#ifndef __DEH_SOUNDS_HDR__
#define __DEH_SOUNDS_HDR__

#include <string>

namespace Deh_Edge
{

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

	// MBF sounds:
	sfx_dgsit = NUMSFX,
	sfx_dgatk,
	sfx_dgact,
	sfx_dgdth,
	sfx_dgpain,

	NUMSFX_MBF,

	// other source ports:
	sfx_secret = NUMSFX_MBF,
	sfx_gibdth,
	sfx_scrsht,

	// DEHEXTRA: 200 additional sounds
	sfx_fre000 = 500,
	sfx_fre199 = 699,

	NUMSFX_DEHEXTRA
}
sfxtype_e;


namespace Sounds
{
	void Init();
	void Shutdown();

	// this returns true if the string was found.
	bool ReplaceSound(const char *before, const char *after);

	void AlterBexSound(const char *new_val);

	void MarkSound(int s_num);
	void AlterSound(int new_val);

	const char *GetSound(int sound_id);

	void ConvertSFX(void);
}

}  // Deh_Edge

#endif  /* __DEH_SOUNDS_HDR__ */
