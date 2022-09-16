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
	extern deh_container_c * dest_container;

	void NewLump(const char *name);
	void Printf(const char *str, ...) GCCATTR((format (printf,1,2)));
}

}  // Deh_Edge

#endif /* __DEH_WAD_HDR__ */
