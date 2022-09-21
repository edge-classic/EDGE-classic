//----------------------------------------------------------------------------
//  EDGE Platform Interface Utility Header
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2008  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
#ifndef __EPI_UTIL__
#define __EPI_UTIL__

#include "arrays.h"

namespace epi
{
	// Forward declaration
	class strlist_c;

    // String Box
    class strbox_c
    {
    public:
    	strbox_c();
    	strbox_c(strbox_c &rhs);
    	~strbox_c();
    	
    private:
    	char **strs;
    	int numstrs;
    	
    	char *data;
    	int datasize;
    	
    	void Copy(strbox_c &src);
    	
    public:
    	void Clear();
    	int GetSize() const { return numstrs; }
    	void Set(strlist_c &src);

    	strbox_c& operator=(strbox_c &rhs);
    	char* operator[](int idx) const { return strs[idx]; }
    };
    
    // String list
    class strlist_c : public array_c
    {
    public:
        strlist_c();
        strlist_c(strlist_c &rhs);
        ~strlist_c();
        
    private:
        void CleanupObject(void *obj);
		void Copy(strlist_c &src);
        
    public:
        void Delete(int idx) { RemoveObject(idx); }
	    int Find(const char* refname);
	    int GetSize() const { return array_entries; }
	    int Insert(const char *s);
    	void Set(strbox_c &src);
    	
		strlist_c& operator=(strlist_c &rhs);
	    char* operator[](int idx) const { return *(char**)FetchObject(idx); } 
    };
    
};




//
// Z_Clear
//
// Clears memory to zero.
//
#define Z_Clear(ptr, type, num)  \
	memset((void *)(ptr), ((ptr) - ((type *)(ptr))), (num) * sizeof(type))


//
// Z_StrNCpy
//
// Copies up to max characters of src into dest, and then applies a
// terminating zero (so dest must hold at least max+1 characters).
// The terminating zero is always applied (there is no reason not to)
//
#define Z_StrNCpy(dest, src, max) \
	(void)(strncpy((dest), (src), (max)), (dest)[(max)] = 0)


#endif /* __EPI_UTIL__ */


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
