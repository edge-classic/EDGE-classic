//----------------------------------------------------------------------------
//  EDGE Search Definition
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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

#pragma once

// QSORT routine  QuickSorts array in arr of type type, number of elements n
//  and stops when elements are cutoff sorted.  Then does an insertion sort
//  to complete data

#define EDGE_QSORT(type, arr, n, cutoff)                                                                               \
    {                                                                                                                  \
        int         *stk;                                                                                              \
        type         pivot;                                                                                            \
        type         t;                                                                                                \
        int          i, j, c, top;                                                                                     \
        unsigned int a, b;                                                                                             \
                                                                                                                       \
        stk = new int[n + 1];                                                                                          \
                                                                                                                       \
        a = top = 0;                                                                                                   \
        b       = n - 1;                                                                                               \
                                                                                                                       \
        while (1)                                                                                                      \
        {                                                                                                              \
            while (b > a + cutoff)                                                                                     \
            {                                                                                                          \
                c = (a + b) / 2;                                                                                       \
                                                                                                                       \
                if (EDGE_CMP(arr[b], arr[a]))                                                                          \
                {                                                                                                      \
                    t      = arr[a];                                                                                   \
                    arr[a] = arr[b];                                                                                   \
                    arr[b] = t;                                                                                        \
                }                                                                                                      \
                                                                                                                       \
                if (EDGE_CMP(arr[c], arr[a]))                                                                          \
                {                                                                                                      \
                    t      = arr[a];                                                                                   \
                    arr[a] = arr[c];                                                                                   \
                    arr[c] = t;                                                                                        \
                }                                                                                                      \
                                                                                                                       \
                if (EDGE_CMP(arr[c], arr[b]))                                                                          \
                {                                                                                                      \
                    t      = arr[c];                                                                                   \
                    arr[c] = arr[b];                                                                                   \
                    arr[b] = t;                                                                                        \
                }                                                                                                      \
                                                                                                                       \
                pivot      = arr[c];                                                                                   \
                arr[c]     = arr[b - 1];                                                                               \
                arr[b - 1] = pivot;                                                                                    \
                                                                                                                       \
                i = a, j = b - 1;                                                                                      \
                while (1)                                                                                              \
                {                                                                                                      \
                    do                                                                                                 \
                    {                                                                                                  \
                        i++;                                                                                           \
                    } while (EDGE_CMP(arr[i], arr[b - 1]));                                                            \
                                                                                                                       \
                    do                                                                                                 \
                    {                                                                                                  \
                        j--;                                                                                           \
                    } while (EDGE_CMP(arr[b - 1], arr[j]));                                                            \
                                                                                                                       \
                    if (j < i)                                                                                         \
                        break;                                                                                         \
                                                                                                                       \
                    t      = arr[i];                                                                                   \
                    arr[i] = arr[j];                                                                                   \
                    arr[j] = t;                                                                                        \
                }                                                                                                      \
                                                                                                                       \
                pivot      = arr[i];                                                                                   \
                arr[i]     = arr[b - 1];                                                                               \
                arr[b - 1] = pivot;                                                                                    \
                                                                                                                       \
                if (j - a > b - 1)                                                                                     \
                {                                                                                                      \
                    stk[top++] = a;                                                                                    \
                    stk[top++] = j;                                                                                    \
                    a          = i + 1;                                                                                \
                }                                                                                                      \
                else                                                                                                   \
                {                                                                                                      \
                    stk[top++] = i + 1;                                                                                \
                    stk[top++] = b;                                                                                    \
                    b          = j;                                                                                    \
                }                                                                                                      \
            }                                                                                                          \
                                                                                                                       \
            if (!top)                                                                                                  \
                break;                                                                                                 \
                                                                                                                       \
            b = stk[--top];                                                                                            \
            a = stk[--top];                                                                                            \
        }                                                                                                              \
                                                                                                                       \
        for (i = 1; i < n; i++)                                                                                        \
        {                                                                                                              \
            t = arr[i];                                                                                                \
            j = i;                                                                                                     \
            while (j >= 1 && EDGE_CMP(t, arr[j - 1]))                                                                  \
            {                                                                                                          \
                arr[j] = arr[j - 1];                                                                                   \
                j--;                                                                                                   \
            }                                                                                                          \
            arr[j] = t;                                                                                                \
        }                                                                                                              \
                                                                                                                       \
        delete[] stk;                                                                                                  \
    }

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
