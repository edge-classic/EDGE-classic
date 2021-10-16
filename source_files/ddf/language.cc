//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Language handling settings)
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
//
// Language handling Setup and Parser Code
//
// 1998/10/29 -KM- Allow cmd line selection of language.
//
// This is somewhat different to most DDF reading files. In order to read the
// language specific strings, it uses the format:
//
// <RefName>=<String>;
//
// as opposed to the normal entry, which should be:
//
// [<Refname>]
// STRING=<string>;
//
// also the file suffix is LDF (Language Def File), this is to avoid confusion with
// the oridnary DDF files. The default file is DEFAULT.LDF, which can be subbed by
// using -lang <NameOfLangFile>.
//

#include "local.h"

#include "language.h"

// ---> ddf buildinfo for language class
class ddf_bi_lang_c
{
public:
	ddf_bi_lang_c() 
	{
		treehead = NULL; 
	}
	
	~ddf_bi_lang_c() 
	{ 
		DeleteLangNodes(); 
	}
	
	struct langentry_s
	{
		langentry_s()
		{
			// Binary tree for sorting
			left = NULL;
			right = NULL;
			
			// Next entry with matching ref
			lang_next = NULL;
			
			// Linked list
			prev = NULL;
			next = NULL;
		}
		
		int lang;
	
		epi::strent_c ref;
		epi::strent_c value;	
		
		// A linked list + binary tree is wasteful, but speed efficent and
		// avoids stack overflow issues that may occur
		struct langentry_s* left;
		struct langentry_s* right;
		
		struct langentry_s* prev;
		struct langentry_s* next;

		// Next entry 
		struct langentry_s* lang_next;		
	};
	
private:
	langentry_s *treehead;
	 
public:
	epi::strlist_c langnames;
	int currlang;
	
	epi::strlist_c comp_langrefs;   // Compiled language references
	epi::strlist_c comp_langvalues; // Compiled language values (1 per lang)
	
	// LANGUAGE ENTRY NODE HANDLING
	
	void AddLangNode(const char *_ref, const char *value)
	{
		langentry_s *currnode, *node, *newnode;

		int cmp;
		enum { CREATE_HEAD, TO_LEFT, TO_RIGHT, ADD, REPLACE } act;

		act = CREATE_HEAD;
		currnode = NULL;
		node = treehead;

		while (node && act != REPLACE)
		{
			currnode = node;
			
			if (act != ADD)
				cmp = stricmp(_ref, node->ref);
			else
				cmp = 0;	// Has to be a match if the last act was ADD
					
			if (cmp == 0)
			{
				//
				// Check to see if the language id matches, if it does then
				// replace the entry, else add the node.
				//
				if (node->lang == currlang)
				{
					act = REPLACE;
				}
				else
				{
					act = ADD;
					node = node->lang_next;
				}
			} 
			else if (cmp < 0)
			{
				act = TO_LEFT;
				node = node->left;
			}
			else if (cmp > 0)
			{
				act = TO_RIGHT;
				node = node->right;
			}
		}
	
		// Handle replace seperately, since we not creating anything
		if (act == REPLACE)
		{
			node->value.Set(value);
			return;
		}
	
		newnode = new langentry_s;

		if (act != ADD) { newnode->ref.Set(_ref); }

		newnode->value.Set(value);
		newnode->lang = currlang;
		
		switch (act)
		{
			case CREATE_HEAD:	
			{ 
				treehead = newnode; 
				break;
			}
			
			case TO_LEFT:		
			{ 
				// Update the binary tree
				currnode->left = newnode;
				
				// Update the linked list (Insert behind current node)
				if (currnode->prev) 
				{ 
					currnode->prev->next = newnode; 
					newnode->prev = currnode->prev;
				}
				
				currnode->prev = newnode;
				newnode->next = currnode; 
				break;
			}
			
			case TO_RIGHT:  	
			{ 
				// Update the binary tree
				currnode->right = newnode; 

				// Update the linked list (Insert infront of current node)
				if (currnode->next) 
				{ 
					currnode->next->prev = newnode; 
					newnode->next = currnode->next;
				}
				
				currnode->next = newnode;
				newnode->prev = currnode; 
				break;
			}
			
			case ADD:  		
			{ 
				currnode->lang_next = newnode; 
				break;
			}
			
			default: 			
			{ 
				break; /* FIXME!! Throw error */ 
			}
		}
	}
	
	void DeleteLangNodes()
	{
		if (!treehead)
			return;
		
		// The node head is the head for the binary tree, so
		// we have to find the head by going backwards from
		// the treehead
		
		langentry_s *currnode, *savenode;

		currnode = treehead;
		while (currnode->prev)
			currnode = currnode->prev;
			
		while (currnode)
		{
			savenode = currnode->next;
			delete currnode;
			currnode = savenode;
		}
	}
	
