//------------------------------------------------------------------------
//  TEXT Strings
//------------------------------------------------------------------------
// 
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
// 
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __DEH_TEXT_HDR__
#define __DEH_TEXT_HDR__

namespace Deh_Edge
{

namespace TextStr
{
	void Init();
	void Shutdown();

	// these return true if the string was found.
	bool ReplaceString(const char *before, const char *after);
	bool ReplaceCheat(const char *deh_name, const char *str);
	bool ReplaceBexString(const char *bex_name, const char *after);
	void ReplaceBinaryString(int v166_index, const char *str);

	void AlterCheat(const char * new_val);

	const char *GetLDFForBex(const char *bex_name);

	void ConvertLDF(void);
}

}  // Deh_Edge

#endif /* __DEH_TEXT_HDR__ */
