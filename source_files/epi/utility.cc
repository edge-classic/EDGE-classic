//----------------------------------------------------------------------------
//  EDGE Platform Interface Utility
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

#include "epi.h"
#include "utility.h"

#define STRBOX_ALIGN(x) x = ((x + 4) & ~3)

namespace epi
{
	// ---> strbox_c
	
	//
	// strbox_c Constructor
	//
	strbox_c::strbox_c()
	{
    	strs = NULL;
    	numstrs = 0;
    	data = NULL;
    	datasize = 0;
	}
	
	//
	// strbox_c Copy constructor
	//
    strbox_c::strbox_c(strbox_c &rhs)
    {
    	Copy(rhs);
    }
    
    //
    // strbox_c Destructor
    //	
    strbox_c::~strbox_c()
    {
    	Clear();
    }
       
    //
    // strbox_c::Clear()
    //
    void strbox_c::Clear()
    {
    	if (strs)
    		delete [] strs;
    		
    	if (data)
    		delete [] data;
    	
    	strs = NULL;
    	numstrs = 0;
    	data = NULL;
    }
    
    //
    // strbox_c::Copy()
    //
    void strbox_c::Copy(strbox_c &src)
    {
    	Clear();
    	
    	// Allocate new data
    	data = new char[src.datasize];
    	strs = new char*[src.numstrs];
    	
    	// Set appropriate sizes
    	datasize = src.datasize;
    	numstrs = src.numstrs;
    	
    	// Copy the data from source
    	memcpy(data, src.data, sizeof(char)*datasize);
    	memcpy(strs, src.strs, sizeof(char*)*numstrs);
    	
    	// Reset the pointers in the table to look at new data
    	int i;
    	int offset = data - src.data;
    	for (i=0; i<numstrs; i++)
    	{
    		if (strs[i])
                strs[i] += offset;
    	}
    }    	    
    
    //
    // strbox_c::Set()
    //
    // Loads the string box info from the string list
    //
   	void strbox_c::Set(strlist_c &src)
   	{
        array_iterator_c it;
        char *s;
        int pos, datapos;
        
        Clear();
        
        // Calculate space required
        for (datapos = 0, it = src.GetBaseIterator(); it.IsValid(); it++)
        {
            s = ITERATOR_TO_TYPE(it, char*);
            if (s)
        	{
        		datapos = datapos + strlen(s) + 1;  
        		STRBOX_ALIGN(datapos);  
        	}
        }
        
        // Allocate space
   		datasize = datapos;
   		data = new char[datapos];
   		memset(data, 0, datasize*sizeof(char));
   		
   		// Allocate pointer list
   		numstrs = src.GetSize();
   		strs = new char*[numstrs];
   		
   		// Load data and pointer list
        for (pos = 0, datapos = 0, it = src.GetBaseIterator(); 
        	 it.IsValid(); it++)
        {
            s = ITERATOR_TO_TYPE(it, char*);
            if (s)
        	{
        		strs[pos] = &data[datapos];
        		strcpy(strs[pos], s);
        		
        		datapos = datapos + strlen(s) + 1;  
        		STRBOX_ALIGN(datapos);	// Increment data pointer        		
        	}
        	else
        	{
        		strs[pos] = NULL;
        	}
        	
       		pos++;					// Next string pointer entry  
        }
        
        return;

   	}
   	
   	//
   	// strbox assignment operator
   	//
   	strbox_c& strbox_c::operator=(strbox_c &rhs)
   	{
   		if (&rhs != this)
   			Copy(rhs);
   			
   		return *this;
   	}
	
	// ---> strent_c
	
	//
	// strent_c::Set()
	//
	void strent_c::Set(const char *s)
	{
		clear();
		if (s)
		{
			data = new char[strlen(s)+1];
			strcpy(data, s);
		}
	}
	
	//
	// strent_c::Set()
	//
	void strent_c::Set(const char *s, int max)
	{
		clear();
		if (s)
		{
			const char *s2;
			int len;
			
			len = 0;
			s2 = s;
			while(*s2 && len<max)
			{
				len++;
				s2++;
			}
			
			data = new char[len+1];
			strncpy(data, s, len);
			data[len] ='\0';
		}
	}
	
    // ---> strlist_c 

    //
    // strlist_c Constructor
    //
    strlist_c::strlist_c() : array_c(sizeof(char*))
    {
    }
    
    //
    // strlist_c Copy constructor
    //
    strlist_c::strlist_c(strlist_c &rhs) : array_c(rhs.array_objsize)
    {
		Copy(rhs);
    }
    
    //
    // strlist Destructor
    //
    strlist_c::~strlist_c()
    {
        Clear();
    }

    //
    // strlist_c::CleanupObject()
    //
    void strlist_c::CleanupObject(void *obj)
    {
        char *s = *(char**)obj;
        if (s)
            delete [] s;
	}

	//
	// strlist_c::Copy()
	//
	void strlist_c::Copy(strlist_c &src)
	{
        // Enlarge to required size
        Size(src.array_entries);
        
        // Copy contents
        array_iterator_c it;
        for (it = src.GetBaseIterator(); it.IsValid(); it++)
            Insert(ITERATOR_TO_TYPE(it, char*));

		Trim();
	}
 
    //
    // int strlist_c::Find()
    // 
    int strlist_c::Find(const char* refname)
    {
        array_iterator_c it;
        char *s;
        
        for (it = GetBaseIterator(); it.IsValid(); it++)
        {
            s = ITERATOR_TO_TYPE(it, char*);
            if (s && strcmp(s, refname) == 0)
                return it.GetPos();
        }
        
        return -1;
    }

    //
    // int strlist_c::Insert()
    //
	int strlist_c::Insert(const char *s)
    {
    	char *copy_of_s;
    	if (s)
        {
        	copy_of_s = new char[strlen(s)+1];
        	strcpy(copy_of_s, s);
        }
        else
        {
        	copy_of_s = NULL;
        }
        
        return InsertObject((void*)&copy_of_s);
    }
    
    //
    // strlist_c::Set()
    //
    // Reads in data from a strbox_c object
    //
    void strlist_c::Set(strbox_c &src)
    {
    	Clear();
    	
    	// Check we haven't something to do
    	if (!src.GetSize())
    		return;

		//
		// Lets not fragment memory when we don't need to - allocate
		// the correct size of array now
		//
    	Size(src.GetSize());
    	
    	int i;
    	for (i=0; i<src.GetSize(); i++)
    		Insert(src[i]);
    }

	//
	// strlist_c assignment operator
	//
	strlist_c& strlist_c::operator=(strlist_c& rhs)
	{
		if (&rhs != this)
		{
			Clear();
			Copy(rhs);
		}

		return *this;
	}
	
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
