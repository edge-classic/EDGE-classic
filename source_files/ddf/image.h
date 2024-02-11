//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Images)
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

#ifndef __DDF_IMAGE_H__
#define __DDF_IMAGE_H__

#include "epi.h"
#include "types.h"

typedef enum
{
    INS_Graphic = 0,
    INS_Texture,
    INS_Flat,
    INS_Sprite,
    INS_Patch,
} image_namespace_e;

//
// -AJA- 2004/11/16 Images.ddf
//
typedef enum
{
    IMGDT_Colour = 0,  // solid colour
    IMGDT_File,        // load from an image file
    IMGDT_Lump,        // load from lump in a WAD
    IMGDT_Package,     // load from an EPK package
    IMGDT_Compose      // compose from patches
} imagedata_type_e;

typedef enum
{
    IMGSP_None = 0,

    IMGSP_NoAlpha   = 0x0001,  // image does not require an alpha channel
    IMGSP_Mip       = 0x0002,  // force   mip-mapping
    IMGSP_NoMip     = 0x0004,  // disable mip-mapping
    IMGSP_Clamp     = 0x0008,  // clamp image
    IMGSP_Smooth    = 0x0010,  // force smoothing
    IMGSP_NoSmooth  = 0x0020,  // disable smoothing
    IMGSP_Crosshair = 0x0040,  // weapon crosshair (center vertically)
    IMGSP_Grayscale = 0x0080,  // forces image to be grayscaled upon creation
    IMGSP_Precache  = 0x0100,  // forces image to be precached upon creation
} image_special_e;

typedef enum
{
    FIXTRN_None    = 0,  // no modification (the default)
    FIXTRN_Blacken = 1,  // make 100% transparent pixels Black
} image_fix_trans_e;

typedef enum
{
    LIF_STANDARD = 0,  // something standard, e.g. PNG, TGA or JPEG
    LIF_DOOM     = 1,  // the DOOM "patch" format (in a wad lump)
} L_image_format_e;

class compose_patch_c
{
   public:
    std::string name;
    int         x = 0;
    int         y = 0;
};

class imagedef_c
{
   public:
    imagedef_c();
    ~imagedef_c(){};

   public:
    void Default(void);
    void CopyDetail(const imagedef_c &src);

    // Member vars....
    std::string       name;
    image_namespace_e belong;

    imagedata_type_e type;

    RGBAColor colour;  // IMGDT_Colour

    std::string      info;    // IMGDT_Package, IMGDT_File, IMGDT_Lump
    L_image_format_e format;  //

    int                          compose_w, compose_h;  // IMGDT_Compose
    std::vector<compose_patch_c> patches;               //

    image_special_e special;

    // offsets for sprites (mainly)
    float x_offset, y_offset;

    int fix_trans;  // FIXTRN_XXX value

    bool is_font;

    // RENDERING specifics:
    float scale, aspect;

    int hsv_rotation;
    int hsv_saturation;
    int hsv_value;

    // Gaussian blurring
    float blur_factor;

   private:
    // disable copy construct and assignment operator
    explicit imagedef_c(imagedef_c &rhs) { (void)rhs; }
    imagedef_c &operator=(imagedef_c &rhs)
    {
        (void)rhs;
        return *this;
    }
};

// Our imagedefs container
class imagedef_container_c : public std::vector<imagedef_c *>
{
   public:
    imagedef_container_c() {}
    ~imagedef_container_c()
    {
        for (auto iter = begin(); iter != end(); iter++)
        {
            imagedef_c *img = *iter;
            delete img;
            img = nullptr;
        }
    }

   private:
    void CleanupObject(void *obj);

   public:
    // Search Functions
    imagedef_c *Lookup(const char *refname, image_namespace_e belong);
};

extern imagedef_container_c imagedefs;

void DDF_ReadImages(const std::string &data);

#endif /*__DDF_IMAGE_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
