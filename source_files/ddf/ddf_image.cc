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
//
// Image Setup and Parser Code
//

#include "ddf_image.h"

#include <string.h>

#include "ddf_local.h"
#include "epi_filesystem.h"
#include "sokol_color.h"

static ImageDefinition *dynamic_image;

static void DdfImageGetType(const char *info, void *storage);
static void DdfImageGetSpecial(const char *info, void *storage);
static void DdfImageGetFixTrans(const char *info, void *storage);
static void DdfImageGetPatches(const char *info, void *storage);

// -ACB- 1998/08/10 Use DdfMainGetLumpName for getting the..lump name.
// -KM- 1998/09/27 Use DdfMainGetTime for getting tics

static ImageDefinition dummy_image;

static const DDFCommandList image_commands[] = {
    DDF_FIELD("IMAGE_DATA", dummy_image, type_, DdfImageGetType),
    DDF_FIELD("PATCHES", dummy_image, patches_, DdfImageGetPatches),
    DDF_FIELD("SPECIAL", dummy_image, special_, DdfImageGetSpecial),
    DDF_FIELD("X_OFFSET", dummy_image, x_offset_, DdfMainGetFloat),
    DDF_FIELD("Y_OFFSET", dummy_image, y_offset_, DdfMainGetFloat),
    DDF_FIELD("SCALE", dummy_image, scale_, DdfMainGetFloat),
    DDF_FIELD("ASPECT", dummy_image, aspect_, DdfMainGetFloat),
    DDF_FIELD("FIX_TRANS", dummy_image, fix_trans_, DdfImageGetFixTrans),
    DDF_FIELD("IS_FONT", dummy_image, is_font_, DdfMainGetBoolean),
    DDF_FIELD("ROTATE_HUE", dummy_image, hsv_rotation_, DdfMainGetNumeric),
    DDF_FIELD("SATURATION", dummy_image, hsv_saturation_, DdfMainGetNumeric),
    DDF_FIELD("BRIGHTNESS", dummy_image, hsv_value_, DdfMainGetNumeric),
    DDF_FIELD("BLUR_FACTOR", dummy_image, blur_factor_, DdfMainGetFloat),

    {nullptr, nullptr, 0, nullptr}};

ImageDefinitionContainer imagedefs;

static ImageNamespace GetImageNamespace(const char *prefix)
{
    if (DdfCompareName(prefix, "gfx") == 0)
        return kImageNamespaceGraphic;

    if (DdfCompareName(prefix, "tex") == 0)
        return kImageNamespaceTexture;

    if (DdfCompareName(prefix, "flat") == 0)
        return kImageNamespaceFlat;

    if (DdfCompareName(prefix, "spr") == 0)
        return kImageNamespaceSprite;

    if (DdfCompareName(prefix, "patch") == 0)
        return kImageNamespacePatch;

    DdfError("Invalid image prefix '%s' (use: gfx,tex,flat,spr)\n", prefix);
    return kImageNamespaceFlat; /* NOT REACHED */
}

//
//  DDF PARSE ROUTINES
//

static void ImageStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
        DdfError("New image entry is missing a name!\n");

    //	LogDebug("ImageStartEntry [%s]\n", name);

    ImageNamespace belong = kImageNamespaceGraphic;

    const char *pos = strchr(name, ':');

    if (!pos)
        DdfError("Missing image prefix.\n");

    if (pos)
    {
        std::string nspace(name, pos - name);

        if (nspace.empty())
            DdfError("Missing image prefix.\n");

        belong = GetImageNamespace(nspace.c_str());

        name = pos + 1;

        if (!name[0])
            DdfError("Missing image name.\n");
    }

    dynamic_image = imagedefs.Lookup(name, belong);

    if (extend)
    {
        if (!dynamic_image)
            DdfError("Unknown image to extend: %s\n", name);
        return;
    }

    // replaces an existing entry?
    if (dynamic_image)
    {
        dynamic_image->Default();
        return;
    }

    // not found, create a new one
    dynamic_image = new ImageDefinition;

    dynamic_image->name_   = name;
    dynamic_image->belong_ = belong;

    imagedefs.push_back(dynamic_image);
}

static void ImageParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DDF_DEBUG)
    LogDebug("IMAGE_PARSE: %s = %s;\n", field, contents);
#endif

    // ensure previous patches are cleared when beginning a new set
    if (DdfCompareName(field, "PATCHES") == 0 && index == 0)
        dynamic_image->patches_.clear();

    if (DdfMainParseField(image_commands, field, contents, (uint8_t *)dynamic_image))
        return; // OK

    DdfError("Unknown images.ddf command: %s\n", field);
}

static void ImageFinishEntry(void)
{
    if (dynamic_image->type_ == kImageDataFile || dynamic_image->type_ == kImageDataPackage)
    {
        if (epi::GetExtension(dynamic_image->info_) == ".lmp")
            dynamic_image->format_ = kLumpImageFormatDoom;
        else
            dynamic_image->format_ = kLumpImageFormatStandard;
    }

    // Add these automatically so modders don't have to remember them
    if (dynamic_image->is_font_)
    {
        dynamic_image->special_ = (ImageSpecial)(dynamic_image->special_ | kImageSpecialClamp);
        dynamic_image->special_ = (ImageSpecial)(dynamic_image->special_ | kImageSpecialNoMip);
    }

    // TODO: check more stuff...
}

