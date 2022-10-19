//------------------------------------------------------------------------
//  RSCRIPT output
//------------------------------------------------------------------------
// 
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
// 
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __DEH_RSCRIPT_HDR__
#define __DEH_RSCRIPT_HDR__

namespace Deh_Edge
{

namespace Rscript
{
	void Init();
	void Shutdown();

	void MarkBossDeath(int mt_num);

	void ConvertRAD();
}

}  // Deh_Edge

#endif /* __DEH_RSCRIPT_HDR__ */
