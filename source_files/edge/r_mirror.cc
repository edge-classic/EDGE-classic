
#include "epi_doomdefs.h"
#include "p_local.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_render.h"
#include "r_units.h"

extern bool solid_mode;

static int32_t total_active_mirrors = 0;

static inline void ClipPlaneHorizontalLine(GLdouble *p, const HMM_Vec2 &s, const HMM_Vec2 &e)
{
    p[0] = e.Y - s.Y;
    p[1] = s.X - e.X;
    p[2] = 0.0f;
    p[3] = e.X * s.Y - s.X * e.Y;
}

static inline void ClipPlaneEyeAngle(GLdouble *p, BAMAngle ang)
{
    HMM_Vec2 s, e;

    s = {{view_x, view_y}};

    e = {{view_x + epi::BAMCos(ang), view_y + epi::BAMSin(ang)}};

    ClipPlaneHorizontalLine(p, s, e);
}

class MirrorInfo
{
  public:
    DrawMirror *draw_mirror_;

    float xc_, xx_, xy_; // x' = xc + x*xx + y*xy
    float yc_, yx_, yy_; // y' = yc + x*yx + y*yy
    float zc_, z_scale_; // z' = zc + z*z_scale

    float xy_scale_;

    BAMAngle tc_;

  public:
    void ComputeMirror()
    {
        Seg *seg = draw_mirror_->seg;

        float sdx = seg->vertex_2->X - seg->vertex_1->X;
        float sdy = seg->vertex_2->Y - seg->vertex_1->Y;

        float len_p2 = seg->length * seg->length;

        float A = (sdx * sdx - sdy * sdy) / len_p2;
        float B = (sdx * sdy * 2.0) / len_p2;

        xx_ = A;
        xy_ = B;
        yx_ = B;
        yy_ = -A;

        xc_ = seg->vertex_1->X * (1.0 - A) - seg->vertex_1->Y * B;
        yc_ = seg->vertex_1->Y * (1.0 + A) - seg->vertex_1->X * B;

        tc_ = seg->angle << 1;

        zc_       = 0;
        z_scale_  = 1.0f;
        xy_scale_ = 1.0f;
    }

    float GetAlong(const Line *ld, float x, float y)
    {
        if (fabs(ld->delta_x) >= fabs(ld->delta_y))
            return (x - ld->vertex_1->X) / ld->delta_x;
        else
            return (y - ld->vertex_1->Y) / ld->delta_y;
    }

    void ComputePortal()
    {
        Seg  *seg   = draw_mirror_->seg;
        Line *other = seg->linedef->portal_pair;

        EPI_ASSERT(other);

        float ax1 = seg->vertex_1->X;
        float ay1 = seg->vertex_1->Y;

        float ax2 = seg->vertex_2->X;
        float ay2 = seg->vertex_2->Y;

        // find corresponding coords on partner line
        float along1 = GetAlong(seg->linedef, ax1, ay1);
        float along2 = GetAlong(seg->linedef, ax2, ay2);

        float bx1 = other->vertex_2->X - other->delta_x * along1;
        float by1 = other->vertex_2->Y - other->delta_y * along1;

        float bx2 = other->vertex_2->X - other->delta_x * along2;
        float by2 = other->vertex_2->Y - other->delta_y * along2;

        // compute rotation angle
        tc_ = kBAMAngle180 + PointToAngle(0, 0, other->delta_x, other->delta_y) - seg->angle;

        xx_ = epi::BAMCos(tc_);
        xy_ = epi::BAMSin(tc_);
        yx_ = -epi::BAMSin(tc_);
        yy_ = epi::BAMCos(tc_);

        // scaling
        float a_len = seg->length;
        float b_len = PointToDistance(bx1, by1, bx2, by2);

        xy_scale_ = a_len / HMM_MAX(1, b_len);

        xx_ *= xy_scale_;
        xy_ *= xy_scale_;
        yx_ *= xy_scale_;
        yy_ *= xy_scale_;

        // translation
        xc_ = ax1 - bx1 * xx_ - by1 * xy_;
        yc_ = ay1 - bx1 * yx_ - by1 * yy_;

        // heights
        float a_h = (seg->front_sector->interpolated_ceiling_height - seg->front_sector->interpolated_floor_height);
        float b_h = (other->front_sector->interpolated_ceiling_height - other->front_sector->interpolated_floor_height);

        z_scale_ = a_h / HMM_MAX(1, b_h);
        zc_ = seg->front_sector->interpolated_floor_height - other->front_sector->interpolated_floor_height * z_scale_;
    }