static void ImageClearAll(void)
{
    LogWarning("Ignoring #CLEARALL in images.ddf\n");
}

void DdfReadImages(const std::string &data)
{
    DDFReadInfo images;

    images.tag      = "IMAGES";
    images.lumpname = "DDFIMAGE";

    images.start_entry  = ImageStartEntry;
    images.parse_field  = ImageParseField;
    images.finish_entry = ImageFinishEntry;
    images.clear_all    = ImageClearAll;

    DdfMainReadFile(&images, data);
}

void DdfImageInit(void)
{
    for (ImageDefinition *img : imagedefs)
    {
        delete img;
        img = nullptr;
    }
    imagedefs.clear();
}

void DdfImageCleanUp(void)
{
    imagedefs.shrink_to_fit(); // <-- Reduce to allocated size
}

static void ImageParseColour(const char *value)
{
    DdfMainGetRGB(value, &dynamic_image->colour_);
}

static void ImageParseInfo(const char *value)
{
    dynamic_image->info_ = value;
}

static void ImageParseLump(const char *spec)
{
    const char *colon = DdfMainDecodeList(spec, ':', true);

    if (colon == nullptr)
    {
        dynamic_image->info_   = spec;
        dynamic_image->format_ = kLumpImageFormatStandard;
    }
    else
    {
        // all this is mainly for backwards compatibility, but the
        // format "DOOM" does affect how the lump is handled.

        if (colon == spec || colon[1] == 0 || (colon - spec) >= 16)
            DdfError("Malformed image lump spec: 'LUMP:%s'\n", spec);

        char keyword[20];

        strncpy(keyword, spec, colon - spec);
        keyword[colon - spec] = 0;

        // store the lump name
        dynamic_image->info_ = (colon + 1);

        if (DdfCompareName(keyword, "PNG") == 0 || DdfCompareName(keyword, "TGA") == 0 ||
            DdfCompareName(keyword, "JPG") == 0 || DdfCompareName(keyword, "JPEG") == 0 ||
            DdfCompareName(keyword, "EXT") == 0) // 2.x used this for auto-detection of regular images, but
                                                  // we do this regardless of the extension
        {
            dynamic_image->format_ = kLumpImageFormatStandard;
        }
        else if (DdfCompareName(keyword, "DOOM") == 0)
        {
            dynamic_image->format_ = kLumpImageFormatDoom;
        }
        else
        {
            DdfError("Unknown image format: %s (use PNG,JPEG,TGA or DOOM)\n", keyword);
        }
    }
}

static void ImageParseCompose(const char *info)
{
    const char *colon = DdfMainDecodeList(info, ':', true);

    if (colon == nullptr || colon == info || colon[1] == 0)
        DdfError("Malformed image compose spec: %s\n", info);

    dynamic_image->compose_w_ = atoi(info);
    dynamic_image->compose_h_ = atoi(colon + 1);

    if (dynamic_image->compose_w_ <= 0 || dynamic_image->compose_h_ <= 0)
        DdfError("Illegal image compose size: %d x %d\n", dynamic_image->compose_w_, dynamic_image->compose_h_);
}

static void DdfImageGetType(const char *info, void *storage)
{
    const char *colon = DdfMainDecodeList(info, ':', true);

    if (colon == nullptr || colon == info || (colon - info) >= 16 || colon[1] == 0)
        DdfError("Malformed image type spec: %s\n", info);

    char keyword[20];

    strncpy(keyword, info, colon - info);
    keyword[colon - info] = 0;

    if (DdfCompareName(keyword, "COLOUR") == 0)
    {
        dynamic_image->type_ = kImageDataColor;
        ImageParseColour(colon + 1);
    }
    else if (DdfCompareName(keyword, "BUILTIN") == 0)
    {
        // accepted for backwards compat. only
        dynamic_image->type_   = kImageDataColor;
        dynamic_image->colour_ = 0;
    }
    else if (DdfCompareName(keyword, "FILE") == 0)
    {
        dynamic_image->type_ = kImageDataFile;
        ImageParseInfo(colon + 1);
    }
    else if (DdfCompareName(keyword, "LUMP") == 0)
    {
        dynamic_image->type_ = kImageDataLump;
        ImageParseLump(colon + 1);
    }
    else if (DdfCompareName(keyword, "PACK") == 0)
    {
        dynamic_image->type_ = kImageDataPackage;
        ImageParseInfo(colon + 1);
    }
    else if (DdfCompareName(keyword, "COMPOSE") == 0)
    {
        dynamic_image->type_ = kImageDataCompose;
        ImageParseCompose(colon + 1);
    }
    else
        DdfError("Unknown image type: %s\n", keyword);
}

