//----------------------------------------------------------------------------
//  PCX defines, raw on-disk layout
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
//
//  Based on "qfiles.h" from the GPL'd quake 2 source release.
//  Copyright (C) 1997-2001 Id Software, Inc.
//
//----------------------------------------------------------------------------

#ifndef __EPI_RAWDEF_PCX_H__
#define __EPI_RAWDEF_PCX_H__

namespace epi
{

/* 
 * PCX image structure
 */

typedef struct
{
  u8_t manufacturer;
  u8_t version;
  u8_t encoding;
  u8_t bits_per_pixel;

  u16_t xmin, ymin, xmax, ymax;
  u16_t hres, vres;

  u8_t palette[48];

  u8_t reserved;
  u8_t color_planes;

  u16_t bytes_per_line;
  u16_t palette_type;

  u8_t filler[58];
  u8_t data[1];      /* variable sized */
} 
raw_pcx_header_t;

}  // namespace epi

#endif  /* __EPI_RAWDEF_PCX_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