	// LANGUAGES
	
	//
	// AddLanguage()
	//
	// Returns if the language already exists
	//
	bool AddLanguage(const char *name)
	{
		int langsel = -1;
		
		// Look for an existing language entry if one exists
		{
			epi::array_iterator_c it;

			for (it = langnames.GetBaseIterator(); it.IsValid(); it++)
			{
				char *it2 = ITERATOR_TO_TYPE(it, char*);

				if (stricmp(it2, name) == 0)
				{
					langsel = it.GetPos();
					break;
				}
			}
		}
		
		// Setup the current language index, adding the new entry if needs be
		if (langsel < 0)
		{
			langnames.Insert(name);

			currlang = langnames.GetSize() - 1;
		}
		else
		{
			currlang = langsel;
		}
		
		
		return (langsel < 0);
	}
	
	// LIST COMPILING
			
	void CompileLanguageReferences()
	{
		comp_langrefs.Clear();
		
		if (!treehead)
			return;					// Nothing to do

		langentry_s *currnode, *head;
		int count;
		
		currnode = treehead;
		while (currnode->prev)
			currnode = currnode->prev;
			
		head = currnode;
			
		// Count the entries so strlist_c does not over-fragment memory
		count = 0;
		while (currnode)
		{
			currnode = currnode->next;
			count++;
		}
		
		comp_langrefs.Size(count);
		
		// Add entries
		for (currnode = head; currnode; currnode = currnode->next)
			comp_langrefs.Insert(currnode->ref);
		
		return;		
	}
	
	void CompileLanguageValues(int lang)
	{
		SYS_ASSERT(lang >=0 && lang <= langnames.GetSize());
			
		comp_langvalues.Clear();
		
		if (!treehead)
			return;					// Nothing to do

		langentry_s *currnode, *head, *langnode;
		int count;
		
		// Find me head, son!
		currnode = treehead;
		while (currnode->prev)
			currnode = currnode->prev;
			
		head = currnode;
			
		// Count the entries so strlist_c does not over-fragment memory
		count = 0;
		while (currnode)
		{
			currnode = currnode->next;
			count++;
		}
		
		comp_langvalues.Size(count);
		
		// Add entries
		for (currnode = head; currnode; currnode = currnode->next)
		{
			langnode = currnode;
			while (langnode && langnode->lang != lang)
				langnode = langnode->lang_next;
				
			if (langnode)
				comp_langvalues.Insert(langnode->value);
			else	
				comp_langvalues.Insert(NULL);
		}
		
		return;
	}
};

// Globals
language_c language;	// -ACB- 2004/07/28 Languages instance

// Locals
ddf_bi_lang_c* lang_buildinfo;

//
//  DDF PARSING ROUTINES
//
static void LanguageStartEntry(const char *name, bool extend)
{
	if (!name || !name[0])
	{
		DDF_WarnError("New language entry is missing a name!");
		name = "DEAD_LANGUAGE";
	}

	// Note: extension is the norm for LANGUAGES.LDF

	lang_buildinfo->AddLanguage(name);
}


static void LanguageParseField(const char *field, const char *contents,
    int index, bool is_last)
{
#if (DEBUG_DDF)  
	I_Debugf("LANGUAGE_PARSE: %s = %s;\n", field, contents);
#endif

	if (! is_last)
	{
		DDF_WarnError("Unexpected comma `,' in LANGUAGE.LDF\n");
		return;
	}

	lang_buildinfo->AddLangNode(field, contents);
}

static void LanguageFinishEntry(void)
{
	/* ... */
}

static void LanguageClearAll(void)
{
	// safe to delete all language entries
	language.Clear();
}


bool DDF_ReadLangs(void *data, int size)
{
	readinfo_t languages;

	languages.memfile = (char*)data;
	languages.memsize = size;
	languages.tag = "LANGUAGES";
	languages.entries_per_dot = 1;

	if (languages.memfile)
	{
		languages.message = NULL;
		languages.filename = NULL;
		languages.lumpname = "DDFLANG";
	}
	else
	{
		languages.message = "DDF_InitLanguage";
		languages.filename = "language.ldf";
		languages.lumpname = NULL;
	}

	languages.start_entry  = LanguageStartEntry;
	languages.parse_field  = LanguageParseField;
	languages.finish_entry = LanguageFinishEntry;
	languages.clear_all    = LanguageClearAll;

	return DDF_MainReadFile(&languages);
}

void DDF_LanguageInit(void)
{
	// FIXME! Copy any existing entries into the new buildinfo

	// Clear any existing
	language.Clear();

	// Create a build info object
	lang_buildinfo = new ddf_bi_lang_c;
}

