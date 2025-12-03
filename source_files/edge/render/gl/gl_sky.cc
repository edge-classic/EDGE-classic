
#include <math.h>

#include "con_var.h"
#include "dm_state.h"
#include "epi.h"
#include "g_game.h" // current_map
#include "i_defs_gl.h"
#include "im_data.h"
#include "m_math.h"
#include "n_network.h"
#include "p_tick.h"
#include "r_colormap.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_sky.h"
#include "r_texgl.h"
#include "r_units.h"
#include "stb_sprintf.h"
#include "w_flat.h"
#include "w_wad.h"

extern ConsoleVariable fliplevels;
extern SkyStretch current_sky_stretch;

void SetupSkyMatrices(void)
{
    if (custom_skybox)
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();

        if (fliplevels.d_)
            glFrustum(-view_x_slope * renderer_near_clip.f_, view_x_slope * renderer_near_clip.f_,
                      -view_y_slope * renderer_near_clip.f_, view_y_slope * renderer_near_clip.f_,
                      renderer_near_clip.f_, renderer_far_clip.f_);
        else
            glFrustum(view_x_slope * renderer_near_clip.f_, -view_x_slope * renderer_near_clip.f_,
                      -view_y_slope * renderer_near_clip.f_, view_y_slope * renderer_near_clip.f_,
                      renderer_near_clip.f_, renderer_far_clip.f_);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glRotatef(270.0f - epi::DegreesFromBAM(view_vertical_angle), 1.0f, 0.0f, 0.0f);
        glRotatef(epi::DegreesFromBAM(view_angle), 0.0f, 0.0f, 1.0f);
    }
    else
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();

        if (fliplevels.d_)
            glFrustum(view_x_slope * renderer_near_clip.f_, -view_x_slope * renderer_near_clip.f_,
                    -view_y_slope * renderer_near_clip.f_, view_y_slope * renderer_near_clip.f_, renderer_near_clip.f_,
                    renderer_far_clip.f_ * 4.0);
        else
            glFrustum(-view_x_slope * renderer_near_clip.f_, view_x_slope * renderer_near_clip.f_,
                    -view_y_slope * renderer_near_clip.f_, view_y_slope * renderer_near_clip.f_, renderer_near_clip.f_,
                    renderer_far_clip.f_ * 4.0);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glRotatef(270.0f - epi::DegreesFromBAM(view_vertical_angle), 1.0f, 0.0f, 0.0f);

        BAMAngle rot = view_angle;

        if (sky_ref)
        {
            if (!AlmostEquals(sky_ref->old_offset.X, sky_ref->offset.X) && !console_active && !paused && !menu_active &&
                !time_stop_active && !erraticism_active)
                rot += epi::BAMFromDegrees(HMM_Lerp(sky_ref->old_offset.X, fractional_tic, sky_ref->offset.X) /
                                           sky_image->ScaledWidth());
            else
                rot += epi::BAMFromDegrees(sky_ref->offset.X / sky_image->ScaledWidth());
        }

        glRotatef(-epi::DegreesFromBAM(rot), 0.0f, 0.0f, 1.0f);

        if (current_sky_stretch == kSkyStretchStretch)
            glTranslatef(0.0f, 0.0f,
                         (renderer_far_clip.f_ * 2 * 0.15));  // Draw center above horizon a little
        else
            glTranslatef(0.0f, 0.0f,
                         -(renderer_far_clip.f_ * 2 * 0.15)); // Draw center below horizon a little
    }
}

void RendererRevertSkyMatrices(void)
{
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}