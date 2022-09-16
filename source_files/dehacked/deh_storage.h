//------------------------------------------------------------------------
//  STORAGE for modifications
//------------------------------------------------------------------------
// 
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
// 
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __DEH_STORAGE_HDR__
#define __DEH_STORAGE_HDR__

namespace Deh_Edge
{

namespace Storage
{
	void Startup(void);
	void RememberMod(int *target, int value);
	void ApplyAll(void);
	void RestoreAll(void);
}

}  // Deh_Edge

#endif /* __DEH_STORAGE_HDR__ */
