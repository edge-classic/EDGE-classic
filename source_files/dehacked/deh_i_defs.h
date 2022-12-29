//----------------------------------------------------------------------------
//  System Specific Header
//----------------------------------------------------------------------------
// 
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
// 
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __DEH_SYSTEM_SPECIFIC_DEFS__
#define __DEH_SYSTEM_SPECIFIC_DEFS__

// COMMON STUFF...
#define FLOAT_IEEE_754
namespace Deh_Edge
{
	typedef unsigned char byte;
}

#include <cstddef>

// Windows
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)

#define STRICT
#define _WINDOWS
#define WIN32_LEAN_AND_MEAN
#ifndef WIN32
#define WIN32
#endif

// Access() define values. Nicked from DJGPP's <unistd.h>
#ifndef R_OK
#define R_OK    0x02
#define W_OK    0x04
#endif

#endif

#endif /*__DEH_SYSTEM_SPECIFIC_DEFS__*/