static DDFSpecialFlags image_specials[] = {
    {"NOALPHA", kImageSpecialNoAlpha, 0},         {"FORCE_MIP", kImageSpecialMip, 0},
    {"FORCE_NOMIP", kImageSpecialNoMip, 0},       {"FORCE_CLAMP", kImageSpecialClamp, 0},
    {"FORCE_SMOOTH", kImageSpecialSmooth, 0},     {"FORCE_NOSMOOTH", kImageSpecialNoSmooth, 0},
    {"CROSSHAIR", kImageSpecialCrosshair, 0},     {"GRAYSCALE", kImageSpecialGrayscale, 0},
    {"FORCE_PRECACHE", kImageSpecialPrecache, 0}, {nullptr, 0, 0}};

static void DdfImageGetSpecial(const char *info, void *storage)
{
    ImageSpecial *dest = (ImageSpecial *)storage;

    int flag_value;

    switch (DdfMainCheckSpecialFlag(info, image_specials, &flag_value, false /* allow_prefixes */, false))
    {
    case kDdfCheckFlagPositive:
        *dest = (ImageSpecial)(*dest | flag_value);
        break;

    case kDdfCheckFlagNegative:
        *dest = (ImageSpecial)(*dest & ~flag_value);
        break;

    case kDdfCheckFlagUser:
    case kDdfCheckFlagUnknown:
        DdfWarnError("Unknown image special: %s\n", info);
        break;
    }
}

static void DdfImageGetFixTrans(const char *info, void *storage)
{
    ImageTransparencyFix *var = (ImageTransparencyFix *)storage;

    if (DdfCompareName(info, "NONE") == 0)
    {
        *var = kTransparencyFixNone;
    }
    else if (DdfCompareName(info, "BLACKEN") == 0)
    {
        *var = kTransparencyFixBlacken;
    }
    else
        DdfError("Unknown FIX_TRANS type: %s\n", info);
}

static void DdfImageGetPatches(const char *info, void *storage)
{
    // the syntax is: `NAME : XOFFSET : YOFFSET`.
    // in the future we may accept more stuff at the end.

    const char *colon1 = DdfMainDecodeList(info, ':', true);
    if (colon1 == nullptr || colon1 == info || colon1[1] == 0)
        DdfError("Malformed patch spec: %s\n", info);

    const char *colon2 = DdfMainDecodeList(colon1 + 1, ':', true);
    if (colon2 == nullptr || colon2 == colon1 + 1 || colon2[1] == 0)
        DdfError("Malformed patch spec: %s\n", info);

    ComposePatch patch;

    patch.name = std::string(info, (int)(colon1 - info));
    patch.x    = atoi(colon1 + 1);
    patch.y    = atoi(colon2 + 1);

    dynamic_image->patches_.push_back(patch);
}

// ---> imagedef_c class

ImageDefinition::ImageDefinition() : name_(), belong_(kImageNamespaceGraphic), info_()
{
    Default();
}

//
// Copies all the detail with the exception of ddf info
//
void ImageDefinition::CopyDetail(const ImageDefinition &src)
{
    type_   = src.type_;
    colour_ = src.colour_;
    info_   = src.info_;
    format_ = src.format_;

    compose_w_ = src.compose_w_;
    compose_h_ = src.compose_h_;
    patches_   = src.patches_;

    special_        = src.special_;
    x_offset_       = src.x_offset_;
    y_offset_       = src.y_offset_;
    scale_          = src.scale_;
    aspect_         = src.aspect_;
    fix_trans_      = src.fix_trans_;
    is_font_        = src.is_font_;
    hsv_rotation_   = src.hsv_rotation_;
    hsv_saturation_ = src.hsv_saturation_;
    hsv_value_      = src.hsv_value_;
    blur_factor_    = src.blur_factor_;
}

void ImageDefinition::Default()
{
    info_.clear();

    type_   = kImageDataColor;
    colour_ = SG_BLACK_RGBA32;
    format_ = kLumpImageFormatStandard;

    compose_w_ = compose_h_ = 0;
    patches_.clear();

    special_  = kImageSpecialNone;
    x_offset_ = y_offset_ = 0;

    scale_          = 1.0f;
    aspect_         = 1.0f;
    fix_trans_      = kTransparencyFixBlacken;
    is_font_        = false;
    hsv_rotation_   = 0;
    hsv_saturation_ = -1;
    hsv_value_      = 0;
    blur_factor_    = 0.0f;
}

// ---> imagedef_container_c class

void ImageDefinitionContainer::CleanupObject(void *obj)
{
    ImageDefinition *a = *(ImageDefinition **)obj;

    if (a)
        delete a;
}

ImageDefinition *ImageDefinitionContainer::Lookup(const char *refname, ImageNamespace belong)
{
    if (!refname || !refname[0])
        return nullptr;

    for (std::vector<ImageDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        ImageDefinition *g = *iter;

        if (DdfCompareName(g->name_.c_str(), refname) == 0 && g->belong_ == belong)
            return g;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
