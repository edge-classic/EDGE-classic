//----------------------------------------------------------------------------
//  EC_VOXELIB Voxel Loading Library
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2022-2023  The EDGE Team.
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
//  NOTE: This fork of voxelib is distributed as the "ec_voxelib" library 
//  under the GPL3+ with permission from the original author. Upstream voxelib
//  maintains GPL3-only licensing. Original copyright and licensing
//  statement follow.
//----------------------------------------------------------------------------


//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2023 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
#include "ec_voxelib.h"
#include <stdarg.h>
#include <math.h>

#include "AlmostEquals.h"

//#define VOXELIB_CHECK_INVARIANTS


VoxLibMsg voxlib_verbose = VoxLibMsg_None;
void (*voxlib_message) (VoxLibMsg type, const char *msg) = nullptr;

// this function MUST NOT RETURN!
void (*voxlib_fatal) (const char *msg) = nullptr;


#define VABUF_SIZE  (4096)
static char vabuf[VABUF_SIZE];


// default palette for Magica Voxel files
static const uint32_t magicaPal[256] = {
  0x00000000U,0xffffffffU,0xffccffffU,0xff99ffffU,0xff66ffffU,0xff33ffffU,0xff00ffffU,0xffffccffU,0xffccccffU,0xff99ccffU,0xff66ccffU,0xff33ccffU,0xff00ccffU,0xffff99ffU,0xffcc99ffU,0xff9999ffU,
  0xff6699ffU,0xff3399ffU,0xff0099ffU,0xffff66ffU,0xffcc66ffU,0xff9966ffU,0xff6666ffU,0xff3366ffU,0xff0066ffU,0xffff33ffU,0xffcc33ffU,0xff9933ffU,0xff6633ffU,0xff3333ffU,0xff0033ffU,0xffff00ffU,
  0xffcc00ffU,0xff9900ffU,0xff6600ffU,0xff3300ffU,0xff0000ffU,0xffffffccU,0xffccffccU,0xff99ffccU,0xff66ffccU,0xff33ffccU,0xff00ffccU,0xffffccccU,0xffccccccU,0xff99ccccU,0xff66ccccU,0xff33ccccU,
  0xff00ccccU,0xffff99ccU,0xffcc99ccU,0xff9999ccU,0xff6699ccU,0xff3399ccU,0xff0099ccU,0xffff66ccU,0xffcc66ccU,0xff9966ccU,0xff6666ccU,0xff3366ccU,0xff0066ccU,0xffff33ccU,0xffcc33ccU,0xff9933ccU,
  0xff6633ccU,0xff3333ccU,0xff0033ccU,0xffff00ccU,0xffcc00ccU,0xff9900ccU,0xff6600ccU,0xff3300ccU,0xff0000ccU,0xffffff99U,0xffccff99U,0xff99ff99U,0xff66ff99U,0xff33ff99U,0xff00ff99U,0xffffcc99U,
  0xffcccc99U,0xff99cc99U,0xff66cc99U,0xff33cc99U,0xff00cc99U,0xffff9999U,0xffcc9999U,0xff999999U,0xff669999U,0xff339999U,0xff009999U,0xffff6699U,0xffcc6699U,0xff996699U,0xff666699U,0xff336699U,
  0xff006699U,0xffff3399U,0xffcc3399U,0xff993399U,0xff663399U,0xff333399U,0xff003399U,0xffff0099U,0xffcc0099U,0xff990099U,0xff660099U,0xff330099U,0xff000099U,0xffffff66U,0xffccff66U,0xff99ff66U,
  0xff66ff66U,0xff33ff66U,0xff00ff66U,0xffffcc66U,0xffcccc66U,0xff99cc66U,0xff66cc66U,0xff33cc66U,0xff00cc66U,0xffff9966U,0xffcc9966U,0xff999966U,0xff669966U,0xff339966U,0xff009966U,0xffff6666U,
  0xffcc6666U,0xff996666U,0xff666666U,0xff336666U,0xff006666U,0xffff3366U,0xffcc3366U,0xff993366U,0xff663366U,0xff333366U,0xff003366U,0xffff0066U,0xffcc0066U,0xff990066U,0xff660066U,0xff330066U,
  0xff000066U,0xffffff33U,0xffccff33U,0xff99ff33U,0xff66ff33U,0xff33ff33U,0xff00ff33U,0xffffcc33U,0xffcccc33U,0xff99cc33U,0xff66cc33U,0xff33cc33U,0xff00cc33U,0xffff9933U,0xffcc9933U,0xff999933U,
  0xff669933U,0xff339933U,0xff009933U,0xffff6633U,0xffcc6633U,0xff996633U,0xff666633U,0xff336633U,0xff006633U,0xffff3333U,0xffcc3333U,0xff993333U,0xff663333U,0xff333333U,0xff003333U,0xffff0033U,
  0xffcc0033U,0xff990033U,0xff660033U,0xff330033U,0xff000033U,0xffffff00U,0xffccff00U,0xff99ff00U,0xff66ff00U,0xff33ff00U,0xff00ff00U,0xffffcc00U,0xffcccc00U,0xff99cc00U,0xff66cc00U,0xff33cc00U,
  0xff00cc00U,0xffff9900U,0xffcc9900U,0xff999900U,0xff669900U,0xff339900U,0xff009900U,0xffff6600U,0xffcc6600U,0xff996600U,0xff666600U,0xff336600U,0xff006600U,0xffff3300U,0xffcc3300U,0xff993300U,
  0xff663300U,0xff333300U,0xff003300U,0xffff0000U,0xffcc0000U,0xff990000U,0xff660000U,0xff330000U,0xff0000eeU,0xff0000ddU,0xff0000bbU,0xff0000aaU,0xff000088U,0xff000077U,0xff000055U,0xff000044U,
  0xff000022U,0xff000011U,0xff00ee00U,0xff00dd00U,0xff00bb00U,0xff00aa00U,0xff008800U,0xff007700U,0xff005500U,0xff004400U,0xff002200U,0xff001100U,0xffee0000U,0xffdd0000U,0xffbb0000U,0xffaa0000U,
  0xff880000U,0xff770000U,0xff550000U,0xff440000U,0xff220000U,0xff110000U,0xffeeeeeeU,0xffddddddU,0xffbbbbbbU,0xffaaaaaaU,0xff888888U,0xff777777U,0xff555555U,0xff444444U,0xff222222U,0xff111111U,
};


//==========================================================================
//
//  vox_logf
//
//==========================================================================
static
#ifndef _MSC_VER
__attribute__((format(printf, 2, 3)))
#endif
void vox_logf (VoxLibMsg type, const char *fmt, ...) {
  if (type != VoxLibMsg_Error) {
    if ((int)voxlib_verbose <= 0) return;
    if ((int)type > (int)voxlib_verbose) return;
  }
  if (!fmt || !fmt[0] || !voxlib_message) return;
  va_list ap;
  va_start(ap, fmt);
  int size = vsnprintf(vabuf, (size_t)VABUF_SIZE, fmt, ap);
  va_end(ap);
  if (size < 0 || size >= VABUF_SIZE) {
    vabuf[VABUF_SIZE-1] = 0;
    size_t slen = strlen(vabuf);
    if (slen+4 > (size_t)VABUF_SIZE) slen = (size_t)VABUF_SIZE-4;
    strcpy(vabuf+slen, "...");
  }
  voxlib_message(type, vabuf);
}


//==========================================================================
//
//  vox_fatal
//
//==========================================================================
#ifdef _MSC_VER
__declspec(noreturn)
#else
__attribute__((noreturn))
#endif
void vox_fatal (const char *msg) {
  if (!msg || !msg[0]) msg = "voxlib fatal error";
  if (!voxlib_fatal) {
    vox_logf(VoxLibMsg_Error, "%s", msg);
  } else {
    voxlib_fatal(msg);
  }
  exit(1);
}


#define VOX_COMATOZE_BUF_SIZE   (128)
#define VOX_COMATOZE_BUF_COUNT  (8)
static char vox_comatozebufs[VOX_COMATOZE_BUF_SIZE][VOX_COMATOZE_BUF_COUNT];
static unsigned vox_comatozebufidx = 0;


//==========================================================================
//
//  vox_comatoze
//
//==========================================================================
static const char *vox_comatoze (uint32_t n, const char *sfx=nullptr) {
  char *buf = vox_comatozebufs[vox_comatozebufidx++];
  if (vox_comatozebufidx == VOX_COMATOZE_BUF_COUNT) vox_comatozebufidx = 0;
  int bpos = (int)VOX_COMATOZE_BUF_SIZE;
  buf[--bpos] = 0;
  if (sfx) {
    size_t slen = strlen(sfx);
    while (slen--) {
      if (bpos > 0) buf[--bpos] = sfx[slen];
    }
  }
  int xcount = 0;
  do {
    if (xcount == 3) { if (bpos > 0) buf[--bpos] = ','; xcount = 0; }
    if (bpos > 0) buf[--bpos] = '0'+n%10;
    ++xcount;
  } while ((n /= 10) != 0);
  return &buf[bpos];
}


// ////////////////////////////////////////////////////////////////////////// //
// Vox2DBitmap
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  Vox2DBitmap::doOne
//
//==========================================================================
bool Vox2DBitmap::doOne (int *rx0, int *ry0, int *rx1, int *ry1) {
  if (dotCount == 0) return false;

  if (cache.length() < wdt+1) cache.setLength(wdt+1);
  if (stack.length() < wdt+1) stack.setLength(wdt+1);

  Pair best_ll;
  Pair best_ur;
  int best_area;

  best_ll.one = best_ll.two = 0;
  best_ur.one = best_ur.two = -1;
  best_area = 0;
  top = 0;
  bool cacheCleared = true;

  for (int m = 0; m <= wdt; ++m) cache[m] = stack[m].one = stack[m].two = 0;

  // main algorithm
  for (int n = 0; n < hgt; ++n) {
    // there is no need to scan empty lines
    // (and we usually have quite a lot of them)
    if (ydotCount/*.ptr()*/[n] == 0) {
      if (!cacheCleared) {
        cacheCleared = true;
        memset(cache.ptr(), 0, wdt*sizeof(int));
      }
      continue;
    }
    int openWidth = 0;
    updateCache(n);
    cacheCleared = false;
    const int *cp = cache.ptr();
    for (int m = 0; m <= wdt; ++m) {
      const int cvl = *cp++;
      if (cvl > openWidth) {
        // open new rectangle
        push(m, openWidth);
        openWidth = cvl;
      } else if (cvl < openWidth) {
        // close rectangle(s)
        int m0, w0;
        do {
          pop(&m0, &w0);
          const int area = openWidth*(m-m0);
          if (area > best_area) {
            best_area = area;
            best_ll.one = m0; best_ll.two = n;
            best_ur.one = m-1; best_ur.two = n-openWidth+1;
          }
          openWidth = w0;
        } while (cvl < openWidth);
        openWidth = cvl;
        if (openWidth != 0) push(m0, w0);
      }
    }
  }

  *rx0 = (best_ll.one < best_ur.one ? best_ll.one : best_ur.one);
  *ry0 = (best_ll.two < best_ur.two ? best_ll.two : best_ur.two);
  *rx1 = (best_ll.one > best_ur.one ? best_ll.one : best_ur.one);
  *ry1 = (best_ll.two > best_ur.two ? best_ll.two : best_ur.two);

  // remove dots
  /*
  for (int y = *ry0; y <= *ry1; ++y) {
    for (int x = *rx0; x <= *rx1; ++x) {
      resetPixel(x, y);
    }
  }
  */

  return true;
};


// ////////////////////////////////////////////////////////////////////////// //
// VoxTexAtlas
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VoxTexAtlas::findBestFit
//
//  node id or BadRect
//
//==========================================================================
uint32_t VoxTexAtlas::findBestFit (int w, int h) {
  uint32_t fitW = BadRect, fitH = BadRect, biggest = BadRect;

  const Rect *r = rects.ptr();
  const int rlen = rects.length();
  for (int idx = 0; idx < rlen; ++idx, ++r) {
    if (r->w < w || r->h < h) continue; // absolutely can't fit
    if (r->w == w && r->h == h) return (uint32_t)idx; // perfect fit
    if (r->w == w) {
      // width fit
      if (fitW == BadRect || rects/*.ptr()*/[fitW].h < r->h) fitW = (uint32_t)idx;
    } else if (r->h == h) {
      // height fit
      if (fitH == BadRect || rects/*.ptr()*/[fitH].w < r->w) fitH = (uint32_t)idx;
    } else {
      // get biggest rect
      if (biggest == BadRect || rects/*.ptr()*/[biggest].getArea() > r->getArea()) {
        biggest = (uint32_t)idx;
      }
    }
  }

  // both?
  if (fitW != BadRect && fitH != BadRect) {
    return (rects/*.ptr()*/[fitW].getArea() > rects/*.ptr()*/[fitH].getArea() ? fitW : fitH);
  }
  if (fitW != BadRect) return fitW;
  if (fitH != BadRect) return fitH;
  return biggest;
}


