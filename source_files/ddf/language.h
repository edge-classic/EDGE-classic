//----------------------------------------------------------------------------
//  EDGE Language Definitions
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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

#ifndef __DDF_LANG_H__
#define __DDF_LANG_H__

#include "utility.h"

#include "types.h"

class ddf_bi_lang_c;

// ------------------------------------------------------------------
// ---------------------------LANGUAGES------------------------------
// ------------------------------------------------------------------

class language_c
{
public:
	language_c();
	~language_c();

private:
	epi::strbox_c choices;
	epi::strbox_c refs;
	epi::strbox_c *values;

	int current;

	int Find(const char *ref);

	void Recompile(void);

public:
	void Clear();
	
	int GetChoiceCount() { return choices.GetSize(); }
	int GetChoice() { return current; }
	
	const char* GetName(int idx = -1);
	bool IsValidRef(const char *refname);
	
	void LoadLanguageChoices(epi::strlist_c& langnames);
	void LoadLanguageReferences(epi::strlist_c& _refs);
	void LoadLanguageValues(int lang, epi::strlist_c& _values);
				
	bool Select(const char *name);
	bool Select(int idx);

	const char* operator[](const char *refname);
	const char* operator[](const std::string& refname)
	{
		return (*this)[refname.c_str()];
	}

	ddf_bi_lang_c* buildinfo;
	
	void AddOrReplace(const char *ref, const char *value);

	//void Dump(void);
};

extern language_c language;   // -ACB- 2004/06/27 Implemented

void DDF_ReadLangs(const std::string& data);

#endif /* __DDF_LANG_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
