#pragma once

#include "r_gldefs.h"
#include "r_image.h"
#include "r_units.h"

constexpr uint8_t kMaximumMirrors = 3;

void MirrorPush(DrawMirror *mir);
void MirrorPop();
void RenderMirror(DrawMirror *mir);
bool MirrorSegOnPortal(Seg *seg);
void MirrorPushSubsector(int32_t index, DrawSubsector *subsector);

void MirrorTransform(int32_t index, float &x, float &y);
bool MirrorIsPortal(int32_t index);
Seg *MirrorSeg(int32_t index);

int32_t MirrorTotalActive();

void RenderSubList(std::list<DrawSubsector *> &dsubs, bool for_mirror = false);

void BspWalkNode(unsigned int);

void QueueSkyWall(Seg *seg, float h1, float h2);
void QueueSkyPlane(Subsector *sub, float h);

inline BlendingMode GetBlending(float alpha, ImageOpacity opacity)
{
    int blending;

    if (alpha >= 0.99f && opacity == kOpacitySolid)
        blending = kBlendingNone;
    else if (alpha >= 0.99f && opacity == kOpacityMasked)
        blending = kBlendingMasked;
    else
        blending = kBlendingLess;

    if (alpha < 0.99f || opacity == kOpacityComplex)
        blending |= kBlendingAlpha;

    return (BlendingMode)blending;
}
