//----------------------------------------------------------------------------
//  EDGE Search Definition
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#ifndef __E_SEARCH_H__
#define __E_SEARCH_H__

// QSORT routine  QuickSorts array in arr of type type, number of elements n
//  and stops when elements are cutoff sorted.  Then does an insertion sort
//  to complete data
// -ES- 2000/02/15 Removed iQSORT: Unneeded.
#define CUTOFF 10
#define QSORT(type, arr, n, cutoff)\
{                                                                       \
  	int *stk;                                                             \
  	type pivot;                                                           \
  	type t;                                                               \
  	int i, j, c, top;                                                     \
  	unsigned int a, b;                                                    \
                                                                        \
  	stk = new int[n+1];                                              \
                                                                        \
  	a = top = 0;                                                          \
  	b = n - 1;                                                            \
                                                                        \
  	while (1)                                                             \
  	{                                                                     \
		while (b > a + cutoff)                                             \
     	{                                                                  \
       		c = (a + b) / 2;                                                 \
       																		\
       		if (CMP(arr[b], arr[a]))                                         \
       		{                                                                \
          		t = arr[a];                                                   \
          		arr[a] = arr[b];                                              \
          		arr[b] = t;                                                   \
       		} 																\
																       		\
       		if (CMP(arr[c], arr[a]))                                          \
       		{                                                                \
          		t = arr[a];                                                   \
          		arr[a] = arr[c];                                              \
          		arr[c] = t;                                                   \
       		}																\
       		                                                              \
       		if (CMP(arr[c], arr[b]))                                     \
       		{                                                                \
          		t = arr[c];                                                   \
          		arr[c] = arr[b];                                              \
          		arr[b] = t;                                                   \
       		}                                                                \
                                                                        \
			pivot = arr[c];                                                  \
       		arr[c] = arr[b-1];                                               \
       		arr[b-1] = pivot;                                                \
                                                                        \
       		i = a, j = b-1;                                                  \
       		while (1)                                                        \
       		{                                                                \
          		do { i++; }                                                   \
          		while (CMP(arr[i], arr[b-1]));                          \
          																\
          		do { j--; }                                                   \
          		while (CMP(arr[b-1], arr[j]));                             \
          																	\
          		if (j < i)                                                    \
            		break;                                                  \
            																\
          		t = arr[i];                                                   \
          		arr[i] = arr[j];                                              \
          		arr[j] = t;                                                   \
          	}                                                                \
                                                                        \
       		pivot = arr[i];                                                  \
       		arr[i] = arr[b-1];                                               \
       		arr[b-1] = pivot;                                                \
                                                                        \
       		if (j - a > b - 1)                                               \
       		{                                                                \
          		stk[top++] = a;                                               \
          		stk[top++] = j;                                               \
          		a = i+1;                                                      \
       		}                                                                \
       		else                                                             \
       		{                                                                \
          		stk[top++] = i+1;                                             \
          		stk[top++] = b;                                               \
         		b = j;                                                        \
       		}                                                                \
     	}                                                                  \
                                                                        \
     	if (!top)                                                          \
       		break;                                                           \
       																	\
		b = stk[--top];                                                    \
     	a = stk[--top];                                                    \
   	}                                                                    \
                                                                        \
   	for (i = 1; i < n; i++)                                              \
   	{                                                                    \
     	t = arr[i];                                                        \
     	j = i;                                                             \
     	while (j >= 1 && CMP(t, arr[j-1]))                                 \
     	{                                                                  \
        	arr[j] = arr[j-1];                                              \
        	j--;                                                            \
     	}                                                                  \
     	arr[j] = t;                                                        \
   }                                                                    \
                                                                        \
   delete [] stk;                                                         \
}

#define MAPSORT(type, arr, map, n)                                      \
{                                                                       \
   type tmp;                                                            \
   int i, j, k;                                                         \
                                                                        \
   for (i = 0; i < n; i++)                                              \
   {                                                                    \
      if (map[i] != i)                                                  \
      {                                                                 \
         tmp = arr[i];                                                  \
         k = i;                                                         \
         do                                                             \
         {                                                              \
            j = k;                                                      \
            k = map[j];                                                 \
            arr[j] = arr[k];                                            \
            map[j] = j;                                                 \
         } while (k != i);                                              \
         arr[j] = tmp;                                                  \
      }                                                                 \
   }                                                                    \
}

#define BSEARCH(n, pos)                                                 \
{                                                                       \
     int hi, mid;                                                       \
                                                                        \
     pos = 0;                                                           \
     hi = n-1;                                                          \
                                                                        \
     while (pos <= hi)                                                  \
     {                                                                  \
        mid = (pos + hi) >> 1;                                          \
        if (CMP(mid))                                                   \
          pos = mid + 1;                                                \
        else                                                            \
          hi = mid - 1;                                                 \
     }                                                                  \
}

#endif

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