    void Compute()
    {
        if (draw_mirror_->is_portal)
            ComputePortal();
        else
            ComputeMirror();
    }

    void Transform(float &x, float &y)
    {
        float tx = x, ty = y;

        x = xc_ + tx * xx_ + ty * xy_;
        y = yc_ + tx * yx_ + ty * yy_;
    }

    void Z_Adjust(float &z)
    {
        z = zc_ + z * z_scale_;
    }

    void Turn(BAMAngle &ang)
    {
        ang = (draw_mirror_->is_portal) ? (ang - tc_) : (tc_ - ang);
    }
};

static MirrorInfo active_mirrors[kMaximumMirrors];

void MirrorTransform(int32_t index, float &x, float &y)
{
    active_mirrors[index].Transform(x, y);
}

bool MirrorIsPortal(int32_t index)
{
    return active_mirrors[index].draw_mirror_->is_portal;
}

Seg *MirrorSeg(int32_t index)
{
    return active_mirrors[index].draw_mirror_->seg;
}

int32_t MirrorTotalActive()
{
    return total_active_mirrors;
}

void MirrorCoordinate(float &x, float &y)
{
    for (int i = total_active_mirrors - 1; i >= 0; i--)
        active_mirrors[i].Transform(x, y);
}

void MirrorHeight(float &z)
{
    for (int i = total_active_mirrors - 1; i >= 0; i--)
        active_mirrors[i].Z_Adjust(z);
}

void MirrorAngle(BAMAngle &ang)
{
    for (int i = total_active_mirrors - 1; i >= 0; i--)
        active_mirrors[i].Turn(ang);
}

float MirrorXYScale(void)
{
    float result = 1.0f;

    for (int i = total_active_mirrors - 1; i >= 0; i--)
        result *= active_mirrors[i].xy_scale_;

    return result;
}

float MirrorZScale(void)
{
    float result = 1.0f;

    for (int i = total_active_mirrors - 1; i >= 0; i--)
        result *= active_mirrors[i].z_scale_;

    return result;
}

bool MirrorReflective(void)
{
    if (total_active_mirrors == 0)
        return false;

    bool result = false;

    for (int i = total_active_mirrors - 1; i >= 0; i--)
        if (!active_mirrors[i].draw_mirror_->is_portal)
            result = !result;

    return result;
}
bool MirrorSegOnPortal(Seg *seg)
{
    if (total_active_mirrors == 0)
        return false;

    if (seg->miniseg)
        return false;

    DrawMirror *def = active_mirrors[total_active_mirrors - 1].draw_mirror_;

    if (def->is_portal)
    {
        if (seg->linedef == def->seg->linedef->portal_pair)
            return true;
    }
    else // mirror
    {
        if (seg->linedef == def->seg->linedef)
            return true;
    }

    return false;
}

void MirrorPushSubsector(int32_t index, DrawSubsector *subsector)
{
    active_mirrors[index].draw_mirror_->draw_subsectors.push_back(subsector);
}

