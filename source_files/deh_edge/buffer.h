//------------------------------------------------------------------------
//  BUFFER for Parsing
//------------------------------------------------------------------------
// 
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
// 
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __BUFFER_HDR__
#define __BUFFER_HDR__

namespace Deh_Edge
{

class parse_buffer_api
{
public:
	parse_buffer_api()  { }
	virtual ~parse_buffer_api() { }

	virtual bool eof() = 0;
	virtual bool error() = 0;
	virtual int  read(void *buf, int count) = 0; 
	virtual int  getch() = 0;
	virtual void ungetch(int c) = 0;

	virtual bool isBinary() const = 0;
	virtual void showProgress() = 0;
};

namespace Buffer
{
	parse_buffer_api *OpenFile(const char *filename);
	// returns NULL if could not open the file, and the error
	// message will have been stored (via SetErrorMsg).

	parse_buffer_api *OpenLump(const char *data, int length);
}

}  // Deh_Edge

#endif /* __BUFFER_HDR__ */
