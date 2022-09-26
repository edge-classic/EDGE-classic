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

#ifndef __DEH_BUFFER_HDR__
#define __DEH_BUFFER_HDR__

namespace Deh_Edge
{

class input_buffer_c
{
public:
	input_buffer_c(const char *_data, int _length);
	~input_buffer_c();

	bool eof();
	bool error();
	int  read(void *buf, int count);
	int  getch();
	void ungetch(int c);
	bool isBinary() const;

private:
	const char *data;  // base pointer
	const char *ptr;   // current read pointer

	int length;
};

}  // Deh_Edge

#endif /* __DEH_BUFFER_HDR__ */
