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

#include "ddf_types.h"
#include "epi.h"

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
    kImageDataColor = 0, // solid colour
    kImageDataFile,      // load from an image file
    kImageDataLump,      // load from lump in a WAD
    kImageDataPackage    // load from an EPK package
};

enum ImageSpecial : uint32_t
{
    kImageSpecialNone      = 0,
    kImageSpecialNoAlpha   = 1 << 0, // image does not require an alpha channel
    kImageSpecialMip       = 1 << 1, // force   mip-mapping
    kImageSpecialNoMip     = 1 << 2, // disable mip-mapping
    kImageSpecialClamp     = 1 << 3, // clamp image
    kImageSpecialRepeat    = 1 << 4, // repeat image
    kImageSpecialSmooth    = 1 << 5, // force smoothing
    kImageSpecialNoSmooth  = 1 << 6, // disable smoothing
    kImageSpecialCrosshair = 1 << 7, // weapon crosshair (center vertically)
    kImageSpecialGrayscale = 1 << 8, // forces image to be grayscaled upon creation
    kImageSpecialPrecache  = 1 << 9, // forces image to be precached upon creation
    kImageSpecialFlip      = 1 << 10, // horizontally flip image when loading
    kImageSpecialInvert    = 1 << 11, // vertically flip image when loading
};

enum ImageTransparencyFix
{
    kTransparencyFixNone    = 0, // no modification (the default)
    kTransparencyFixBlacken = 1, // make 100% transparent pixels Black
};

enum LumpImageFormat
{
    kLumpImageFormatStandard = 0, // something standard, e.g. PNG, TGA or JPEG
    kLumpImageFormatDoom     = 1, // the DOOM "patch" format (in a wad lump)
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

    RGBAColor colour_;       // kImageDataColor

    std::string     info_;   // kImageDataPackage, kImageDataFile, kImageDataLump
    LumpImageFormat format_; //

    ImageSpecial special_;

    // offsets for sprites (mainly)
    float x_offset_, y_offset_;

    int fix_trans_; // kTransparencyFixXXX value

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
    explicit ImageDefinition(ImageDefinition &rhs)
    {
        EPI_UNUSED(rhs);
    }
    ImageDefinition &operator=(ImageDefinition &rhs)
    {
        EPI_UNUSED(rhs);
        return *this;
    }
};

class ImageDefinitionContainer : public std::vector<ImageDefinition *>
{
  public:
    ImageDefinitionContainer()
    {
    }
    ~ImageDefinitionContainer()
    {
        for (std::vector<ImageDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
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

void DDFReadImages(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