static void MirrorSetClippers()
{
    render_state->Disable(GL_CLIP_PLANE0);
    render_state->Disable(GL_CLIP_PLANE1);
    render_state->Disable(GL_CLIP_PLANE2);
    render_state->Disable(GL_CLIP_PLANE3);
    render_state->Disable(GL_CLIP_PLANE4);
    render_state->Disable(GL_CLIP_PLANE5);

    if (total_active_mirrors == 0)
        return;

    // setup planes for left and right sides of innermost mirror.
    // Angle clipping has ensured that for multiple mirrors all
    // later mirrors are limited to the earlier mirrors.

    MirrorInfo &inner = active_mirrors[total_active_mirrors - 1];

    GLdouble left_p[4];
    GLdouble right_p[4];

    ClipPlaneEyeAngle(left_p, inner.draw_mirror_->left);
    ClipPlaneEyeAngle(right_p, inner.draw_mirror_->right + kBAMAngle180);

    render_state->Enable(GL_CLIP_PLANE0);
    render_state->Enable(GL_CLIP_PLANE1);

    render_state->ClipPlane(GL_CLIP_PLANE0, left_p);
    render_state->ClipPlane(GL_CLIP_PLANE1, right_p);

    // now for each mirror, setup a clip plane that removes
    // everything that gets projected in front of that mirror.

    for (int i = 0; i < total_active_mirrors; i++)
    {
        MirrorInfo &mir = active_mirrors[i];

        HMM_Vec2 v1, v2;

        v1 = {{mir.draw_mirror_->seg->vertex_1->X, mir.draw_mirror_->seg->vertex_1->Y}};
        v2 = {{mir.draw_mirror_->seg->vertex_2->X, mir.draw_mirror_->seg->vertex_2->Y}};

        for (int k = i - 1; k >= 0; k--)
        {
            if (!active_mirrors[k].draw_mirror_->is_portal)
            {
                HMM_Vec2 tmp;
                tmp = v1;
                v1  = v2;
                v2  = tmp;
            }

            active_mirrors[k].Transform(v1.X, v1.Y);
            active_mirrors[k].Transform(v2.X, v2.Y);
        }

        GLdouble front_p[4];

        ClipPlaneHorizontalLine(front_p, v2, v1);

        render_state->Enable(GL_CLIP_PLANE2 + i);

        render_state->ClipPlane(GL_CLIP_PLANE2 + i, front_p);
    }
}

void MirrorPush(DrawMirror *mir)
{
    EPI_ASSERT(mir);
    EPI_ASSERT(mir->seg);

    EPI_ASSERT(total_active_mirrors < kMaximumMirrors);

    active_mirrors[total_active_mirrors].draw_mirror_ = mir;
    active_mirrors[total_active_mirrors].Compute();

    total_active_mirrors++;

    MirrorSetClippers();
}

void MirrorPop()
{
    EPI_ASSERT(total_active_mirrors > 0);

    total_active_mirrors--;

    MirrorSetClippers();
}

static void DrawMirrorPolygon(DrawMirror *mir)
{
    float alpha = 0.15 + 0.10 * total_active_mirrors;

    Line *ld = mir->seg->linedef;
    EPI_ASSERT(ld);
    RGBAColor unit_col;

    if (ld->special)
    {
        uint8_t col_r = epi::GetRGBARed(ld->special->fx_color_);
        uint8_t col_g = epi::GetRGBAGreen(ld->special->fx_color_);
        uint8_t col_b = epi::GetRGBABlue(ld->special->fx_color_);

        // looks better with reduced color in multiple reflections
        float reduce = 1.0f / (1 + 1.5 * total_active_mirrors);

        unit_col = epi::MakeRGBA((uint8_t)(reduce * col_r), (uint8_t)(reduce * col_g), (uint8_t)(reduce * col_b),
                                 (uint8_t)(alpha * 255.0f));
    }
    else
        unit_col = epi::MakeRGBA(255, 0, 0, (uint8_t)(alpha * 255.0f));

    float x1 = mir->seg->vertex_1->X;
    float y1 = mir->seg->vertex_1->Y;
    float z1 = ld->front_sector->interpolated_floor_height;

    float x2 = mir->seg->vertex_2->X;
    float y2 = mir->seg->vertex_2->Y;
    float z2 = ld->front_sector->interpolated_ceiling_height;

    MirrorCoordinate(x1, y1);
    MirrorCoordinate(x2, y2);

    RendererVertex *glvert = BeginRenderUnit(GL_POLYGON, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0,
                                             alpha < 0.99f ? kBlendingAlpha : kBlendingNone);

    glvert->rgba       = unit_col;
    glvert++->position = {{x1, y1, z1}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x1, y1, z2}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x2, y2, z2}};
    glvert->rgba       = unit_col;
    glvert->position   = {{x2, y2, z1}};

    EndRenderUnit(4);
}

