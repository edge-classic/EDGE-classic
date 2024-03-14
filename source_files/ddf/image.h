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

#pragma once

#include "types.h"

enum ImageNamespace
{
    kImageNamespaceGraphic = 0,
    kImageNamespaceTexture,
    kImageNamespaceFlat,
    kImageNamespaceSprite,
    kImageNamespacePatch,
};

//
// -AJA- 2004/11/16 Images.ddf
//
enum ImageDataType
{
    kImageDataColor = 0,  // solid colour
    kImageDataFile,       // load from an image file
    kImageDataLump,       // load from lump in a WAD
    kImageDataPackage,    // load from an EPK package
    kImageDataCompose     // compose from patches
};

enum ImageSpecial
{
    kImageSpecialNone      = 0,
    kImageSpecialNoAlpha   = 0x0001,  // image does not require an alpha channel
    kImageSpecialMip       = 0x0002,  // force   mip-mapping
    kImageSpecialNoMip     = 0x0004,  // disable mip-mapping
    kImageSpecialClamp     = 0x0008,  // clamp image
    kImageSpecialSmooth    = 0x0010,  // force smoothing
    kImageSpecialNoSmooth  = 0x0020,  // disable smoothing
    kImageSpecialCrosshair = 0x0040,  // weapon crosshair (center vertically)
    kImageSpecialGrayscale =
        0x0080,  // forces image to be grayscaled upon creation
    kImageSpecialPrecache =
        0x0100,  // forces image to be precached upon creation
};

enum ImageTransparencyFix
{
    kTransparencyFixNone    = 0,  // no modification (the default)
    kTransparencyFixBlacken = 1,  // make 100% transparent pixels Black
};

enum LumpImageFormat
{
    kLumpImageFormatStandard = 0,  // something standard, e.g. PNG, TGA or JPEG
    kLumpImageFormatDoom     = 1,  // the DOOM "patch" format (in a wad lump)
};

struct ComposePatch
{
    std::string name;
    int         x = 0;
    int         y = 0;
};

class ImageDefinition
{
   public:
    ImageDefinition();
    ~ImageDefinition(){};

   public:
    void Default(void);
    void CopyDetail(const ImageDefinition &src);

    std::string    name_;
    ImageNamespace belong_;

    ImageDataType type_;

    RGBAColor colour_;  // kImageDataColor

    std::string     info_;  // kImageDataPackage, kImageDataFile, kImageDataLump
    LumpImageFormat format_;  //

    int                       compose_w_, compose_h_;  // kImageDataCompose
    std::vector<ComposePatch> patches_;                //

    ImageSpecial special_;

    // offsets for sprites (mainly)
    float x_offset_, y_offset_;

    int fix_trans_;  // kTransparencyFixXXX value

    bool is_font_;

    // RENDERING specifics:
    float scale_, aspect_;

    int hsv_rotation_;
    int hsv_saturation_;
    int hsv_value_;

    // Gaussian blurring
    float blur_factor_;

   private:
    // disable copy construct and assignment operator
    explicit ImageDefinition(ImageDefinition &rhs) { (void)rhs; }
    ImageDefinition &operator=(ImageDefinition &rhs)
    {
        (void)rhs;
        return *this;
    }
};

class ImageDefinitionContainer : public std::vector<ImageDefinition *>
{
   public:
    ImageDefinitionContainer() {}
    ~ImageDefinitionContainer()
    {
        for (std::vector<ImageDefinition *>::iterator iter     = begin(),
                                                      iter_end = end();
             iter != iter_end; iter++)
        {
            ImageDefinition *img = *iter;
            delete img;
            img = nullptr;
        }
    }

   private:
    void CleanupObject(void *obj);

   public:
    // Search Functions
    ImageDefinition *Lookup(const char *refname, ImageNamespace belong);
};

extern ImageDefinitionContainer imagedefs;

void DDF_ReadImages(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