void DDF_LanguageCleanUp(void)
{
	// Convert build info into the language structure
	int langcount = lang_buildinfo->langnames.GetSize();
	if (langcount == 0)
		I_Error("Missing languages !\n");
	
	// Load the choice of languages
	language.LoadLanguageChoices(lang_buildinfo->langnames);
	
	// Load the reference table
	lang_buildinfo->CompileLanguageReferences();
	language.LoadLanguageReferences(lang_buildinfo->comp_langrefs);
	
	// Load the value table for each one of the languages
	int i;
	for (i=0; i<langcount; i++)
	{
		lang_buildinfo->CompileLanguageValues(i);
		language.LoadLanguageValues(i, lang_buildinfo->comp_langvalues);
	}
	
	//language.Dump();
	//exit(1);
	
	// Dispose of the data
	delete lang_buildinfo;
}

//
// language_c Constructor
//
language_c::language_c()
{
	current = -1;
}

//
// language_c Destructor
//
language_c::~language_c()
{
	Clear();
}

//
// language_c::Clear()
//
// Clear the contents
//
void language_c::Clear()
{
	choices.Clear();
	refs.Clear();
	
	if (values)
	{
		delete [] values;
		values = NULL;
	}
	
	current = -1;
}

//
// int language_c::Find(const char* ref)
//
int language_c::Find(const char *name)
{
	if (!values || !name)
		return -1;

	// TODO Optimise search - this list is sorted in order

	for (int i = refs.GetSize()-1; i >= 0; i--)
		if (stricmp(refs[i], name) == 0)
			return i;

	return -1; // not found
}

//
// const char* language_c::GetName()
//	
// Returns the current name if idx is isvalid, can be
// NULL if the choices table has not been setup
//
const char* language_c::GetName(int idx)
{
	if (idx < 0)
		idx = current;
		
	if (idx < 0 || idx >= choices.GetSize())
		return NULL; 
	
	return choices[idx];
}

//
// bool language_c::IsValidRef()
//
bool language_c::IsValidRef(const char *refname)
{
	return (Find(refname)>=0);
}

//
// language_c::LoadLanguageChoices()
//	
void language_c::LoadLanguageChoices(epi::strlist_c& _langnames)
{
	choices.Set(_langnames);
}

//
// language_c::LoadLanguageReferences()
//
void language_c::LoadLanguageReferences(epi::strlist_c& _refs)
{
	if (values)
		delete [] values;
	
	refs.Set(_refs);
	values = new epi::strbox_c[refs.GetSize()];
	
	return;
}

//
// language_c::LoadLanguageValues()
//
void language_c::LoadLanguageValues(int lang, epi::strlist_c& _values)
{
	if (_values.GetSize() != refs.GetSize())
		return;	// FIXME!! Throw error
		
	if (lang < 0 || lang >= choices.GetSize())
		return;	// FIXME!! Throw error
		
	values[lang].Set(_values);
	
	return;
}

//
// language_c::Select() Named Select
//
bool language_c::Select(const char *name)
{
	int i, max;
	
	for(i=0, max=choices.GetSize(); i<max; i++)
	{
		if (! DDF_CompareName(name, choices[i]))
		{
			current = i;
			return true;
		}
	}
	
	// FIXME!! Throw error
	return false;
}

//
// language_c::Select() Index Select
//
bool language_c::Select(int idx)
{
	if (idx < 0 || idx >= choices.GetSize())
		return false;	// FIXME!! Throw error
		
	current = idx;
	return true;
}

//
// const char* language_c::operator[]()
//	
const char* language_c::operator[](const char *refname)
{
	int idx;
	
	idx = Find(refname);
	if (idx>=0)
	{
		if (current < 0 || current >= choices.GetSize())
			return refname;  // -AJA- preserve old behaviour
		
		char *s = values[current][idx];
		if (s != NULL)
			return s;
			
		// Look through other language definitions is one does not exist
		int i, max;
		for (i=0, max=choices.GetSize(); i<max; i++)
		{
			if (i != current)
			{
				char *s = values[i][idx];
				if (s != NULL)
					return values[i][idx];
			}
		}
	}
	
	return refname;  // -AJA- preserve old behaviour
}

/*
void language_c::Dump(void)
{
	int i,j;
	for (i=0; i<choices.GetSize(); i++)
	{
		I_Printf("Language %d is %s\n", i, choices[i]);
	}	
	I_Printf("\n");

	for (i=0; i<refs.GetSize(); i++)
	{
		I_Printf("Ref %d is %s\n", i, refs[i]);
	}

	I_Printf("\n");
	for (i=0; i<choices.GetSize(); i++)
	{
		for (j=0; j<values[i].GetSize(); j++)
			I_Printf("Value %d/%d (%s) is %s\n", i, j, refs[j], values[i][j]);
			
		I_Printf("\n");
	}	
}
*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
