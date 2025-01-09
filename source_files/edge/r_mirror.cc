
#include "epi_doomdefs.h"
#include "r_image.h"
#include "r_mirror.h"
#include "r_render.h"
#include "r_units.h"

extern MirrorSet render_mirror_set;
extern bool solid_mode;

static void DrawMirrorPolygon(DrawMirror *mir)
{
    float alpha = 0.15 + 0.10 * render_mirror_set.TotalActive();

    Line *ld = mir->seg->linedef;
    EPI_ASSERT(ld);
    RGBAColor unit_col;

    if (ld->special)
    {
        uint8_t col_r = epi::GetRGBARed(ld->special->fx_color_);
        uint8_t col_g = epi::GetRGBAGreen(ld->special->fx_color_);
        uint8_t col_b = epi::GetRGBABlue(ld->special->fx_color_);

        // looks better with reduced color in multiple reflections
        float reduce = 1.0f / (1 + 1.5 * render_mirror_set.TotalActive());

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

    render_mirror_set.Coordinate(x1, y1);
    render_mirror_set.Coordinate(x2, y2);

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

    render_mirror_set.Coordinate(x1, y1);
    render_mirror_set.Coordinate(x2, y2);

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

    render_mirror_set.Push(mir);
    {
        RenderSubList(mir->draw_subsectors, true);
    }
    render_mirror_set.Pop();

    StartUnitBatch(false);

    if (mir->is_portal)
        DrawPortalPolygon(mir);
    else
        DrawMirrorPolygon(mir);

    FinishUnitBatch();

    solid_mode = true;
    StartUnitBatch(solid_mode);
}