//==========================================================================
//
//  VoxTexAtlas::insert
//
//  returns invalid rect if there's no room
//
//==========================================================================
VoxTexAtlas::Rect VoxTexAtlas::insert (int cwdt, int chgt) {
  vassert(cwdt > 0 && chgt > 0);
  if (cwdt > imgWidth || chgt > imgHeight) return Rect::Invalid();
  uint32_t ri = findBestFit(cwdt, chgt);
  if (ri == BadRect) return Rect::Invalid();
  Rect rc = rects[ri];
  auto res = Rect(rc.x, rc.y, cwdt, chgt);
  // split this rect
  if (rc.w == res.w && rc.h == res.h) {
    // best fit, simply remove this rect
    rects.removeAt((int)ri);
  } else {
    if (rc.w == res.w) {
      // split vertically
      rc.y += res.h;
      rc.h -= res.h;
    } else if (rc.h == res.h) {
      // split horizontally
      rc.x += res.w;
      rc.w -= res.w;
    } else {
      Rect nr = rc;
      // split in both directions (by longer edge)
      if (rc.w-res.w > rc.h-res.h) {
        // cut the right part
        nr.x += res.w;
        nr.w -= res.w;
        // cut the bottom part
        rc.y += res.h;
        rc.h -= res.h;
        rc.w = res.w;
      } else {
        // cut the bottom part
        nr.y += res.h;
        nr.h -= res.h;
        // cut the right part
        rc.x += res.w;
        rc.w -= res.w;
        rc.h = res.h;
      }
      rects.append(nr);
    }
    rects/*.ptr()*/[ri] = rc;
  }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
// VoxColorPack
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VoxColorPack::growImage
//
//  grow image, and relayout everything
//
//==========================================================================
void VoxColorPack::growImage (uint32_t inswdt, uint32_t inshgt) {
  uint32_t neww = clrwdt, newh = clrhgt;
  while (neww < inswdt) neww <<= 1;
  while (newh < inshgt) newh <<= 1;
  for (;;) {
    if (neww < newh) neww <<= 1; else newh <<= 1;
    // relayout data
    bool again = false;
    atlas.setSize(neww, newh);
    for (int f = 0; f < citems.length(); ++f) {
      ColorItem &ci = citems[f];
      auto rc = atlas.insert((int)ci.wh.getW(), (int)ci.wh.getH());
      if (!rc.isValid()) {
        // alas, no room
        again = true;
        break;
      }
      // record new coords
      ci.newxy = VoxXY16(rc.x, rc.y);
    }
    if (!again) break; // done
  }

  // allocate new image, copy old data
  if (voxlib_verbose >= VoxLibMsg_Debug) {
    vox_logf(VoxLibMsg_Debug, "ATLAS: resized from %ux%u to %ux%u", clrwdt, clrhgt, neww, newh);
  }

  VoxLibArray<uint32_t> newclr;
  newclr.setLength(neww*newh);
  memset(newclr.ptr(), 0, neww*newh*sizeof(uint32_t));
  for (int f = 0; f < citems.length(); ++f) {
    ColorItem &ci = citems[f];
    const uint32_t rcw = ci.wh.getW();
    uint32_t oaddr = ci.xy.getY()*clrwdt+ci.xy.getX();
    uint32_t naddr = ci.newxy.getY()*neww+ci.newxy.getX();
    uint32_t dy = ci.wh.getH();
    /*
    conwriteln(": : : oldpos=(", ci.rc.getX(), ",", ci.rc.getY(), "); newpos=(", newx, ",",
               newy, "); size=(", rcw, "x", ci.rc.getH(), "); oaddr=", oaddr, "; naddr=", naddr);
    */
    while (dy--) {
      memcpy(newclr.ptr()+naddr, colors.ptr()+oaddr, rcw*sizeof(uint32_t));
      oaddr += clrwdt;
      naddr += neww;
    }
    ci.xy = ci.newxy;
  }
  /*
  colors.setLength(0);
  colors = newclr;
  newclr.clear();
  */
  colors.transferDataFrom(newclr);
  vassert(newclr.length() == 0);
  clrwdt = neww;
  clrhgt = newh;
  vassert((uint32_t)colors.length() == clrwdt*clrhgt);
}


//==========================================================================
//
//  VoxColorPack::findRectEx
//
//  returns true if found, and sets `*cidxp` and `*xyofsp`
//  `*xyofsp` is offset inside `cidxp`
//
//==========================================================================
bool VoxColorPack::findRectEx (const uint32_t *clrs, uint32_t cwdt, uint32_t chgt,
                               uint32_t cxofs, uint32_t cyofs,
                               uint32_t wdt, uint32_t hgt, uint32_t *cidxp, VoxWH16 *whp)
{
  vassert(wdt > 0 && hgt > 0);
  vassert(cwdt >= wdt && chgt >= hgt);

  const uint32_t saddrOrig = cyofs*cwdt+cxofs;
  auto cp = citemhash.get(clrs[saddrOrig]);
  if (!cp) return false;

  for (int cidx = *cp; cidx >= 0; cidx = citems[cidx].next) {
    const ColorItem &ci = citems[cidx];
    if (wdt > ci.wh.getW() || hgt > ci.wh.getH()) continue; // impossibiru
    // compare colors
    bool ok = true;
    uint32_t saddr = saddrOrig;
    uint32_t caddr = ci.xy.getY()*clrwdt+ci.xy.getX();
    for (uint32_t dy = 0; dy < hgt; ++dy) {
      if (memcmp(colors.ptr()+caddr, clrs+saddr, wdt*sizeof(uint32_t)) != 0) {
        ok = false;
        break;
      }
      saddr += cwdt;
      caddr += clrwdt;
    }
    if (ok) {
      // i found her!
      // topmost
      if (cidxp) *cidxp = (uint32_t)cidx;
      if (whp) *whp = VoxWH16(wdt, hgt);
      return true;
    }
  }

  return false;
}


//==========================================================================
//
//  VoxColorPack::addNewRect
//
//  returns index in `citems`
//
//==========================================================================
uint32_t VoxColorPack::addNewRect (const uint32_t *clrs, uint32_t wdt, uint32_t hgt) {
  vassert(wdt > 0 && hgt > 0);
  VoxXY16 coord;

  if (clrwdt == 0) {
    // no rects yet
    vassert(clrhgt == 0);
    clrwdt = 1;
    while (clrwdt < wdt) clrwdt <<= 1;
    clrhgt = 1;
    while (clrhgt < hgt) clrhgt <<= 1;
    if (clrhgt < clrwdt) clrhgt = clrwdt; //!!
    atlas.setSize(clrwdt, clrhgt);
    colors.setLength(clrwdt*clrhgt);
    memset(colors.ptr(), 0, clrwdt*clrhgt*sizeof(uint32_t));
  }

  // insert into atlas; grow texture if cannot insert
  for (;;) {
    auto rc = atlas.insert((int)wdt, (int)hgt);
    if (rc.isValid()) {
      coord = VoxXY16(rc.x, rc.y);
      break;
    }
    // no room, grow the texture, and relayout everything
    growImage(wdt, hgt);
  }

  // copy source colors into the atlas image
  uint32_t saddr = 0;
  uint32_t daddr = coord.getY()*clrwdt+coord.getX();
  for (uint32_t dy = 0; dy < hgt; ++dy) {
    memcpy(colors.ptr()+daddr, clrs+saddr, wdt*sizeof(uint32_t));
    saddr += wdt;
    daddr += clrwdt;
  }

  // hash main rect
  ColorItem ci;
  ci.xy = coord;
  ci.wh = VoxWH16(wdt, hgt);
  const int parentIdx = citems.length();
  uint32_t cc = clrs[0];
  auto cpp = citemhash.get(cc);
  if (cpp) {
    ci.next = *cpp;
    *cpp = parentIdx;
  } else {
    ci.next = -1;
    citemhash.put(cc, parentIdx);
  }
  citems.append(ci);

  return (uint32_t)parentIdx;
}


// ////////////////////////////////////////////////////////////////////////// //
vv_push_pack
struct vv_packed_struct VoxXYZ16 {
  uint16_t x, y, z;

  inline VoxXYZ16 () {}
  inline VoxXYZ16 (uint16_t ax, uint16_t ay, uint16_t az) : x(ax), y(ay), z(az) {}
};
vv_pop_pack


// ////////////////////////////////////////////////////////////////////////// //
// VoxelData
// ////////////////////////////////////////////////////////////////////////// //

const int VoxelData::cullofs[6][3] = {
  { 1, 0, 0}, // left
  {-1, 0, 0}, // right
  { 0,-1, 0}, // near
  { 0, 1, 0}, // far
  { 0, 0, 1}, // top
  { 0, 0,-1}, // bottom
};


//==========================================================================
//
//  VoxelData::allocVox
//
//==========================================================================
uint32_t VoxelData::allocVox () {
  vassert(data.length());
  ++voxpixtotal;
  if (!freelist) {
    if (data.length() >= 0x3fffffff) voxlib_fatal("too many voxels");
    const uint32_t lastel = (uint32_t)data.length();
    data.setLength((int)lastel+1024);
    freelist = (uint32_t)data.length()-1;
    while (freelist >= lastel) {
      data/*.ptr()*/[freelist].nextz = freelist+1;
      --freelist;
    }
    freelist = lastel;
    data/*.ptr()*/[data.length()-1].nextz = 0;
  }
  const uint32_t res = freelist;
  freelist = data/*.ptr()*/[res].nextz;
  return res;
}


//==========================================================================
//
//  VoxelData::clear
//
//==========================================================================
void VoxelData::clear () {
  data.clear();
  xyofs.clear();
  xsize = ysize = zsize = 0;
  cx = cy = cz = 0.0f;
  freelist = 0;
  voxpixtotal = 0;
}


//==========================================================================
//
//  VoxelData::setSize
//
//==========================================================================
void VoxelData::setSize (uint32_t xs, uint32_t ys, uint32_t zs) {
  clear();
  if (!xs || !ys || !zs) return;
  xsize = xs;
  ysize = ys;
  zsize = zs;
  xyofs.setLength(xsize*ysize);
  memset(xyofs.ptr(), 0, xsize*ysize*sizeof(uint32_t));
  data.setLength(1); // data[0] is never used
}


//==========================================================================
//
//  VoxelData::removeVoxel
//
//==========================================================================
void VoxelData::removeVoxel (int x, int y, int z) {
  if (x < 0 || y < 0 || z < 0) return;
  if ((uint32_t)x >= xsize || (uint32_t)y >= ysize || (uint32_t)z >= zsize) return;
  uint32_t dofs = getDOfs(x, y);
  uint32_t prevdofs = 0;
  while (dofs) {
    if (data/*.ptr()*/[dofs].z == (uint16_t)z) {
      // remove this voxel
      if (prevdofs) {
        data/*.ptr()*/[prevdofs].nextz = data/*.ptr()*/[dofs].nextz;
      } else {
        xyofs[(uint32_t)y*xsize+(uint32_t)x] = data/*.ptr()*/[dofs].nextz;
      }
      data/*.ptr()*/[dofs].nextz = freelist;
      freelist = dofs;
      --voxpixtotal;
      return;
    }
    if (data/*.ptr()*/[dofs].z > (uint16_t)z) return;
    prevdofs = dofs;
    dofs = data/*.ptr()*/[dofs].nextz;
  }
}


//==========================================================================
//
//  VoxelData::addVoxel
//
//==========================================================================
void VoxelData::addVoxel (int x, int y, int z, uint32_t rgb, uint8_t cull) {
  cull &= 0x3f;
  if (!cull) { removeVoxel(x, y, z); return; }
  if (x < 0 || y < 0 || z < 0) return;
  if ((uint32_t)x >= xsize || (uint32_t)y >= ysize || (uint32_t)z >= zsize) return;
  uint32_t dofs = getDOfs(x, y);
  uint32_t prevdofs = 0;
  while (dofs) {
    if (data/*.ptr()*/[dofs].z == (uint16_t)z) {
      // replace this voxel
      data/*.ptr()*/[dofs].b = rgb&0xff;
      data/*.ptr()*/[dofs].g = (rgb>>8)&0xff;
      data/*.ptr()*/[dofs].r = (rgb>>16)&0xff;
      data/*.ptr()*/[dofs].cull = cull;
      return;
    }
    if (data/*.ptr()*/[dofs].z > (uint16_t)z) break;
    prevdofs = dofs;
    dofs = data/*.ptr()*/[dofs].nextz;
  }
  // insert before dofs
  const uint32_t vidx = allocVox();
  data/*.ptr()*/[vidx].b = rgb&0xff;
  data/*.ptr()*/[vidx].g = (rgb>>8)&0xff;
  data/*.ptr()*/[vidx].r = (rgb>>16)&0xff;
  data/*.ptr()*/[vidx].cull = cull;
  data/*.ptr()*/[vidx].z = (uint16_t)z;
  data/*.ptr()*/[vidx].nextz = dofs;
  if (prevdofs) {
    vassert(data/*.ptr()*/[prevdofs].nextz == dofs);
    data/*.ptr()*/[prevdofs].nextz = vidx;
  } else {
    xyofs[(uint32_t)y*xsize+(uint32_t)x] = vidx;
  }
}


//==========================================================================
//
//  VoxelData::checkInvariants
//
//==========================================================================
void VoxelData::checkInvariants () {
  uint32_t voxcount = 0;
  for (uint32_t y = 0; y < ysize; ++y) {
    for (uint32_t x = 0; x < xsize; ++x) {
      uint32_t dofs = getDOfs(x, y);
      if (!dofs) continue;
      ++voxcount;
      uint16_t prevz = data/*.ptr()*/[dofs].z;
      dofs = data/*.ptr()*/[dofs].nextz;
      while (dofs) {
        ++voxcount;
        vassert(prevz < data/*.ptr()*/[dofs].z); //, "broken voxel data Z invariant");
        prevz = data/*.ptr()*/[dofs].z;
        dofs = data/*.ptr()*/[dofs].nextz;
      }
    }
  }
  vassert(voxcount == voxpixtotal);//, "invalid number of voxels");
}


//==========================================================================
//
//  VoxelData::removeEmptyVoxels
//
//==========================================================================
void VoxelData::removeEmptyVoxels () {
  uint32_t count = 0;
  for (uint32_t y = 0; y < ysize; ++y) {
    for (uint32_t x = 0; x < xsize; ++x) {
      uint32_t dofs = getDOfs(x, y);
      if (!dofs) continue;
      uint32_t prevdofs = 0;
      while (dofs) {
        if (!data/*.ptr()*/[dofs].cull) {
          // remove it
          const uint32_t ndofs = data/*.ptr()*/[dofs].nextz;
          if (prevdofs) {
            data/*.ptr()*/[prevdofs].nextz = ndofs;
          } else {
            xyofs[(uint32_t)y*xsize+(uint32_t)x] = ndofs;
          }
          data/*.ptr()*/[dofs].nextz = freelist;
          freelist = dofs;
          --voxpixtotal;
          dofs = ndofs;
          ++count;
        } else {
          prevdofs = dofs;
          dofs = data/*.ptr()*/[dofs].nextz;
        }
      }
    }
  }
  if (count && voxlib_verbose) {
    vox_logf(VoxLibMsg_Normal, "removed %s empty voxel%s", vox_comatoze(count), (count != 1 ? "s" : ""));
  }
}


//==========================================================================
//
//  VoxelData::removeInsideFaces
//
//  remove inside voxels, leaving only contour
//
//==========================================================================
void VoxelData::removeInsideFaces () {
  for (uint32_t y = 0; y < ysize; ++y) {
    for (uint32_t x = 0; x < xsize; ++x) {
      for (uint32_t dofs = getDOfs(x, y); dofs; dofs = data/*.ptr()*/[dofs].nextz) {
        if (!data/*.ptr()*/[dofs].cull) continue;
        // check
        const int z = (int)data/*.ptr()*/[dofs].z;
        for (uint32_t cidx = 0; cidx < 6; ++cidx) {
          // go in this dir, removing the corresponding voxel side
          const uint8_t cmask = cullmask(cidx);
          const uint8_t opmask = cullopmask(cidx);
          const uint8_t checkmask = cmask|opmask;
          const int dx = cullofs[cidx][0];
          const int dy = cullofs[cidx][1];
          const int dz = cullofs[cidx][2];
          int vx = x, vy = y, vz = z;
          uint32_t myofs = dofs;
          while (myofs && (data/*.ptr()*/[myofs].cull&cmask)) {
            const int sx = vx+dx;
            const int sy = vy+dy;
            const int sz = vz+dz;
            const uint32_t sofs = voxofs(sx, sy, sz);
            if (!sofs) break;
            if (!(data/*.ptr()*/[sofs].cull&checkmask)) break;
            // fix culls
            data/*.ptr()*/[myofs].cull ^= cmask;
            data/*.ptr()*/[sofs].cull &= (uint8_t)(~(uint32_t)opmask);
            vx = sx;
            vy = sy;
            vz = sz;
            myofs = sofs;
          }
        }
      }
    }
  }
}


//==========================================================================
//
//  VoxelData::fixFaceVisibility
//
//  if we have ANY voxel at the corresponding side, don't render that face
//  returns number of fixed voxels
//
//==========================================================================
uint32_t VoxelData::fixFaceVisibility () {
  uint32_t count = 0;
  for (uint32_t y = 0; y < ysize; ++y) {
    for (uint32_t x = 0; x < xsize; ++x) {
      for (uint32_t dofs = getDOfs(x, y); dofs; dofs = data/*.ptr()*/[dofs].nextz) {
        const uint8_t ocull = data/*.ptr()*/[dofs].cull;
        if (!ocull) continue;
        const int z = (int)data/*.ptr()*/[dofs].z;
        // if we have ANY voxel at the corresponding side, don't render that face
        for (uint32_t cidx = 0; cidx < 6; ++cidx) {
          const uint8_t cmask = cullmask(cidx);
          if (data/*.ptr()*/[dofs].cull&cmask) {
            if (queryCull(x+cullofs[cidx][0], y+cullofs[cidx][1], z+cullofs[cidx][2])) {
              data/*.ptr()*/[dofs].cull ^= cmask; // reset bit
            }
          }
        }
        count += (data/*.ptr()*/[dofs].cull != ocull);
      }
    }
  }
  return count;
}


//==========================================================================
//
//  VoxelData::create3DBitmap
//
//==========================================================================
void VoxelData::create3DBitmap (Vox3DBitmap &bmp) {
  bmp.setSize(xsize, ysize, zsize);
  for (uint32_t y = 0; y < ysize; ++y) {
    for (uint32_t x = 0; x < xsize; ++x) {
      for (uint32_t dofs = getDOfs(x, y); dofs; dofs = data/*.ptr()*/[dofs].nextz) {
        if (data/*.ptr()*/[dofs].cull) bmp.setPixel(x, y, (int)data/*.ptr()*/[dofs].z);
      }
    }
  }
}


//==========================================================================
//
//  VoxelData::hollowFill
//
//  this fills everything outside of the voxel, and
//  then resets culling bits for all invisible faces
//  i don't care about memory yet
//
//==========================================================================
uint32_t VoxelData::hollowFill () {
  Vox3DBitmap bmp;
  bmp.setSize(xsize+2, ysize+2, zsize+2);

  VoxLibArray<VoxXYZ16> stack;
  uint32_t stackpos;

  stack.setLength(32768);
  stackpos = 0;
  vassert(xsize <= (uint32_t)stack.length());

  // this is definitely empty
  VoxXYZ16 xyz;
  xyz.x = xyz.y = xyz.z = 0;
  bmp.setPixel((int)xyz.x, (int)xyz.y, (int)xyz.z);
  stack/*.ptr()*/[stackpos++] = xyz;

  while (stackpos) {
    xyz = stack/*.ptr()*/[--stackpos];
    for (uint32_t dd = 0; dd < 6; ++dd) {
      const int nx = (int)xyz.x+cullofs[dd][0];
      const int ny = (int)xyz.y+cullofs[dd][1];
      const int nz = (int)xyz.z+cullofs[dd][2];
      if (bmp.setPixel(nx, ny, nz)) continue;
      if (queryCull(nx-1, ny-1, nz-1)) continue;
      if (stackpos == (uint32_t)stack.length()) {
        stack.setLength(stack.length()+32768);
      }
      stack/*.ptr()*/[stackpos++] = VoxXYZ16((uint16_t)nx, (uint16_t)ny, (uint16_t)nz);
    }
  }
  if (voxlib_verbose >= VoxLibMsg_Debug) {
    vox_logf(VoxLibMsg_Debug, "*** hollow fill used %s stack items", vox_comatoze(stack.length()));
  }

  // unmark contour voxels
  // this is required for proper face removing
  for (uint32_t y = 0; y < ysize; ++y) {
    for (uint32_t x = 0; x < xsize; ++x) {
      for (uint32_t dofs = getDOfs(x, y); dofs; dofs = data/*.ptr()*/[dofs].nextz) {
        if (!data/*.ptr()*/[dofs].cull) continue;
        const int z = (int)data/*.ptr()*/[dofs].z;
        bmp.resetPixel(x+1, y+1, z+1);
      }
    }
  }

  // now check it
  uint32_t changed = 0;
  for (uint32_t y = 0; y < ysize; ++y) {
    for (uint32_t x = 0; x < xsize; ++x) {
      for (uint32_t dofs = getDOfs(x, y); dofs; dofs = data/*.ptr()*/[dofs].nextz) {
        const uint8_t omask = data/*.ptr()*/[dofs].cull;
        if (!omask) continue;
        data/*.ptr()*/[dofs].cull = 0x3f;
        // check
        const int z = (int)data/*.ptr()*/[dofs].z;
        for (uint32_t cidx = 0; cidx < 6; ++cidx) {
          const uint8_t cmask = cullmask(cidx);
          if (!(data/*.ptr()*/[dofs].cull&cmask)) continue;
          const int nx = x+cullofs[cidx][0];
          const int ny = y+cullofs[cidx][1];
          const int nz = z+cullofs[cidx][2];
          if (bmp.getPixel(nx+1, ny+1, nz+1)) continue;
          // reset this cull bit
          data/*.ptr()*/[dofs].cull ^= cmask;
        }
        changed += (omask != data/*.ptr()*/[dofs].cull);
      }
    }
  }
  return changed;
}


//==========================================================================
//
//  VoxelData::optimise
//
//==========================================================================
void VoxelData::optimise (bool doHollowFill) {
  #ifdef VOXELIB_CHECK_INVARIANTS
  checkInvariants();
  #endif
  //version(voxdata_debug) conwriteln("optimising mesh with ", voxpixtotal, " individual voxels...");
  if (doHollowFill) {
    //version(voxdata_debug) conwriteln("optimising voxel culling...");
    uint32_t count = hollowFill();
    if (count && voxlib_verbose) {
      vox_logf(VoxLibMsg_Normal, "hollow fill fixed %s voxel%s", vox_comatoze(count), (count != 1 ? "s" : ""));
    }
    count = fixFaceVisibility();
    if (count && voxlib_verbose) {
      vox_logf(VoxLibMsg_Normal, "final fix fixed %s voxel%s", vox_comatoze(count), (count != 1 ? "s" : ""));
    }
  } else {
    removeInsideFaces();
    //version(voxdata_debug) conwriteln("optimising voxel culling...");
    uint32_t count = fixFaceVisibility();
    if (count && voxlib_verbose) {
      vox_logf(VoxLibMsg_Normal, "fixed %s voxel%s", vox_comatoze(count), (count != 1 ? "s" : ""));
    }
  }
  removeEmptyVoxels();
  #ifdef VOXELIB_CHECK_INVARIANTS
  checkInvariants();
  #endif
  if (voxlib_verbose) {
    vox_logf(VoxLibMsg_Normal, "final optimised mesh contains %s individual voxels", vox_comatoze(voxpixtotal));
  }
}


// ////////////////////////////////////////////////////////////////////////// //
/*
  voxel data optimised for queries
  (about two times faster than `VoxelData`, but immutable)

  each slab is kept in this format:
    ushort zlo, zhi
    ushort runcount
  then run index follows:
    ushort z0
    ushort z1+1
    ushort ofs (from this z)
    ushort reserved
  index always ends with zhi+1, zhi+1
  then (b,g,r,cull) array
 */
struct VoxelDataSmall {
public:
  uint32_t xsize = 0, ysize = 0, zsize = 0;
  float cx = 0.0f, cy = 0.0f, cz = 0.0f;

  VoxLibArray<uint8_t> data;
  // xsize*ysize array, offsets in `data`; 0 means "no data here"
  // slabs are sorted from bottom to top, and never intersects
  VoxLibArray<uint32_t> xyofs;

public:
  VoxelDataSmall ()
    : xsize(0)
    , ysize(0)
    , zsize(0)
    , cx(0.0f)
    , cy(0.0f)
    , cz(0.0f)
  {}

private:
  inline void appendByte (uint8_t v) {
    data.append(v);
  }

  inline void appendShort (uint16_t v) {
    data.append((uint8_t)v);
    data.append((uint8_t)(v>>8));
  }

private:
  uint32_t createSlab (VoxelData &vox, uint32_t dofs0);

  void checkValidity (VoxelData &vox);

public:
  void clear () {
    data.clear();
    xyofs.clear();
    xsize = ysize = zsize = 0;
    cx = cy = cz = 0.0f;
  }

  void createFrom (VoxelData &vox);

  uint32_t queryVox (int x, int y, int z);
};


//==========================================================================
//
//  VoxelDataSmall::checkValidity
//
//==========================================================================
void VoxelDataSmall::checkValidity (VoxelData &vox) {
  for (uint32_t y = 0; y < ysize; ++y) {
    for (uint32_t x = 0; x < xsize; ++x) {
      for (uint32_t z = 0; z < zsize; ++z) {
        const uint32_t vd = vox.query(x, y, z);
        if (vd != queryVox(x, y, z)) voxlib_fatal("internal error in compressed voxel data");
      }
    }
  }
}


//==========================================================================
//
//  VoxelDataSmall::createSlab
//
//==========================================================================
uint32_t VoxelDataSmall::createSlab (VoxelData &vox, uint32_t dofs0) {
  while (dofs0 && !vox.data/*.ptr()*/[dofs0].cull) dofs0 = vox.data/*.ptr()*/[dofs0].nextz;
  if (!dofs0) return 0;
  // calculate zlo and zhi, and count runs
  uint16_t runcount = 0;
  uint16_t z0 = vox.data/*.ptr()*/[dofs0].z;
  uint16_t z1 = z0;
  uint16_t nxz = (uint16_t)(z0-1);
  for (uint32_t dofs = dofs0; dofs; dofs = vox.data/*.ptr()*/[dofs].nextz) {
    if (!vox.data/*.ptr()*/[dofs].cull) continue;
    z1 = vox.data/*.ptr()*/[dofs].z;
    if (z1 != nxz) ++runcount;
    nxz = (uint16_t)(z1+1);
  }
  vassert(runcount);
  if (data.length() == 0) appendByte(0); // unused
  const uint32_t startofs = (uint32_t)data.length();
  // zlo
  appendShort(z0);
  // zhi
  appendShort(z1);
  // runcount
  appendShort(runcount);
  // run index (will be filled later)
  uint32_t idxofs = (uint32_t)data.length();
  for (uint32_t f = 0; f < runcount; ++f) {
    appendShort(0); // z0
    appendShort(0); // z1
    appendShort(0); // offset
    appendShort(0); // reserved
  }
  // last index item
  appendShort((uint16_t)(z1+1));
  appendShort((uint16_t)(z1+1));
  appendShort(0); // offset
  appendShort(0); // reserved
  nxz = (uint16_t)(z0-1);
  uint16_t lastz = 0xffffU;
  // put runs
  for (uint32_t dofs = dofs0; dofs; dofs = vox.data/*.ptr()*/[dofs].nextz) {
    if (!vox.data/*.ptr()*/[dofs].cull) continue;
    z1 = vox.data/*.ptr()*/[dofs].z;
    if (z1 != nxz) {
      // new run
      // fix prev run?
      if (lastz != 0xffffU) {
        data/*.ptr()*/[idxofs-6] = (uint8_t)lastz;
        data/*.ptr()*/[idxofs-5] = (uint8_t)(lastz>>8);
      }
      // set run info
      // calculate offset
      const uint32_t rofs = (uint32_t)data.length()-idxofs;
      vassert(rofs <= 0xffffU);
      // z0
      data/*.ptr()*/[idxofs++] = (uint8_t)z1;
      data/*.ptr()*/[idxofs++] = (uint8_t)(z1>>8);
      // skip z1
      idxofs += 2;
      // offset
      data/*.ptr()*/[idxofs++] = (uint8_t)rofs;
      data/*.ptr()*/[idxofs++] = (uint8_t)(rofs>>8);
      // skip reserved
      idxofs += 2;
    }
    lastz = nxz = (uint16_t)(z1+1);
    // b, g, r, cull
    appendByte(vox.data/*.ptr()*/[dofs].b);
    appendByte(vox.data/*.ptr()*/[dofs].g);
    appendByte(vox.data/*.ptr()*/[dofs].r);
    appendByte(vox.data/*.ptr()*/[dofs].cull);
  }
  // fix prev run?
  vassert(lastz != 0xffffU);
  data/*.ptr()*/[idxofs-6] = (uint8_t)lastz;
  data/*.ptr()*/[idxofs-5] = (uint8_t)(lastz>>8);
  return startofs;
}


//==========================================================================
//
//  VoxelDataSmall::createFrom
//
//==========================================================================
void VoxelDataSmall::createFrom (VoxelData &vox) {
  clear();
  xsize = vox.xsize;
  ysize = vox.ysize;
  zsize = vox.zsize;
  xyofs.setLength(xsize*ysize);
  cx = vox.cx;
  cy = vox.cy;
  cz = vox.cz;
  for (uint32_t y = 0; y < ysize; ++y) {
    for (uint32_t x = 0; x < xsize; ++x) {
      const uint32_t dofs = createSlab(vox, vox.getDOfs(x, y));
      xyofs[(uint32_t)y*xsize+(uint32_t)x] = dofs;
    }
  }
  checkValidity(vox);
}


//==========================================================================
//
//  VoxelDataSmall::queryVox
//
//==========================================================================
uint32_t VoxelDataSmall::queryVox (int x, int y, int z) {
  //pragma(inline, true);
  if (x < 0 || y < 0 || z < 0) return 0;
  if ((uint32_t)x >= xsize || (uint32_t)y >= ysize) return 0;
  uint32_t dofs = xyofs/*.ptr()*/[(uint32_t)y*xsize+(uint32_t)x];
  if (!dofs) return 0;
  const uint16_t *dptr = (const uint16_t *)(data.ptr()+dofs);
  if ((uint16_t)z < *dptr++) return 0;
  if ((uint16_t)z > *dptr++) return 0;
  uint32_t runcount = *dptr++;
  if (runcount <= 4) {
    // there is no reason to perform binary search here
    while ((uint16_t)z > *dptr) dptr += 4;
    if (z == *dptr) {
      const uint32_t *dv = (const uint32_t *)(((const uint8_t *)dptr)+dptr[2]);
      return *dv;
    } else {
      dptr -= 4;
      const uint16_t cz = *dptr;
      vassert(cz < z);
      if ((uint16_t)z >= dptr[1]) return 0; // no such voxel
      const uint32_t *dv = (const uint32_t *)(((const uint8_t *)dptr)+dptr[2]);
      return *(dv+z-cz);
    }
  } else {
    // perform binary search
    uint32_t lo = 0, hi = runcount-1;
    for (;;) {
      uint32_t mid = (lo+hi)>>1;
      const uint16_t *dp = dptr+(mid<<2);
      if ((uint16_t)z >= dp[0] && (uint16_t)z < dp[1]) {
        const uint32_t *dv = (const uint32_t *)(((const uint8_t *)dp)+dp[2]);
        return *(dv+z-*dp);
      }
      if ((uint16_t)z < dp[0]) {
        if (mid == lo) break;
        hi = mid-1;
      } else {
        if (mid == hi) { lo = hi; break; }
        lo = mid+1;
      }
    }
    const uint16_t *dp = dptr+(lo<<2);
    while ((uint16_t)z >= dp[1]) dp += 4;
    if ((uint16_t)z < dp[0]) return 0;
    const uint32_t *dv = (const uint32_t *)(((const uint8_t *)dp)+dp[2]);
    return *(dv+z-*dp);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// VoxelMesh
// ////////////////////////////////////////////////////////////////////////// //

const uint8_t VoxelMesh::quadFaces[6][4] = {
  // right (&0x01) (right)
  {
    X1_Y1_Z0,
    X1_Y0_Z0,
    X1_Y0_Z1,
    X1_Y1_Z1,
  },
  // left (&0x02) (left)
  {
    X0_Y0_Z0,
    X0_Y1_Z0,
    X0_Y1_Z1,
    X0_Y0_Z1,
  },
  // top (&0x04) (near)
  {
    X0_Y0_Z0,
    X0_Y0_Z1,
    X1_Y0_Z1,
    X1_Y0_Z0,
  },
  // bottom (&0x08) (far)
  {
    X1_Y1_Z0,
    X1_Y1_Z1,
    X0_Y1_Z1,
    X0_Y1_Z0,
  },
  // back (&0x10)  (top)
  {
    X0_Y1_Z1,
    X1_Y1_Z1,
    X1_Y0_Z1,
    X0_Y0_Z1,
  },
  // front (&0x20)  (bottom)
  {
    X0_Y0_Z0,
    X1_Y0_Z0,
    X1_Y1_Z0,
    X0_Y1_Z0,
  }
};


const float VoxelMesh::quadNormals[6][4] = {
  // right (&0x01)
  { 1.0f, 0.0f, 0.0f},
  // left (&0x02)
  {-1.0f, 0.0f, 0.0f},
  // near (&0x04)
  { 0.0f,-1.0f, 0.0f},
  // far (&0x08)
  { 0.0f, 1.0f, 0.0f},
  // top (&0x10)
  { 0.0f, 0.0f, 1.0f},
  // bottom (&0x20)
  { 0.0f, 0.0f,-1.0f},
};


//==========================================================================
//
//  VoxelMesh::clear
//
//==========================================================================
void VoxelMesh::clear () {
  quads.clear();
  catlas.clear();
  cx = cy = cz = 0.0f;
}



//==========================================================================
//
//  VoxelMesh::setColors
//
//==========================================================================
void VoxelMesh::setColors (VoxQuad &vq, const uint32_t *clrs, uint32_t wdt, uint32_t hgt) {
  if (catlas.findRect(clrs, wdt, hgt, &vq.cidx, &vq.wh)) {
    /*
    conwriteln("  ...reused rect: (", catlas.getTexX(vq.cidx, vq.rc), ",",
               catlas.getTexY(vq.cidx, vq.rc), ")-(", wdt, "x", hgt, ")");
    */
    return;
  }
  vq.cidx = catlas.addNewRect(clrs, wdt, hgt);
  vq.wh = VoxWH16(wdt, hgt);
}


// ////////////////////////////////////////////////////////////////////////// //
// VoxelMesh
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VoxelMesh::quadCalcNormal
//
//==========================================================================
void VoxelMesh::quadCalcNormal (VoxQuad &vq) {
  uint32_t qidx;
  switch (vq.cull) {
    case 0x01: qidx = 0; break;
    case 0x02: qidx = 1; break;
    case 0x04: qidx = 2; break;
    case 0x08: qidx = 3; break;
    case 0x10: qidx = 4; break;
    case 0x20: qidx = 5; break;
    default: vassert(0);
  }
  vq.normal.x = vq.normal.dx = quadNormals[qidx][0];
  vq.normal.y = vq.normal.dy = quadNormals[qidx][1];
  vq.normal.z = vq.normal.dz = quadNormals[qidx][2];
  vq.normal.qtype = 0xff;
}


//==========================================================================
//
//  VoxelMesh::addSlabFace
//
//  dmv: bit 2 means XLong, bit 1 means YLong, bit 0 means ZLong
//
//==========================================================================
void VoxelMesh::addSlabFace (uint8_t cull, uint8_t dmv,
                             float x, float y, float z,
                             int len, const uint32_t *colors)
{
  if (len < 1) return;
  vassert(dmv == DMV_X || dmv == DMV_Y || dmv == DMV_Z);
  vassert(cull == 0x01 || cull == 0x02 || cull == 0x04 || cull == 0x08 || cull == 0x10 || cull == 0x20);

  bool allsame = (len == 1);
  if (!allsame) {
    for (int cidx = 1; cidx < len; ++cidx) {
      if (colors[cidx] != colors[0]) {
        allsame = false;
        break;
      }
    }
  }
  //if (allsame) colors = colors[0..1];

  const int qtype =
    allsame ? Point :
    (dmv&DMV_X) ? XLong :
    (dmv&DMV_Y) ? YLong :
    ZLong;
  const float dx = (dmv&DMV_X ? (float)len : 1.0f);
  const float dy = (dmv&DMV_Y ? (float)len : 1.0f);
  const float dz = (dmv&DMV_Z ? (float)len : 1.0f);
  uint32_t qidx;
  switch (cull) {
    case 0x01: qidx = 0; break;
    case 0x02: qidx = 1; break;
    case 0x04: qidx = 2; break;
    case 0x08: qidx = 3; break;
    case 0x10: qidx = 4; break;
    case 0x20: qidx = 5; break;
    default: vassert(0);
  }
  VoxQuad vq;
  for (uint32_t vidx = 0; vidx < 4; ++vidx) {
    vq.vx[vidx] = genVertex(quadFaces[qidx][vidx], x, y, z, dx, dy, dz);
  }
  setColors(vq, colors, (uint32_t)(allsame ? 1 : len), 1);

  vq.type = qtype;
  vq.cull = cull;
  quadCalcNormal(vq);
  quads.append(vq);
}


//==========================================================================
//
//  VoxelMesh::addCube
//
//==========================================================================
void VoxelMesh::addCube (uint8_t cull, float x, float y, float z, uint32_t rgb) {
  // generate quads
  for (uint32_t qidx = 0; qidx < 6; ++qidx) {
    const uint8_t cmask = VoxelData::cullmask(qidx);
    if (cull&cmask) {
      addSlabFace(cmask, DMV_X/*doesn't matter*/, x, y, z, 1, &rgb);
    }
  }
}


//==========================================================================
//
//  VoxelMesh::addQuad
//
//==========================================================================
void VoxelMesh::addQuad (uint8_t cull,
                         float x, float y, float z,
                         int wdt, int hgt, // quad size
                         const uint32_t *colors)
{
  vassert(wdt > 0 && hgt > 0);
  vassert(cull == 0x01 || cull == 0x02 || cull == 0x04 || cull == 0x08 || cull == 0x10 || cull == 0x20);

  bool allsame = (wdt == 1 && hgt == 1);
  if (!allsame) {
    const int csz = wdt*hgt;
    for (auto cidx = 1; cidx < csz; ++cidx) {
      if (colors[cidx] != colors[0]) {
        allsame = false;
        break;
      }
    }
  }
  //if (allsame) colors = colors[0..1];

  const int qtype = Quad;
  uint32_t qidx;
  switch (cull) {
    case 0x01: qidx = 0; break;
    case 0x02: qidx = 1; break;
    case 0x04: qidx = 2; break;
    case 0x08: qidx = 3; break;
    case 0x10: qidx = 4; break;
    case 0x20: qidx = 5; break;
    default: vassert(0);
  }

  VoxQuad vq;
  for (uint32_t vidx = 0; vidx < 4; ++vidx) {
    const uint8_t vtype = quadFaces[qidx][vidx];
    VoxQuadVertex vx;
    vx.qtype = vtype;
    vx.dx = vx.dy = vx.dz = 0.0f;
    vx.x = x;
    vx.y = y;
    vx.z = z;
    if (cull&Cull_ZAxisMask) {
      if (vtype&DMV_X) vx.dx = (float)wdt;
      if (vtype&DMV_Y) vx.dy = (float)hgt;
      if (vtype&DMV_Z) vx.dz = 1.0f;
    } else if (cull&Cull_XAxisMask) {
      if (vtype&DMV_X) vx.dx = 1.0f;
      if (vtype&DMV_Y) vx.dy = (float)wdt;
      if (vtype&DMV_Z) vx.dz = (float)hgt;
    } else if (cull&Cull_YAxisMask) {
      if (vtype&DMV_X) vx.dx = (float)wdt;
      if (vtype&DMV_Y) vx.dy = 1.0f;
      if (vtype&DMV_Z) vx.dz = (float)hgt;
    } else {
      vassert(0);
    }
    vx.x += vx.dx;
    vx.y += vx.dy;
    vx.z += vx.dz;
    vq.vx[vidx] = vx;
  }

  if (allsame) {
    setColors(vq, colors, 1, 1);
  } else {
    setColors(vq, colors, wdt, hgt);
  }

  vq.type = qtype;
  vq.cull = cull;
  quadCalcNormal(vq);
  quads.append(vq);
}


//==========================================================================
//
//  VoxelMesh::buildOpt0
//
//==========================================================================
void VoxelMesh::buildOpt0 (VoxelData &vox) {
  if (voxlib_verbose) {
    vox_logf(VoxLibMsg_Normal, "method: quad per face...");
  }
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = vox.cz;
  for (int y = 0; y < (int)vox.ysize; ++y) {
    for (int x = 0; x < (int)vox.xsize; ++x) {
      uint32_t dofs = vox.getDOfs(x, y);
      while (dofs) {
        addCube(vox.data/*.ptr()*/[dofs].cull, x-px, y-py, vox.data/*.ptr()*/[dofs].z-pz, vox.data/*.ptr()*/[dofs].rgb());
        dofs = vox.data/*.ptr()*/[dofs].nextz;
      }
    }
  }
}


//==========================================================================
//
//  VoxelMesh::buildOpt1
//
//==========================================================================
void VoxelMesh::buildOpt1 (VoxelData &vox) {
  if (voxlib_verbose) {
    vox_logf(VoxLibMsg_Normal, "method: quad per vertical slab...");
  }
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = vox.cz;

  uint32_t slab[1024];

  for (int y = 0; y < (int)vox.ysize; ++y) {
    for (int x = 0; x < (int)vox.xsize; ++x) {
      // try slabs in all 6 directions?
      uint32_t dofs = vox.getDOfs(x, y);
      if (!dofs) continue;

      // long top and bottom quads
      while (dofs) {
        for (uint32_t cidx = 4; cidx < 6; ++cidx) {
          const uint8_t cmask = VoxelData::cullmask(cidx);
          if ((vox.data/*.ptr()*/[dofs].cull&cmask) == 0) continue;
          const int z = (int)vox.data/*.ptr()*/[dofs].z;
          slab[0] = vox.data/*.ptr()*/[dofs].rgb();
          addSlabFace(cmask, DMV_X, x-px, y-py, z-pz, 1, slab);
        }
        dofs = vox.data/*.ptr()*/[dofs].nextz;
      }

      // build long quads for each side
      for (uint32_t cidx = 0; cidx < 4; ++cidx) {
        const uint8_t cmask = VoxelData::cullmask(cidx);
        dofs = vox.getDOfs(x, y);
        while (dofs) {
          while (dofs && (vox.data/*.ptr()*/[dofs].cull&cmask) == 0) dofs = vox.data/*.ptr()*/[dofs].nextz;
          if (!dofs) break;
          const int z = (int)vox.data/*.ptr()*/[dofs].z;
          int count = 0;
          uint32_t eofs = dofs;
          while (eofs && (vox.data/*.ptr()*/[eofs].cull&cmask)) {
            if ((int)vox.data/*.ptr()*/[eofs].z != z+count) break;
            vox.data/*.ptr()*/[eofs].cull ^= cmask;
            slab[count] = vox.data/*.ptr()*/[eofs].rgb();
            eofs = vox.data/*.ptr()*/[eofs].nextz;
            ++count;
            if (count == (int)(sizeof(slab)/sizeof(slab[0]))) break;
          }
          vassert(count);
          dofs = eofs;
          addSlabFace(cmask, DMV_Z, x-px, y-py, z-pz, count, slab);
        }
      }
    }
  }
}


//==========================================================================
//
//  VoxelMesh::buildOpt2
//
//==========================================================================
void VoxelMesh::buildOpt2 (VoxelData &vox) {
  if (voxlib_verbose) {
    vox_logf(VoxLibMsg_Normal, "method: quad per vertical slab, top and bottom slabs...");
  }
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = vox.cz;

  uint32_t slab[1024];

  for (int y = 0; y < (int)vox.ysize; ++y) {
    for (int x = 0; x < (int)vox.xsize; ++x) {
      // try slabs in all 6 directions?
      uint32_t dofs = vox.getDOfs(x, y);
      if (!dofs) continue;

      // long top and bottom quads
      while (dofs) {
        for (uint32_t cidx = 4; cidx < 6; ++cidx) {
          const uint8_t cmask = VoxelData::cullmask(cidx);
          if ((vox.data/*.ptr()*/[dofs].cull&cmask) == 0) continue;
          const int z = (int)vox.data/*.ptr()*/[dofs].z;
          vassert(vox.queryCull(x, y, z) == vox.data/*.ptr()*/[dofs].cull);
          // by x
          int xcount = 0;
          while (x+xcount < (int)vox.xsize) {
            const uint8_t vcull = vox.queryCull(x+xcount, y, z);
            if ((vcull&cmask) == 0) break;
            ++xcount;
          }
          // by y
          int ycount = 0;
          while (y+ycount < (int)vox.ysize) {
            const uint8_t vcull = vox.queryCull(x, y+ycount, z);
            if ((vcull&cmask) == 0) break;
            ++ycount;
          }
          vassert(xcount && ycount);
          // now use the longest one
          if (xcount >= ycount) {
            xcount = 0;
            while (x+xcount < (int)vox.xsize) {
              const uint32_t vrgb = vox.query(x+xcount, y, z);
              if (((vrgb>>24)&cmask) == 0) break;
              slab[xcount] = vrgb|0xff000000U;
              vox.setVoxelCull(x+xcount, y, z, (vrgb>>24)^cmask);
              ++xcount;
            }
            vassert(xcount);
            addSlabFace(cmask, DMV_X, x-px, y-py, z-pz, xcount, slab);
          } else {
            ycount = 0;
            while (y+ycount < (int)vox.ysize) {
              const uint32_t vrgb = vox.query(x, y+ycount, z);
              if (((vrgb>>24)&cmask) == 0) break;
              slab[ycount] = vrgb|0xff000000U;
              vox.setVoxelCull(x, y+ycount, z, (vrgb>>24)^cmask);
              ++ycount;
            }
            vassert(ycount);
            addSlabFace(cmask, DMV_Y, x-px, y-py, z-pz, ycount, slab);
          }
        }
        dofs = vox.data/*.ptr()*/[dofs].nextz;
      }

      // build long quads for each side
      for (uint32_t cidx = 0; cidx < 4; ++cidx) {
        const uint8_t cmask = VoxelData::cullmask(cidx);
        dofs = vox.getDOfs(x, y);
        while (dofs) {
          while (dofs && (vox.data/*.ptr()*/[dofs].cull&cmask) == 0) dofs = vox.data/*.ptr()*/[dofs].nextz;
          if (!dofs) break;
          const int z = (int)vox.data/*.ptr()*/[dofs].z;
          int count = 0;
          uint32_t eofs = dofs;
          while (eofs && (vox.data/*.ptr()*/[eofs].cull&cmask)) {
            if ((int)vox.data/*.ptr()*/[eofs].z != z+count) break;
            vox.data/*.ptr()*/[eofs].cull ^= cmask;
            slab[count] = vox.data/*.ptr()*/[eofs].rgb();
            eofs = vox.data/*.ptr()*/[eofs].nextz;
            ++count;
            if (count == (int)(sizeof(slab)/sizeof(slab[0]))) break;
          }
          vassert(count);
          dofs = eofs;
          addSlabFace(cmask, DMV_Z, x-px, y-py, z-pz, count, slab);
        }
      }
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
static inline int getDX (uint8_t dmv) { return !!(dmv&VoxelMesh::DMV_X); }
static inline int getDY (uint8_t dmv) { return !!(dmv&VoxelMesh::DMV_Y); }
static inline int getDZ (uint8_t dmv) { return !!(dmv&VoxelMesh::DMV_Z); }

static inline void incXYZ (uint8_t dmv, int &sx, int &sy, int &sz) {
  sx += getDX(dmv);
  sy += getDY(dmv);
  sz += getDZ(dmv);
}


//==========================================================================
//
//  VoxelMesh::buildOpt3
//
//==========================================================================
void VoxelMesh::buildOpt3 (VoxelData &vox) {
  if (voxlib_verbose) {
    vox_logf(VoxLibMsg_Normal, "method: quad per slab in any direction...");
  }
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = vox.cz;

  // try slabs in all 6 directions?
  uint32_t slab[1024];

  const uint8_t dmove[3][2] = {
    {DMV_Y, DMV_Z}, // left, right
    {DMV_X, DMV_Z}, // near, far
    {DMV_X, DMV_Y}, // top, bottom
  };

  for (int y = 0; y < (int)vox.ysize; ++y) {
    for (int x = 0; x < (int)vox.xsize; ++x) {
      for (uint32_t dofs = vox.getDOfs(x, y); dofs; dofs = vox.data/*.ptr()*/[dofs].nextz) {
        while (vox.data/*.ptr()*/[dofs].cull) {
          uint32_t count = 0;
          uint8_t clrdmv = 0;
          uint8_t clrmask = 0;
          const int z = (int)vox.data/*.ptr()*/[dofs].z;
          // check all faces
          for (uint32_t cidx = 0; cidx < 6; ++cidx) {
            const uint8_t cmask = VoxelData::cullmask(cidx);
            if ((vox.data/*.ptr()*/[dofs].cull&cmask) == 0) continue;
            // try two dirs
            for (uint32_t ndir = 0; ndir < 2; ++ndir) {
              const uint8_t dmv = dmove[cidx>>1][ndir];
              int cnt = 1;
              int sx = x, sy = y, sz = z;
              incXYZ(dmv, sx, sy, sz);
              for (;;) {
                const uint8_t vxc = vox.queryCull(sx, sy, sz);
                if ((vxc&cmask) == 0) break;
                ++cnt;
                incXYZ(dmv, sx, sy, sz);
              }
              if (cnt > (int)count) {
                count = cnt;
                clrdmv = dmv;
                clrmask = cmask;
              }
            }
          }
          if (clrmask) {
            vassert(count);
            vassert(clrdmv == DMV_X || clrdmv == DMV_Y || clrdmv == DMV_Z);
            int sx = x, sy = y, sz = z;
            for (uint32_t f = 0; f < count; ++f) {
              VoxPix *vp = vox.queryVP(sx, sy, sz);
              slab[f] = vp->rgb();
              vassert(vp->cull&clrmask);
              vp->cull ^= clrmask;
              incXYZ(clrdmv, sx, sy, sz);
            }
            addSlabFace(clrmask, clrdmv, x-px, y-py, z-pz, count, slab);
          }
        }
      }
    }
  }
}


//==========================================================================
//
//  VoxelMesh::buildOpt4
//
//  this tries to create big quads
//
//==========================================================================
void VoxelMesh::buildOpt4 (VoxelData &vox) {
  if (voxlib_verbose) {
    vox_logf(VoxLibMsg_Normal, "method: optimal quad fill...");
  }
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = vox.cz;

  VoxLibArray<uint32_t> slab;

  // for faster scans
  Vox3DBitmap bmp3d;
  vox.create3DBitmap(bmp3d);

  VoxelDataSmall vxopt;
  vxopt.createFrom(vox);

  Vox2DBitmap bmp2d;
  for (uint32_t cidx = 0; cidx < 6; ++cidx) {
    const uint8_t cmask = VoxelData::cullmask(cidx);

    uint32_t vwdt, vhgt, vlen;
    if (cmask&Cull_ZAxisMask) {
      vwdt = vox.xsize;
      vhgt = vox.ysize;
      vlen = vox.zsize;
    } else if (cmask&Cull_XAxisMask) {
      vwdt = vox.ysize;
      vhgt = vox.zsize;
      vlen = vox.xsize;
    } else {
      vwdt = vox.xsize;
      vhgt = vox.zsize;
      vlen = vox.ysize;
    }
    bmp2d.setSize(vwdt, vhgt);

    for (uint32_t vcrd = 0; vcrd < vlen; ++vcrd) {
      //bmp2d.clearBmp(); // no need to, it is guaranteed
      vassert(bmp2d.dotCount == 0);
      for (uint32_t vdy = 0; vdy < vhgt; ++vdy) {
        for (uint32_t vdx = 0; vdx < vwdt; ++vdx) {
          uint32_t vx, vy, vz;
               if (cmask&Cull_ZAxisMask) { vx = vdx; vy = vdy; vz = vcrd; }
          else if (cmask&Cull_XAxisMask) { vx = vcrd; vy = vdx; vz = vdy; }
          else { vx = vdx; vy = vcrd; vz = vdy; }
          //conwriteln("*vcrd=", vcrd, "; vdx=", vdx, "; vdy=", vdy);
          if (!bmp3d.getPixel(vx, vy, vz)) continue;
          const uint32_t vd = vxopt.queryVox(vx, vy, vz);
          if (((vd>>24)&cmask) == 0) continue;
          bmp2d.setPixel(vdx, vdy, vd|0xff000000U);
        }
      }
      //conwriteln(":: cidx=", cidx, "; vcrd=", vcrd, "; dotCount=", bmp2d.dotCount);
      if (bmp2d.dotCount == 0) continue;
      // ok, we have some dots, go create quads
      int x0, y0, x1, y1;
      while (bmp2d.doOne(&x0, &y0, &x1, &y1)) {
        const uint32_t cwdt = (x1-x0)+1;
        const uint32_t chgt = (y1-y0)+1;
        if (slab.length() < (int)(cwdt*chgt)) slab.setLength((int)((cwdt*chgt)|0xff)+1);
        // get colors
        uint32_t *dp = slab.ptr();
        for (int dy = y0; dy <= y1; ++dy) {
          for (int dx = x0; dx <= x1; ++dx) {
            *dp++ = bmp2d.resetPixel(dx, dy);
          }
        }
        float fx, fy, fz;
             if (cmask&Cull_ZAxisMask) { fx = x0; fy = y0; fz = vcrd; }
        else if (cmask&Cull_XAxisMask) { fx = vcrd; fy = x0; fz = y0; }
        else { fx = x0; fy = vcrd; fz = y0; }
        addQuad(cmask, fx-px, fy-py, fz-pz, cwdt, chgt, slab.ptr());
      }
    }
  }
}


//==========================================================================
//
//  VoxelMesh::createFrom
//
//==========================================================================
void VoxelMesh::createFrom (VoxelData &vox, int optlevel) {
  vassert(vox.xsize && vox.ysize && vox.zsize);
  // now build cubes
  //conwriteln("building slabs...");
  //auto tm = iv.timer.Timer(true);
  if (optlevel < 0) optlevel = 0;
  switch (optlevel) {
    case 0: buildOpt0(vox); break;
    case 1: buildOpt1(vox); break;
    case 2: buildOpt2(vox); break;
    case 3: buildOpt3(vox); break;
    case 4: default: buildOpt4(vox); break;
  }
  //tm.stop();
  //conwriteln("basic conversion: ", quads.length, " quads (", quads.length*2, " tris).");
  //conwriteln("converted in ", tm.toString(), "; optlevel is ", vox_optimisation, "/", kvx_max_optim_level);
  if (voxlib_verbose) {
    vox_logf(VoxLibMsg_Normal, "basic conversion: %s quads (%s tris)",
             vox_comatoze(quads.length()), vox_comatoze(quads.length()*2));
  }
  cx = vox.cx;
  cy = vox.cy;
  cz = vox.cz;
}


// ////////////////////////////////////////////////////////////////////////// //
// GLVoxelMesh
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  GLVoxelMesh::appendVertex
//
//==========================================================================
uint32_t GLVoxelMesh::appendVertex (VVoxVertexEx &gv) {
  ++totaladded;
  // normalize negative zeroes
  if (AlmostEquals(gv.x, 0.0f)) gv.x = 0.0f;
  if (AlmostEquals(gv.y, 0.0f)) gv.y = 0.0f;
  if (AlmostEquals(gv.z, 0.0f)) gv.z = 0.0f;
  if (AlmostEquals(gv.s, 0.0f)) gv.s = 0.0f;
  if (AlmostEquals(gv.t, 0.0f)) gv.t = 0.0f;
  if (AlmostEquals(gv.nx, 0.0f)) gv.nx = 0.0f;
  if (AlmostEquals(gv.ny, 0.0f)) gv.ny = 0.0f;
  if (AlmostEquals(gv.nz, 0.0f)) gv.nz = 0.0f;
  
  // check hashtable
  auto vp = vertcache.get(gv);
  if (vp) return *vp;
  const uint32_t res = (uint32_t)vertices.length();
  vertices.append(gv);
  vertcache.put(gv, res);
  // fix min coords
  if (vmin[0] > gv.x) vmin[0] = gv.x;
  if (vmin[1] > gv.y) vmin[1] = gv.y;
  if (vmin[2] > gv.z) vmin[2] = gv.z;
  // fix max coords
  if (vmax[0] < gv.x) vmax[0] = gv.x;
  if (vmax[1] < gv.y) vmax[1] = gv.y;
  if (vmax[2] < gv.z) vmax[2] = gv.z;
  return res;
}


//==========================================================================
//
//  GLVoxelMesh::clear
//
//==========================================================================
void GLVoxelMesh::clear () {
  vertices.clear();
  indices.clear();
  vertcache.clear();
  totaladded = 0;
  // our voxels are 1024x1024x1024 at max
  vmin[0] = vmin[1] = vmin[2] = +8192.0f;
  vmax[0] = vmax[1] = vmax[2] = -8192.0f;
  img.clear();
  imgWidth = imgHeight = 0;
}


/*
  here starts the t-junction fixing part
  probably the most complex piece of code here
  (because almost everything else is done elsewhere)

  the algorithm is very simple and fast, tho, because i can
  abuse the fact that vertices are always snapped onto the grid.
  so i simply created a bitmap that tells if there is any vertex
  at the given grid coords, and then walk over the edge, checking
  the bitmap, and adding the vertices. this is easy too, because
  all vertices are parallel to one of the coordinate axes; so no
  complex math required at all.

  another somewhat complex piece of code is triangle fan creator.
  there are four basic cases here.

  first: normal quad without any added vertices.
  we can simply copy its vertices, because such quad makes a valid fan.

  second: quad that have at least two edges without added vertices.
  we can use the shared vertex of those two edges as a starting point of
  a fan.

  third: two opposite edges have no added vertices.
  this can happen in "run conversion", and we can create two fans here.

  fourth: no above conditions are satisfied.
  this is the most complex case: to create a fan without degenerate triangles,
  we have to add a vertex in the center of the quad, and use it as a start of
  a triangle fan.

  note that we can always convert our triangle fans into triangle soup, so i
  didn't bothered to create a separate triangle soup code.
 */

//==========================================================================
//
//  GLVoxelMesh::freeSortStructs
//
//==========================================================================
void GLVoxelMesh::freeSortStructs () {
  gridbmp.clear();
}


//==========================================================================
//
//  GLVoxelMesh::createGrid
//
//==========================================================================
void GLVoxelMesh::createGrid () {
  // create the grid
  for (uint32_t f = 0; f < 3; ++f) {
    gridmin[f] = (int)vmin[f];
    gridmax[f] = (int)vmax[f];
  }
  uint32_t gxs = (uint32_t)(gridmax[0]-gridmin[0]+1);
  uint32_t gys = (uint32_t)(gridmax[1]-gridmin[1]+1);
  uint32_t gzs = (uint32_t)(gridmax[2]-gridmin[2]+1);
  /*
  conwriteln("vox dims: (", gridmin[0], ",", gridmin[1], ",", gridmin[2], ")-(",
             gridmax[0], ",", gridmax[1], ",", gridmax[2], ")");
  conwriteln("grid size: (", gxs, ",", gys, ",", gzs, ")");
  */
  gridbmp.setSize(gxs, gys, gzs);
}


//==========================================================================
//
//  GLVoxelMesh::sortEdges
//
//  create 3d grid, put edges into it
//
//==========================================================================
void GLVoxelMesh::sortEdges () {
  createGrid();
  for (uint32_t f = 0; f < (uint32_t)edges.length(); ++f) putEdgeToGrid(f);
}


//==========================================================================
//
//  GLVoxelMesh::createEdges
//
//  create list of edges
//
//==========================================================================
void GLVoxelMesh::createEdges () {
  addedlist.clear();
  edges.setLength(indices.length()/5*4); // one quad is 4 edges
  uint32_t eidx = 0;
  uint32_t uqcount = 0;
  for (uint32_t f = 0; f < (uint32_t)indices.length(); f += 5) {
    bool unitquad = true;
    for (uint32_t vx0 = 0; vx0 < 4; ++vx0) {
      const uint32_t vx1 = (vx0+1)&3;
      VoxEdge &e = edges[eidx++];
      memset((void *)&e, 0, sizeof(VoxEdge));
      e.morefirst = -1;
      e.v0 = indices[f+vx0];
      e.v1 = indices[f+vx1];
      if (!AlmostEquals(vertices[e.v0].x, vertices[e.v1].x)) {
        vassert(AlmostEquals(vertices[e.v0].y, vertices[e.v1].y));
        vassert(AlmostEquals(vertices[e.v0].z, vertices[e.v1].z));
        e.axis = AXIS_X;
      } else if (!AlmostEquals(vertices[e.v0].y, vertices[e.v1].y)) {
        vassert(AlmostEquals(vertices[e.v0].x, vertices[e.v1].x));
        vassert(AlmostEquals(vertices[e.v0].z, vertices[e.v1].z));
        e.axis = AXIS_Y;
      } else {
        vassert(AlmostEquals(vertices[e.v0].x, vertices[e.v1].x));
        vassert(AlmostEquals(vertices[e.v0].y, vertices[e.v1].y));
        vassert(!AlmostEquals(vertices[e.v0].z, vertices[e.v1].z));
        e.axis = AXIS_Z;
      }
      e.clo = vertices[e.v0].get(e.axis);
      e.chi = vertices[e.v1].get(e.axis);
      e.dir = e.chi-e.clo;
      if (unitquad) unitquad = (AlmostEquals(e.dir, +1.0f) || AlmostEquals(e.dir, -1.0f));
    }
    // "unit quads" can be ignored, they aren't interesting
    // also, each quad always have at least one "unit edge"
    // (this will be used to build triangle strips)
    uqcount += unitquad;
  }
  vassert(eidx == (uint32_t)edges.length());
  //conwriteln(uqcount, " unit quad", (uqcount != 1 ? "s" : ""), " found.");
}


//==========================================================================
//
//  GLVoxelMesh::fixEdgeWithVert
//
//==========================================================================
void GLVoxelMesh::fixEdgeWithVert (VoxEdge &edge, float crd) {
  // calculate time
  const float tm = (crd-edge.clo)/edge.dir;

  const VVoxVertexEx &evx0 = vertices[edge.v0];
  const VVoxVertexEx &evx1 = vertices[edge.v1];

  VVoxVertexEx nvx = evx0;
  // set coord
  nvx.set(edge.axis, crd);
  // calc new (s,t)
  nvx.s += (evx1.s-evx0.s)*tm;
  nvx.t += (evx1.t-evx0.t)*tm;

  // append vertex
  const int addidx = addedlist.length();
  AddedVert &av = addedlist.alloc();
  av.vidx = appendVertex(nvx);
  av.next = -1;
  int lastvx = edge.morefirst;
  if (lastvx >= 0) {
    while (addedlist[lastvx].next >= 0) lastvx = addedlist[lastvx].next;
    addedlist[lastvx].next = addidx;
  } else {
    edge.morefirst = addidx;
  }
}


//==========================================================================
//
//  GLVoxelMesh::fixEdgeNew
//
//  fix one edge
//
//==========================================================================
void GLVoxelMesh::fixEdgeNew (uint32_t eidx) {
  VoxEdge &edge = edges[eidx];
  if (AlmostEquals(edge.dir, +1.0f) || AlmostEquals(edge.dir, -1.0f)) return; // and here
  // check grid by the edge axis
  float gxyz[3];
  for (uint32_t f = 0; f < 3; ++f) gxyz[f] = vertices[edge.v0].get(f);
  const float step = (edge.dir < 0.0f ? -1.0f : +1.0f);
  if (fabs(gxyz[edge.axis]-edge.chi) > 0.00001f) {
    gxyz[edge.axis] += step;
    while (fabs(gxyz[edge.axis]-edge.chi) > 0.00001f) {
      if (hasVertexAt(gxyz[0], gxyz[1], gxyz[2])) {
        fixEdgeWithVert(edge, gxyz[edge.axis]);
      }
      gxyz[edge.axis] += step;
    }
  }
}


//==========================================================================
//
//  GLVoxelMesh::rebuildEdges
//
//==========================================================================
void GLVoxelMesh::rebuildEdges () {
  // now we have to rebuild quads
  // each quad will have at least two unmodified edges of unit length
  uint32_t newindcount = (uint32_t)edges.length()*5;
  for (uint32_t f = 0; f < (uint32_t)edges.length(); ++f) {
    uint32_t vcnt = 0;
    for (int avidx = edges[f].morefirst; avidx >= 0; avidx = addedlist[avidx].next) ++vcnt;
    if (vcnt) newindcount += vcnt+8;
  }
  indices.setLength(newindcount);

  newindcount = 0;
  for (uint32_t f = 0; f < (uint32_t)edges.length(); f += 4) {
    // check if this quad is modified at all
    if (edges[f+0].hasMore() ||
        edges[f+1].hasMore() ||
        edges[f+2].hasMore() ||
        edges[f+3].hasMore())
    {
      // this can be a quad that needs to be converted into a "centroid fan"
      // if we have at least two adjacent edges without extra points, we can use them
      // otherwise, we need to append a centroid
      int firstGoodFace = -1;
      for (uint32_t c = 0; c < 4; ++c) {
        if (edges[f+c].noMore() && edges[f+((c+1)&3)].noMore()) {
          // i found her!
          firstGoodFace = (int)c;
          break;
        }
      }

      // have two good faces?
      if (firstGoodFace >= 0) {
        // yay, we can use v1 of the first face as the start of the fan
        vassert(edges[f+firstGoodFace].noMore());
        indices[newindcount++] = edges[f+firstGoodFace].v1;
        // then v1 of the second good face
        firstGoodFace = (firstGoodFace+1)&3;
        vassert(edges[f+firstGoodFace].noMore());
        indices[newindcount++] = edges[f+firstGoodFace].v1;
        // then add points of the other two faces (ignoring v0)
        for (uint32_t c = 0; c < 2; ++c) {
          firstGoodFace = (firstGoodFace+1)&3;
          int avidx = edges[f+firstGoodFace].morefirst;
          while (avidx >= 0) {
            indices[newindcount++] = addedlist[avidx].vidx;
            avidx = addedlist[avidx].next;
          }
          /*
          for (uint32_t midx = 0; midx < (uint32_t)edges[f+firstGoodFace].moreverts.length(); ++midx) {
            indices[newindcount++] = edges[f+firstGoodFace].moreverts[midx];
          }
          */
          indices[newindcount++] = edges[f+firstGoodFace].v1;
        }
        // we're done with this quad
        indices[newindcount++] = breakIndex;
        continue;
      }

      // check if we have two opposite quads without extra points
      if ((edges[f+0].noMore() && edges[f+2].noMore()) ||
          (edges[f+1].noMore() && edges[f+3].noMore()) ||
          (edges[f+2].noMore() && edges[f+0].noMore()) ||
          (edges[f+3].noMore() && edges[f+1].noMore()))
      {
        // yes, we can use the algo for the strips here
        for (uint32_t eic = 0; eic < 4; ++eic) {
          if (edges[f+eic].noMore()) continue;
          const uint32_t oic = (eic+3)&3; // previous edge
          // sanity checks
          vassert(edges[f+oic].noMore());
          vassert(edges[f+oic].v1 == edges[f+eic].v0);
          // create triangle fan
          indices[newindcount++] = edges[f+oic].v0;
          indices[newindcount++] = edges[f+eic].v0;
          // append additional vertices (they are already properly sorted)
          int avidx = edges[f+eic].morefirst;
          while (avidx >= 0) {
            indices[newindcount++] = addedlist[avidx].vidx;
            avidx = addedlist[avidx].next;
          }
          /*
          for (uint32_t tmpf = 0; tmpf < (uint32_t)edges[f+eic].moreverts.length(); ++tmpf) {
            indices[newindcount++] = edges[f+eic].moreverts[tmpf];
          }
          */
          // and the last vertex
          indices[newindcount++] = edges[f+eic].v1;
          // if the opposite side is not modified, we can finish the fan right now
          const uint32_t loic = (eic+2)&3;
          if (edges[f+loic].noMore()) {
            const uint32_t noic = (eic+1)&3;
            // oic: prev
            // eic: current
            // noic: next
            // loic: last
            indices[newindcount++] = edges[f+noic].v1;
            indices[newindcount++] = breakIndex;
            // we're done here
            break;
          }
          indices[newindcount++] = breakIndex;
        }
        continue;
      }

      // alas, this quad should be converted to "centroid quad"
      // i.e. we will use quad center point to start a triangle fan

      // calculate quad center point
      float cx = 0.0f, cy = 0.0f, cz = 0.0f;
      float cs = 0.0f, ct = 0.0f;
      float prevx = 0.0f, prevy = 0.0f, prevz = 0.0f;
      bool xequal = true, yequal = true, zequal = true;
      for (uint32_t eic = 0; eic < 4; ++eic) {
        cs += (vertices[edges[f+eic].v0].s+vertices[edges[f+eic].v1].s)*0.5f;
        ct += (vertices[edges[f+eic].v0].t+vertices[edges[f+eic].v1].t)*0.5f;
        const float vx = vertices[edges[f+eic].v0].x;
        const float vy = vertices[edges[f+eic].v0].y;
        const float vz = vertices[edges[f+eic].v0].z;
        cx += vx;
        cy += vy;
        cz += vz;
        if (eic) {
          xequal = xequal && (AlmostEquals(prevx, vx));
          yequal = yequal && (AlmostEquals(prevy, vy));
          zequal = zequal && (AlmostEquals(prevz, vz));
        }
        prevx = vx;
        prevy = vy;
        prevz = vz;
      }
      cx /= 4.0f;
      cy /= 4.0f;
      cz /= 4.0f;
      cs /= 4.0f;
      ct /= 4.0f;

      // calculate s and t
      float s = cs;
      float t = ct;

      // append center vertex
      VVoxVertexEx nvx = vertices[edges[f+0].v0];
      // set coord
      nvx.x = cx;
      nvx.y = cy;
      nvx.z = cz;
      // calc new (s,t)
      nvx.s = s;
      nvx.t = t;
      // for statistics
      AddedVert &av = addedlist.alloc();
      av.next = -1;
      av.vidx = appendVertex(nvx);
      indices[newindcount++] = av.vidx;

      // append v0 of the first edge
      indices[newindcount++] = edges[f+0].v0;
      // append all vertices except v0 for all edges
      for (uint32_t eic = 0; eic < 4; ++eic) {
        int avidx = edges[f+eic].morefirst;
        while (avidx >= 0) {
          indices[newindcount++] = addedlist[avidx].vidx;
          avidx = addedlist[avidx].next;
        }
        /*
        for (uint32_t midx = 0; midx < (uint32_t)edges[f+eic].moreverts.length(); ++midx) {
          indices[newindcount++] = edges[f+eic].moreverts[midx];
        }
        */
        indices[newindcount++] = edges[f+eic].v1;
      }
      indices[newindcount++] = breakIndex;
    } else {
      // easy deal, just copy it
      indices[newindcount++] = edges[f+0].v0;
      indices[newindcount++] = edges[f+1].v0;
      indices[newindcount++] = edges[f+2].v0;
      indices[newindcount++] = edges[f+3].v0;
      indices[newindcount++] = breakIndex;
    }
  }

  indices.setLength(newindcount);
  indices.condense();
}


//==========================================================================
//
//  GLVoxelMesh::fixTJunctions
//
//  t-junction fixer entry point
//  this will also convert vertex data to triangle strips
//
//==========================================================================
void GLVoxelMesh::fixTJunctions () {
  const int oldvtotal = vertices.length();
  createEdges();
  sortEdges();
  vassert(addedlist.length() == 0);
  for (uint32_t f = 0; f < (uint32_t)edges.length(); ++f) fixEdgeNew(f);
  freeSortStructs();
  if (addedlist.length()) {
    rebuildEdges();
    if (voxlib_verbose) {
      vox_logf(VoxLibMsg_Normal, "rebuilt model: %s tris, %s vertices (%s added, %s unique)",
               vox_comatoze(countTris()), vox_comatoze(vertices.length()),
               vox_comatoze(addedlist.length()), vox_comatoze(vertices.length()-oldvtotal));
    }
  }
  edges.clear();
  addedlist.clear();
}


//==========================================================================
//
//  GLVoxelMesh::countTris
//
//  count the number of triangles in triangle fan data
//  used for informational messages
//
//==========================================================================
uint32_t GLVoxelMesh::countTris () {
  uint32_t res = 0;
  uint32_t ind = 0;
  while (ind < (uint32_t)indices.length()) {
    vassert(indices[ind] != breakIndex);
    uint32_t end = ind+1;
    while (end < (uint32_t)indices.length() && indices[end] != breakIndex) ++end;
    vassert(end < (uint32_t)indices.length());
    vassert(end-ind >= 3);
    if (end-ind == 3) {
      // simple triangle
      res += 1;
    } else if (end-ind == 4) {
      // quad
      res += 2;
    } else {
      // triangle fan
      res += end-ind-2;
    }
    ind = end+1;
  }
  return res;
}


//==========================================================================
//
//  GLVoxelMesh::recreateTriangles
//
//==========================================================================
void GLVoxelMesh::createTriangles (NewTriCB cb, void *udata) {
  if (!cb) return;
  int ind = 0;
  while (ind < indices.length()) {
    vassert(indices[ind] != breakIndex);
    int end = ind+1;
    while (end < indices.length() && indices[end] != breakIndex) ++end;
    vassert(end < indices.length());
    vassert(end-ind >= 3);
    if (end-ind == 3) {
      // simple triangle
      cb(indices[ind+0], indices[ind+1], indices[ind+2], udata);
    } else if (end-ind == 4) {
      // quad
      cb(indices[ind+0], indices[ind+1], indices[ind+2], udata);
      cb(indices[ind+2], indices[ind+3], indices[ind+0], udata);
    } else {
      // triangle fan
      for (int f = ind+1; f < end-1; ++f) {
        cb(indices[ind+0], indices[f+0], indices[f+1], udata);
      }
    }
    ind = end+1;
  }
}


//==========================================================================
//
//  GLVoxelMesh::create
//
//  main entry point
//
//==========================================================================
void GLVoxelMesh::create (VoxelMesh &vox, bool tjfix, uint32_t BreakIndex) {
  clear();
  breakIndex = BreakIndex;

  imgWidth = vox.catlas.getWidth();
  imgHeight = vox.catlas.getHeight();
  img.setLength(imgWidth*imgHeight);
  memcpy(img.ptr(), vox.catlas.colors.ptr(), imgWidth*imgHeight*4);

  // swap final colors in GL mesh?
  #ifndef VOXLIB_DONT_SWAP_COLORS
  uint8_t *ccs = (uint8_t *)img.ptr();
  for (int f = imgWidth*imgHeight; f--; ccs += 4) {
    const uint8_t ctmp = ccs[0];
    ccs[0] = ccs[2];
    ccs[2] = ctmp;
  }
  #endif

  if (voxlib_verbose) {
    vox_logf(VoxLibMsg_Normal, "color texture size: %dx%d", imgWidth, imgHeight);
  }

  // create arrays
  const int quadcount = vox.quads.length();
  for (int f = 0; f < quadcount; ++f) {
    VoxQuad &vq = vox.quads[f];
    uint32_t vxn[4];

    VVoxVertexEx gv;
    for (uint32_t nidx = 0; nidx < 4; ++nidx) {
      const VoxQuadVertex &vx = vq.vx[nidx];
      gv.x = vx.x;
      gv.y = vx.y;
      gv.z = vx.z;
      if (vq.type == VoxelMesh::ZLong) {
        gv.s = calcS(vox, vq, (vx.dz ? 1 : -1));
        gv.t = calcT(vox, vq, 0);
      } else if (vq.type == VoxelMesh::XLong) {
        gv.s = calcS(vox, vq, (vx.dx ? 1 : -1));
        gv.t = calcT(vox, vq, 0);
      } else if (vq.type == VoxelMesh::YLong) {
        gv.s = calcS(vox, vq, (vx.dy ? 1 : -1));
        gv.t = calcT(vox, vq, 0);
      } else if (vq.type == VoxelMesh::Point) {
        gv.s = calcS(vox, vq, 0);
        gv.t = calcT(vox, vq, 0);
      } else {
        int spos = -1, tpos = -1;
        vassert(vq.type == VoxelMesh::Quad);
        if (vq.cull&VoxelMesh::Cull_ZAxisMask) {
          if (vx.qtype&VoxelMesh::DMV_X) spos = 1;
          if (vx.qtype&VoxelMesh::DMV_Y) tpos = 1;
        } else if (vq.cull&VoxelMesh::Cull_XAxisMask) {
          if (vx.qtype&VoxelMesh::DMV_Y) spos = 1;
          if (vx.qtype&VoxelMesh::DMV_Z) tpos = 1;
        } else if (vq.cull&VoxelMesh::Cull_YAxisMask) {
          if (vx.qtype&VoxelMesh::DMV_X) spos = 1;
          if (vx.qtype&VoxelMesh::DMV_Z) tpos = 1;
        } else {
          vassert(0);
        }
        gv.s = calcS(vox, vq, spos);
        gv.t = calcT(vox, vq, tpos);
      }
      gv.nx = vq.normal.x;
      gv.ny = vq.normal.y;
      gv.nz = vq.normal.z;
      vxn[nidx] = appendVertex(gv);
    }

    indices.append(vxn[0]);
    indices.append(vxn[1]);
    indices.append(vxn[2]);
    indices.append(vxn[3]);
    indices.append(breakIndex);
  }

  if (voxlib_verbose) {
    vox_logf(VoxLibMsg_Normal, "OpenGL: %s quads, %s tris, %s unique vertices (of %s)",
              vox_comatoze(vox.quads.length()), vox_comatoze(countTris()),
              vox_comatoze(vertices.length()), vox_comatoze(totaladded));
  }

  if (tjfix) {
    if (vertices.length() > 4 &&
        (vmax[0]-vmin[0] > 1 || vmax[1]-vmin[1] > 1 || vmax[2]-vmin[2] > 1))
    {
      fixTJunctions();
      if (voxlib_verbose) {
        vox_logf(VoxLibMsg_Normal, "OpenGL: with fixed t-junctions: %s tris",
                 vox_comatoze(countTris()));
      }
    }
  }

  vertcache.clear();
}


// ////////////////////////////////////////////////////////////////////////// //
// memory stream
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  mst_readBuf
//
//==========================================================================
static bool mst_readBuf (void *buf, uint32_t len, VoxByteStream *strm) {
  VoxMemByteStream *mst = (VoxMemByteStream *)strm;
  if (mst->currOfs >= mst->dataSize) return false;
  if (len > mst->dataSize-mst->currOfs) return false;
  memcpy(buf, ((const uint8_t *)strm->udata)+mst->currOfs, len);
  mst->currOfs += len;
  return true;
}


//==========================================================================
//
//  mst_seek
//
//==========================================================================
static bool mst_seek (uint32_t ofs, VoxByteStream *strm) {
  VoxMemByteStream *mst = (VoxMemByteStream *)strm;
  if (ofs >= mst->dataSize) return false;
  mst->currOfs = ofs;
  return true;
}


//==========================================================================
//
//  mst_totalSize
//
//==========================================================================
static uint32_t mst_totalSize (VoxByteStream *strm) {
  VoxMemByteStream *mst = (VoxMemByteStream *)strm;
  return mst->dataSize;
}


//==========================================================================
//
//  vox_InitMemoryStream
//
//  returns `mst`, but properly casted
//  there is no need to deinit this stream
//
//==========================================================================
VoxByteStream *vox_InitMemoryStream (VoxMemByteStream *mst, const void *buf, uint32_t buflen) {
  if (!mst) return nullptr;
  if (!buf) buflen = 0;
  if (!buflen) buf = nullptr;
  mst->strm.readBuf = &mst_readBuf;
  mst->strm.seek = &mst_seek;
  mst->strm.totalSize = &mst_totalSize;
  mst->strm.udata = (void *)buf;
  mst->dataSize = buflen;
  mst->currOfs = 0;
  return (VoxByteStream *)mst;
}


// ////////////////////////////////////////////////////////////////////////// //
// various loaders
// ////////////////////////////////////////////////////////////////////////// //

#define CPOS_ERR  (0xffffffffU)
#define WASERR()  (cpos == CPOS_ERR)

#define CHECKERR()  do { \
  if (cpos == CPOS_ERR) { \
    vox_logf(VoxLibMsg_Error, "error reading voxel data"); \
    return false; \
  } \
} while (0)


#define XRD(tp_)  \
  if (*cpos != CPOS_ERR) { \
    tp_ res; \
    if (strm.readBuf(&res, (uint32_t)sizeof(tp_), &strm)) { \
      *cpos += (uint32_t)sizeof(tp_); \
      return res; \
    } \
  } \
  *cpos = CPOS_ERR; \
  return 0


// WARNING! no big endian support yet!
static inline uint8_t readUByte (VoxByteStream &strm, uint32_t *cpos) { XRD(uint8_t); }
static inline uint16_t readUShort (VoxByteStream &strm, uint32_t *cpos) { XRD(uint16_t); }
static inline uint32_t readULong (VoxByteStream &strm, uint32_t *cpos) { XRD(uint32_t); }
static inline int32_t readILong (VoxByteStream &strm, uint32_t *cpos) { XRD(int32_t); }
static inline float readFloat (VoxByteStream &strm, uint32_t *cpos) { XRD(float); }
static inline float readDouble (VoxByteStream &strm, uint32_t *cpos) { XRD(double); }

static inline bool readBuf (VoxByteStream &strm, void *buf, uint32_t len, uint32_t *cpos) {
  if (*cpos == CPOS_ERR) return false;
  if (!len) return true;
  if (!strm.readBuf(buf, len, &strm)) {
    *cpos = CPOS_ERR;
    return false;
  }
  cpos += len;
  return true;
}


#define CHECK_STRM()  \
  if (!strm.readBuf || !strm.seek || !strm.totalSize) return false; \
  uint32_t cpos = 0; \
  const uint32_t tsize = strm.totalSize(&strm)


//==========================================================================
//
//  vox_loadKVX
//
//==========================================================================
bool vox_loadKVX (VoxByteStream &strm, VoxelData &vox, const uint8_t defpal[768],
                  const uint8_t sign[4])
{
  CHECK_STRM();

  if (tsize < 28 || tsize > 0x00ffffffU) {
    vox_logf(VoxLibMsg_Error, "invalid voxel data (kvx) (tsize=%u)", tsize);
    return false;
  }

  uint32_t fsize;
  if (sign == NULL) {
    fsize = readULong(strm, &cpos);
  } else {
    memcpy(&fsize, (const void *)sign, 4);
  }
  if (WASERR() || fsize < 4*6 || fsize > 0x00ffffffU || fsize > tsize) {
    vox_logf(VoxLibMsg_Error, "invalid voxel data (kvx) (fsize=%u)", fsize);
    return false;
  }

  VoxLibArray<uint32_t> xofs;
  VoxLibArray<uint16_t> xyofs;
  VoxLibArray<uint8_t> data;

  int32_t xsiz = readILong(strm, &cpos); CHECKERR();
  int32_t ysiz = readILong(strm, &cpos); CHECKERR();
  int32_t zsiz = readILong(strm, &cpos); CHECKERR();
  if (voxlib_verbose) vox_logf(VoxLibMsg_Normal, "voxel size: %dx%dx%d", xsiz, ysiz, zsiz);
  if (xsiz < 1 || ysiz < 1 || zsiz < 1 ||
      xsiz > 1024 || ysiz > 1024 || zsiz > 1024)
  {
    vox_logf(VoxLibMsg_Error, "invalid voxel size (kvx)");
    return false;
  }

  int32_t xpivot = readILong(strm, &cpos); CHECKERR();
  int32_t ypivot = readILong(strm, &cpos); CHECKERR();
  int32_t zpivot = readILong(strm, &cpos); CHECKERR();

  const int ww = ysiz+1;

  uint32_t xstart = (xsiz+1)*4+xsiz*(ysiz+1)*2;

  xofs.setLength(xsiz+1);
  for (int f = 0; f <= xsiz; ++f) {
    xofs[f] = readULong(strm, &cpos); CHECKERR();
    xofs[f] -= xstart;
  }

  xyofs.setLength(xsiz*ww);
  for (int x = 0; x < xsiz; ++x) {
    for (int y = 0; y <= ysiz; ++y) {
      xyofs[x*ww+y] = readUShort(strm, &cpos); CHECKERR();
    }
  }
  //vassert(fx.size-fx.tell == fsize-24-(xsiz+1)*4-xsiz*(ysiz+1)*2);
  //data = new vuint8[](cast(vuint32)(fx.size-fx.tell));
  data.setLength(fsize-24-(xsiz+1)*4-xsiz*(ysiz+1)*2);
  if (!readBuf(strm, data.ptr(), (unsigned)data.length(), &cpos)) {
    vox_logf(VoxLibMsg_Error, "error reading voxel data (kvx)");
    return false;
  }

  // read palette
  uint8_t pal[768];
  if (cpos < tsize && tsize-cpos >= 768) {
    cpos = tsize-768;
    if (!strm.seek(cpos, &strm)) {
      vox_logf(VoxLibMsg_Error, "error reading voxel data (kvx)");
      return false;
    }
    if (!readBuf(strm, pal, 768, &cpos)) {
      vox_logf(VoxLibMsg_Error, "error reading voxel data (kvx)");
      return false;
    }
    for (unsigned cidx = 0; cidx < 768; ++cidx) {
      int v = 255*((int)pal[cidx])/64;
      if (v < 0) v = 0; else if (v > 255) v = 255;
      pal[cidx] = (uint8_t)v;
    }
  } else if (!defpal) {
    for (unsigned cidx = 0; cidx < 256; ++cidx) {
      pal[cidx*3+0] = (uint8_t)cidx;
    }
  } else {
    memcpy(pal, defpal, 768);
  }

  const float px = (float)xpivot/256.0f;
  const float py = (float)ypivot/256.0f;
  const float pz = (float)zpivot/256.0f;

  // now build cubes
  vox.setSize(xsiz, ysiz, zsiz);
  for (int y = 0; y < ysiz; ++y) {
    for (int x = 0; x < xsiz; ++x) {
      uint32_t sofs = xofs[x]+xyofs[x*ww+y];
      uint32_t eofs = xofs[x]+xyofs[x*ww+y+1];
      //if (sofs == eofs) continue;
      //vassert(sofs < data.length && eofs <= data.length);
      while (sofs < eofs) {
        int ztop = data[sofs++];
        uint32_t zlen = data[sofs++];
        uint8_t cull = data[sofs++];
        // colors
        for (uint32_t cidx = 0; cidx < zlen; ++cidx) {
          uint8_t palcol = data[sofs++];
          /*
          vuint8 cl = cull;
          if (cidx != 0) cl &= ~0x10;
          if (cidx != zlen-1) cl &= ~0x20;
          */
          const uint32_t rgb =
            pal[palcol*3+2]|
            ((uint32_t)pal[palcol*3+1]<<8)|
            ((uint32_t)pal[palcol*3+0]<<16);
          ++ztop;
          vox.addVoxel(xsiz-x-1, y, zsiz-ztop, rgb, cull);
        }
      }
      //vassert(sofs == eofs);
    }
  }

  vox.cx = px;
  vox.cy = py;
  vox.cz = pz;

  return true;
}


//==========================================================================
//
//  vox_loadKV6
//
//==========================================================================
bool vox_loadKV6 (VoxByteStream &strm, VoxelData &vox, const uint8_t bsign[4]) {
  struct KVox {
    uint32_t rgb;
    uint16_t z;
    uint8_t cull;
    uint8_t normidx;
  };

  CHECK_STRM();

  if (tsize < 32 || tsize > 0x00ffffffU) {
    vox_logf(VoxLibMsg_Error, "invalid voxel data (kv6)");
    return false;
  }

  uint32_t sign;
  if (bsign == NULL) {
    sign = readULong(strm, &cpos); CHECKERR();
  } else {
    memcpy(&sign, (const void *)bsign, 4);
  }
  if (sign != 0x6c78764bU) {
    vox_logf(VoxLibMsg_Error, "invalid voxel data signature (kv6)");
    return false;
  }

  int32_t xsiz = readILong(strm, &cpos); CHECKERR();
  int32_t ysiz = readILong(strm, &cpos); CHECKERR();
  int32_t zsiz = readILong(strm, &cpos); CHECKERR();
  if (voxlib_verbose) vox_logf(VoxLibMsg_Normal, "voxel size: %dx%dx%d", xsiz, ysiz, zsiz);
  if (xsiz < 1 || ysiz < 1 || zsiz < 1 ||
      xsiz > 1024 || ysiz > 1024 || zsiz > 1024)
  {
    vox_logf(VoxLibMsg_Error, "invalid voxel size");
    return false;
  }

  float xpivot = readFloat(strm, &cpos); CHECKERR();
  float ypivot = readFloat(strm, &cpos); CHECKERR();
  float zpivot = readFloat(strm, &cpos); CHECKERR();

  int32_t voxcount = readILong(strm, &cpos); CHECKERR();
  if (voxcount <= 0 || voxcount > 0x00ffffff) {
    vox_logf(VoxLibMsg_Error, "invalid number of voxels");
    return false;
  }

  VoxLibArray<KVox> kvox;
  kvox.setLength(voxcount);
  for (int32_t vidx = 0; vidx < voxcount; ++vidx) {
    KVox &kv = kvox[vidx];
    uint8_t r8 = readUByte(strm, &cpos); CHECKERR();
    uint8_t g8 = readUByte(strm, &cpos); CHECKERR();
    uint8_t b8 = readUByte(strm, &cpos); CHECKERR();
    kv.rgb = r8|(g8<<8)|(b8<<16);
    uint8_t dummy = readUByte(strm, &cpos); CHECKERR(); // always 128; ignore
    (void)dummy;
    uint8_t zlo = readUByte(strm, &cpos); CHECKERR();
    uint8_t zhi = readUByte(strm, &cpos); CHECKERR();
    uint8_t cull = readUByte(strm, &cpos); CHECKERR();
    uint8_t normidx = readUByte(strm, &cpos); CHECKERR();
    kv.z = zlo+(zhi<<8);
    kv.cull = cull;
    kv.normidx = normidx;
  }

  VoxLibArray<uint32_t> xofs;
  xofs.setLength(xsiz+1);
  uint32_t curvidx = 0;
  for (int vidx = 0; vidx < xsiz; ++vidx) {
    xofs[vidx] = curvidx;
    uint32_t count = readULong(strm, &cpos); CHECKERR();
    curvidx += count;
  }
  xofs[xofs.length()-1] = curvidx;

  VoxLibArray<uint32_t> xyofs;
  const int ww = ysiz+1;
  xyofs.setLength(xsiz*ww);
  for (int xxidx = 0; xxidx < xsiz; ++xxidx) {
    curvidx = 0;
    for (int yyidx = 0; yyidx < ysiz; ++yyidx) {
      xyofs[xxidx*ww+yyidx] = curvidx;
      uint32_t count = readUShort(strm, &cpos); CHECKERR();
      curvidx += count;
    }
    xyofs[xxidx*ww+ysiz] = curvidx;
  }

  // now build cubes
  vox.setSize(xsiz, ysiz, zsiz);
  for (int y = 0; y < ysiz; ++y) {
    for (int x = 0; x < xsiz; ++x) {
      uint32_t sofs = xofs[x]+xyofs[x*ww+y];
      uint32_t eofs = xofs[x]+xyofs[x*ww+y+1];
      if (eofs > (uint32_t)kvox.length()) eofs = (uint32_t)kvox.length();
      //if (sofs == eofs) continue;
      //vassert(sofs < data.length && eofs <= data.length);
      while (sofs < eofs) {
        const KVox &kv = kvox[sofs++];
        const int z = kv.z+1;
        vox.addVoxel(xsiz-x-1, y, zsiz-z, kv.rgb, kv.cull);
      }
    }
  }

  vox.cx = xpivot;
  vox.cy = ypivot;
  vox.cz = zpivot;

  return true;
}


//==========================================================================
//
//  vox_loadVox
//
//==========================================================================
bool vox_loadVox (VoxByteStream &strm, VoxelData &vox, const uint8_t defpal[768],
                  const uint8_t sign[4])
{
  CHECK_STRM();

  if (tsize < 16 || tsize > 0x03ffffffU) {
    vox_logf(VoxLibMsg_Error, "invalid voxel data (kv6)");
    return false;
  }

  int32_t xsiz;
  if (sign == NULL) {
    xsiz = readILong(strm, &cpos); CHECKERR();
  } else {
    memcpy(&xsiz, (const void *)sign, 4);
  }
  int32_t ysiz = readILong(strm, &cpos); CHECKERR();
  int32_t zsiz = readILong(strm, &cpos); CHECKERR();
  if (voxlib_verbose) vox_logf(VoxLibMsg_Normal, "voxel size: %dx%dx%d", xsiz, ysiz, zsiz);
  if (xsiz < 1 || ysiz < 1 || zsiz < 1 ||
      xsiz > 1024 || ysiz > 1024 || zsiz > 1024)
  {
    vox_logf(VoxLibMsg_Error, "invalid voxel size (vox)");
    return false;
  }

  VoxLibArray<uint8_t> data;
  data.setLength(xsiz*ysiz*zsiz);
  if (!readBuf(strm, data.ptr(), (uint32_t)data.length(), &cpos)) {
    vox_logf(VoxLibMsg_Error, "error reading voxel data");
    return false;
  }

  uint8_t pal[768];
  if (cpos < tsize && tsize-cpos >= 768) {
    if (!readBuf(strm, pal, 768, &cpos)) {
      vox_logf(VoxLibMsg_Error, "error reading voxel palette");
      return false;
    }
    for (unsigned cidx = 0; cidx < 768; ++cidx) {
      int v = 255*((int)pal[cidx])/64;
      if (v < 0) v = 0; else if (v > 255) v = 255;
      pal[cidx] = (uint8_t)v;
    }
  } else if (!defpal) {
    for (unsigned cidx = 0; cidx < 256; ++cidx) {
      pal[cidx*3+0] = (uint8_t)cidx;
    }
  } else {
    memcpy(pal, defpal, 768);
  }

  const float px = 1.0f*xsiz/2.0f;
  const float py = 1.0f*ysiz/2.0f;
  const float pz = 1.0f*zsiz/2.0f;

  // now build cubes
  uint32_t dpos = 0;
  vox.setSize(xsiz, ysiz, zsiz);
  for (int x = 0; x < xsiz; ++x) {
    for (int y = 0; y < ysiz; ++y) {
      for (int z = 0; z < zsiz; ++z) {
        uint8_t palcol = data[dpos++];
        if (palcol != 255) {
          uint32_t rgb = pal[palcol*3+2]|((uint32_t)pal[palcol*3+1]<<8)|((uint32_t)pal[palcol*3+0]<<16);
          vox.addVoxel(xsiz-x-1, y, zsiz-z-1, rgb, 0x3f);
        }
      }
    }
  }

  vox.cx = px;
  vox.cy = py;
  vox.cz = pz;

  return true;
}


//==========================================================================
//
//  vox_loadVxl
//
//==========================================================================
bool vox_loadVxl (VoxByteStream &strm, VoxelData &vox, const uint8_t bsign[4]) {
  CHECK_STRM();

  if (tsize < 32 || tsize > 0x00ffffffU) {
    vox_logf(VoxLibMsg_Error, "invalid voxel data (vxl)");
    return false;
  }

  uint32_t sign;
  if (bsign == NULL) {
    sign = readULong(strm, &cpos); CHECKERR();
  } else {
    memcpy(&sign, (const void *)bsign, 4);
  }
  if (sign != 0x09072000U) {
    vox_logf(VoxLibMsg_Error, "invalid voxel data signature (vxl)");
    return false;
  }

  int32_t xsiz = readILong(strm, &cpos); CHECKERR();
  int32_t ysiz = readILong(strm, &cpos); CHECKERR();
  int32_t zsiz = 256;
  if (voxlib_verbose) vox_logf(VoxLibMsg_Normal, "voxel size: %dx%dx%d", xsiz, ysiz, zsiz);
  if (xsiz < 1 || ysiz < 1 || zsiz < 1 ||
      xsiz > 1024 || ysiz > 1024 || zsiz > 1024)
  {
    vox_logf(VoxLibMsg_Error, "invalid voxel size (vox)");
    return false;
  }

  float px, py, pz;
  // camera
  px = readDouble(strm, &cpos); CHECKERR();
  py = readDouble(strm, &cpos); CHECKERR();
  pz = readDouble(strm, &cpos); CHECKERR();
  pz = zsiz-1-pz;
  // unit right
  (void)readDouble(strm, &cpos); CHECKERR();
  (void)readDouble(strm, &cpos); CHECKERR();
  (void)readDouble(strm, &cpos); CHECKERR();
  // unit down
  (void)readDouble(strm, &cpos); CHECKERR();
  (void)readDouble(strm, &cpos); CHECKERR();
  (void)readDouble(strm, &cpos); CHECKERR();
  // unit forward
  (void)readDouble(strm, &cpos); CHECKERR();
  (void)readDouble(strm, &cpos); CHECKERR();
  (void)readDouble(strm, &cpos); CHECKERR();

  vox.setSize(xsiz, ysiz, zsiz);

  /*
  void vxlReset (int x, int y, int z) {
    vox.removeVoxel(xsiz-x-1, y, zsiz-z-1);
  }

  void vxlPaint (int x, int y, int z, uint32_t clr) {
    vox.addVoxel(xsiz-x-1, y, zsiz-z-1, clr, 0x3f);
  }
  */

  // now carve crap out of it
  if (cpos >= tsize && tsize-cpos > 0x03fffffff) {
    vox_logf(VoxLibMsg_Error, "invalid voxel data (vxl)");
    return false;
  }

  VoxLibArray<uint8_t> data;
  data.setLength((int)(tsize-cpos));
  if (!readBuf(strm, data.ptr(), (uint32_t)data.length(), &cpos)) {
    vox_logf(VoxLibMsg_Error, "error reading voxel data");
    return false;
  }

  const uint8_t *v = data.ptr();
  for (int x = 0; x < xsiz; ++x) {
    for (int y = 0; y < ysiz; ++y) {
      int z = 0;
      for (;;) {
        for (int i = z; i < v[1]; ++i) vox.removeVoxel(xsiz-x-1, y, zsiz-i-1);
        for (z = v[1]; z <= v[2]; ++z) {
          const uint32_t *cp = (const uint32_t *)(v+(z-v[1]+1)*4);
          //vxlPaint(x, y, z, *cp);
          vox.addVoxel(xsiz-x-1, y, zsiz-z-1, *cp, 0x3f);
        }
        if (!v[0]) break;
        z = v[2]-v[1]-v[0]+2;
        v += v[0]*4;
        for (z += v[3]; z < v[3]; ++z) {
          const uint32_t *cp = (const uint32_t *)(v+(z-v[3])*4);
          //vxlPaint(x, y, z, *cp);
          vox.addVoxel(xsiz-x-1, y, zsiz-z-1, *cp, 0x3f);
        }
      }
      v += (v[2]-v[1]+2)*4;
    }
  }

  vox.cx = px;
  vox.cy = py;
  vox.cz = pz;

  return true;
}


//==========================================================================
//
//  vox_loadMagica
//
//  Magica Voxel (only first model)
//
//==========================================================================
bool vox_loadMagica (VoxByteStream &strm, VoxelData &vox, const uint8_t bsign[4]) {
  struct XYZI {
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t clr;
  };


  CHECK_STRM();

  if (tsize < 16 || tsize > 0x03ffffffU) {
    vox_logf(VoxLibMsg_Error, "invalid voxel data (magica)");
    return false;
  }

  // check signature
  uint32_t sign;
  if (bsign == NULL) {
    sign = readULong(strm, &cpos); CHECKERR();
  } else {
    memcpy(&sign, (const void *)bsign, 4);
  }
  if (sign != 0x20584f56U) {
    vox_logf(VoxLibMsg_Error, "invalid magica signature (0x%08x)", sign);
    return false;
  }

  // check version
  uint32_t ver = readULong(strm, &cpos); CHECKERR();
  if (ver != 150) {
    vox_logf(VoxLibMsg_Error, "invalid magica version (0x%08x)", ver);
    return false;
  }

  // set default palette
  uint32_t pal[256];
  memcpy(pal, magicaPal, sizeof(pal));

  uint32_t csig, dsize, csize;

  // look for "MAIN" chunk
  bool found = false;
  for (;;) {
    csig = readULong(strm, &cpos); CHECKERR();
    dsize = readULong(strm, &cpos); CHECKERR();
    csize = readULong(strm, &cpos); CHECKERR();
    if (csig == 0x4e49414d) { found = true; break; }
  }

  if (!found) {
    vox_logf(VoxLibMsg_Error, "magica \"MAIN\" chunk not found");
    return false;
  }

  if (csize < 32) {
    vox_logf(VoxLibMsg_Error, "magica \"MAIN\" chunk has no children");
    return false;
  }

  if (cpos >= tsize || tsize-cpos < 32) {
    vox_logf(VoxLibMsg_Error, "magica \"MAIN\" chunk too small");
    return false;
  }

  // skip content
  if (dsize) {
    if (dsize >= tsize || tsize-cpos < dsize) {
      vox_logf(VoxLibMsg_Error, "magica \"MAIN\" chunk content too big");
      return false;
    }
    if (!strm.seek(cpos+dsize, &strm)) {
      vox_logf(VoxLibMsg_Error, "error skipping magica \"MAIN\" content");
      return false;
    }
    cpos += dsize;
  }

  const uint32_t endpos = cpos+csize;
  if (endpos > tsize || endpos < cpos) {
    vox_logf(VoxLibMsg_Error, "error in \"MAIN\" children size");
    return false;
  }

  // scan and read subchunks
  // we are interested only in the first "SIZE" and "XYZI"
  // also, we are interested in "RGBA"

  VoxLibArray<XYZI> vxdata;
  int32_t xsiz = 0, ysiz = 0, zsiz = 0;
  bool seenRGBA = false;

  while (cpos < endpos) {
    if (endpos-cpos < 4*3) break;
    csig = readULong(strm, &cpos); CHECKERR();
    dsize = readULong(strm, &cpos); CHECKERR();
    csize = readULong(strm, &cpos); CHECKERR();

    #if 0
    vox_logf(VoxLibMsg_Error, "CHUNK: 0x%08x (dsize=%u; csize=%u)", csig, dsize, csize);
    #endif

    if (csig == 0x455a4953U && xsiz == 0 && dsize >= 4*3) {
      // "SIZE"
      xsiz = readILong(strm, &cpos); CHECKERR();
      ysiz = readILong(strm, &cpos); CHECKERR();
      zsiz = readILong(strm, &cpos); CHECKERR();
      if (zsiz < 0) zsiz = -zsiz;
      if (xsiz < 1 || ysiz < 1 || zsiz < 1) {
        vox_logf(VoxLibMsg_Error, "magica voxel too small (%d,%d,%d)", xsiz, ysiz, zsiz);
        return false;
      }
      if (xsiz > 1024 || ysiz > 1024 || zsiz > 1024) {
        vox_logf(VoxLibMsg_Error, "magica voxel too big (%d,%d,%d)", xsiz, ysiz, zsiz);
        return false;
      }
      dsize -= 4*3;
      if (voxlib_verbose) vox_logf(VoxLibMsg_Normal, "voxel size: %dx%dx%d", xsiz, ysiz, zsiz);
    } else if (csig == 0x495a5958U && dsize >= 4 && vxdata.length() == 0) {
      // "XYZI"
      uint32_t count = readULong(strm, &cpos); CHECKERR(); dsize -= 4;
      if (voxlib_verbose) vox_logf(VoxLibMsg_Normal, "voxel cubes: %u", count);
      if (count > 0) {
        vxdata.setLength(count);
        for (uint32_t f = 0; f < count; f += 1) {
          if (dsize < 4) {
            vox_logf(VoxLibMsg_Error, "out of magica voxel xyzi data");
            return false;
          }
          vxdata[f].x = readUByte(strm, &cpos); CHECKERR(); dsize -= 1;
          vxdata[f].y = readUByte(strm, &cpos); CHECKERR(); dsize -= 1;
          vxdata[f].z = readUByte(strm, &cpos); CHECKERR(); dsize -= 1;
          vxdata[f].clr = readUByte(strm, &cpos); CHECKERR(); dsize -= 1;
        }
      } else {
        // one transparent voxel
        vxdata.setLength(1);
        vxdata[0].x = 0;
        vxdata[0].y = 0;
        vxdata[0].z = 0;
        vxdata[0].clr = 0;
      }
    } else if (csig == 0x41424752U && dsize >= 4 && !seenRGBA) {
      // "RGBA"
      if (voxlib_verbose) vox_logf(VoxLibMsg_Normal, "found voxel palette");
      for (uint32_t f = 1; f <= 255; f += 1) {
        if (dsize < 4) break;
        pal[f] = readULong(strm, &cpos); CHECKERR(); dsize -= 4;
      }
    }

    #if 0
    vox_logf(VoxLibMsg_Error, "CHUNK: left: dsize=%u; csize=%u; cpos=%u; end=%u",
             dsize, csize, cpos, endpos);
    #endif

    // skip content (if there is any)
    if (dsize) {
      if (dsize > endpos-cpos) {
        vox_logf(VoxLibMsg_Error, "error skipping magica subchunk content size");
        return false;
      }
      if (dsize == endpos-cpos) break;
      if (!strm.seek(cpos+dsize, &strm)) {
        vox_logf(VoxLibMsg_Error, "error skipping magica subchunk content size");
        return false;
      }
      cpos += dsize;
    }

    // skip subchunks (if there is any)
    if (csize) {
      if (csize > endpos-cpos) {
        vox_logf(VoxLibMsg_Error, "error skipping magica subchunk children size");
        return false;
      }
      if (csize == endpos-cpos) break;
      if (!strm.seek(cpos+csize, &strm)) {
        vox_logf(VoxLibMsg_Error, "error skipping magica subchunk children size");
        return false;
      }
      cpos += csize;
    }

    #if 0
    vox_logf(VoxLibMsg_Error, "CHUNK: left: %u (cpos=%u)", endpos-cpos, cpos);
    #endif
  }

  if (xsiz == 0) {
    vox_logf(VoxLibMsg_Error, "no \"SIZE\" subchunk in magica");
    return false;
  }
  if (vxdata.length() == 0) {
    vox_logf(VoxLibMsg_Error, "no \"XYZI\" subchunk in magica");
    return false;
  }

  // now build cubes
  const float px = 1.0f*xsiz/2.0f;
  const float py = 1.0f*ysiz/2.0f;
  const float pz = 1.0f*zsiz/2.0f;
  const int xright = xsiz-1;
  const int yright = ysiz-1;
  vox.setSize(xsiz, ysiz, zsiz);
  for (int f = 0; f < vxdata.length(); f += 1) {
    XYZI vx = vxdata[f];
    if (vx.clr == 0) continue; // transparent
    const uint32_t rgb = pal[vx.clr];
    const uint8_t a = (uint8_t)(rgb>>24);
    if (a == 0) continue; // still transparent
    if (a != 0xffU) {
      //conwritefln!"%u: 0x%08x"(vx.clr, rgb);
      vox_logf(VoxLibMsg_Error, "magica translucent voxels are not supported");
      return false;
    }
    const uint32_t b = (uint8_t)(rgb>>16);
    const uint32_t g = (uint8_t)(rgb>>8);
    const uint32_t r = (uint8_t)rgb;
    //vox.addVoxel(xright-(vx.x+xofs), yright-(vx.y+yofs), vx.z, b|(g<<8)|(r<<16), 0xff);
    vox.addVoxel(xright-vx.x, yright-vx.y, vx.z, b|(g<<8)|(r<<16), 0x3f);
  }

  vox.cx = px;
  vox.cy = py;
  vox.cz = pz;

  return true;
}


//==========================================================================
//
//  vox_detectFormat
//
//  detect voxel file format by the first 4 file bytes
//  KVX format has no signature, so it cannot be reliably detected
//
//==========================================================================
VoxFmt vox_detectFormat (const uint8_t bytes[4]) {
  if (!bytes) return VoxFmt_Unknown;
  if (memcmp(bytes, "Kvxl", 4) == 0) return VoxFmt_KV6;
  if (memcmp(bytes, "VOX ", 4) == 0) return VoxFmt_Magica;
  if (memcmp(bytes, "\x00\x20\x07\x09", 4) == 0) return VoxFmt_Vxl;
  return VoxFmt_Unknown;
}


//==========================================================================
//
//  vox_loadModel
//
//  this tries to detect model format
//
//==========================================================================
bool vox_loadModel (VoxByteStream &strm, VoxelData &vox, const uint8_t defpal[768]) {
  uint8_t sign[4];
  if (!strm.readBuf || !strm.seek || !strm.totalSize) return false;
  const uint32_t tsize = strm.totalSize(&strm);
  if (tsize < 8) return false;
  if (!strm.readBuf(sign, 4, &strm)) return false;
  const VoxFmt fmt = vox_detectFormat(sign);
  bool ok = false;
  switch (fmt) {
    case VoxFmt_Unknown: // assume KVX
      vox_logf(VoxLibMsg_Debug, "loading KVX...");
      ok = vox_loadKVX(strm, vox, defpal, sign);
      break;
    case VoxFmt_KV6:
      vox_logf(VoxLibMsg_Debug, "loading KV6...");
      ok = vox_loadKV6(strm, vox, sign);
      break;
    case VoxFmt_Magica:
      vox_logf(VoxLibMsg_Debug, "loading Magica...");
      ok = vox_loadMagica(strm, vox, sign);
      break;
    case VoxFmt_Vxl:
      vox_logf(VoxLibMsg_Error, "cannot load voxel model in VXL format");
      break;
    default:
      break;
  }
  return ok;
}