static void DrawPortalPolygon(DrawMirror *mir)
{
    Line *ld = mir->seg->linedef;
    EPI_ASSERT(ld);

    const MapSurface *surf = &mir->seg->sidedef->middle;

    if (!surf->image || !ld->special || !(ld->special->portal_effect_ & kPortalEffectTypeStandard))
    {
        DrawMirrorPolygon(mir);
        return;
    }

    // set texture
    GLuint tex_id = ImageCache(surf->image);

    // set colour & alpha
    float alpha = ld->special->translucency_ * surf->translucency;

    RGBAColor unit_col = ld->special->fx_color_;
    epi::SetRGBAAlpha(unit_col, alpha);

    // get polygon coordinates
    float x1 = mir->seg->vertex_1->X;
    float y1 = mir->seg->vertex_1->Y;
    float z1 = ld->front_sector->interpolated_floor_height;

    float x2 = mir->seg->vertex_2->X;
    float y2 = mir->seg->vertex_2->Y;
    float z2 = ld->front_sector->interpolated_ceiling_height;

    MirrorCoordinate(x1, y1);
    MirrorCoordinate(x2, y2);

    // get texture coordinates
    float total_w = surf->image->ScaledWidthTotal();
    float total_h = surf->image->ScaledHeightTotal();

    float tx1 = mir->seg->offset;
    float tx2 = tx1 + mir->seg->length;

    float ty1 = 0;
    float ty2 = (z2 - z1);

    tx1 = tx1 * surf->x_matrix.X / total_w;
    tx2 = tx2 * surf->x_matrix.X / total_w;

    ty1 = ty1 * surf->y_matrix.Y / total_h;
    ty2 = ty2 * surf->y_matrix.Y / total_h;

    RendererVertex *glvert = BeginRenderUnit(GL_POLYGON, 4, GL_MODULATE, tex_id, (GLuint)kTextureEnvironmentDisable, 0,
                                             0, alpha < 0.99f ? kBlendingAlpha : kBlendingNone);

    glvert->rgba                     = unit_col;
    glvert->position                 = {{x1, y1, z1}};
    glvert++->texture_coordinates[0] = {{tx1, ty1}};
    glvert->rgba                     = unit_col;
    glvert->position                 = {{x1, y1, z2}};
    glvert++->texture_coordinates[0] = {{tx1, ty2}};
    glvert->rgba                     = unit_col;
    glvert->position                 = {{x2, y2, z2}};
    glvert++->texture_coordinates[0] = {{tx2, ty2}};
    glvert->rgba                     = unit_col;
    glvert->position                 = {{x2, y2, z1}};
    glvert->texture_coordinates[0]   = {{tx2, ty1}};

    EndRenderUnit(4);
}

void RenderMirror(DrawMirror *mir)
{
    // mark the segment on the automap
    mir->seg->linedef->flags |= kLineFlagMapped;

    FinishUnitBatch();

#if defined(EDGE_GL_ES2)
    // GL4ES mirror fix for renderlist
    gl4es_flush();
#endif

    MirrorPush(mir);
    {
        RenderSubList(mir->draw_subsectors, true);
    }
    MirrorPop();

    StartUnitBatch(false);

    if (mir->is_portal)
        DrawPortalPolygon(mir);
    else
        DrawMirrorPolygon(mir);

    FinishUnitBatch();

#if defined(EDGE_GL_ES2)
    // GL4ES mirror fix for renderlist
    gl4es_flush();
#endif

    solid_mode = true;
    StartUnitBatch(solid_mode);
}
