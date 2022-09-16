//------------------------------------------------------------------------
//  WAD I/O
//------------------------------------------------------------------------
// 
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
// 
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __DEH_WAD_HDR__
#define __DEH_WAD_HDR__

#include "deh_system.h"

namespace Deh_Edge
{

namespace WAD
{
	void Startup(void);
	void Shutdown(void);

	void NewLump(const char *name);
	void AddData(const byte *data, int len);
	void Printf(const char *str, ...) GCCATTR((format (printf,1,2)));
	byte * FinishLump(int * length = 0);
}

}  // Deh_Edge

#endif /* __DEH_WAD_HDR__ */
