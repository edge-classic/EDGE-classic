
#include "sokol_local.h"

extern bool       custom_skybox;
extern SkyStretch current_sky_stretch;

void SetupSkyMatrices(void)
{
    if (custom_skybox)
    {
        sgl_matrix_mode_projection();
        //sgl_push_matrix();
        sgl_load_identity();

        sgl_frustum(view_x_slope * renderer_near_clip.f_, -view_x_slope * renderer_near_clip.f_,
                    -view_y_slope * renderer_near_clip.f_, view_y_slope * renderer_near_clip.f_, renderer_near_clip.f_,
                    renderer_far_clip.f_);

        sgl_matrix_mode_modelview();
        //sgl_push_matrix();
        sgl_load_identity();

        sgl_rotate(sgl_rad(270.0f) - epi::RadiansFromBAM(view_vertical_angle), 1.0f, 0.0f, 0.0f);
        sgl_rotate(epi::RadiansFromBAM(view_angle), 0.0f, 0.0f, 1.0f);
    }
    else
    {
        sgl_matrix_mode_projection();
        //sgl_push_matrix();
        sgl_load_identity();

        sgl_frustum(-view_x_slope * renderer_near_clip.f_, view_x_slope * renderer_near_clip.f_,
                    -view_y_slope * renderer_near_clip.f_, view_y_slope * renderer_near_clip.f_, renderer_near_clip.f_,
                    renderer_far_clip.f_ * 4.0);

        sgl_matrix_mode_modelview();
        //sgl_push_matrix();
        sgl_load_identity();

        sgl_rotate(sgl_rad(270.0f) - epi::RadiansFromBAM(view_vertical_angle), 1.0f, 0.0f, 0.0f);
        sgl_rotate(-epi::RadiansFromBAM(view_angle), 0.0f, 0.0f, 1.0f);

        if (current_sky_stretch == kSkyStretchStretch)
            sgl_translate(0.0f, 0.0f,
                          (renderer_far_clip.f_ * 2 * 0.15));  // Draw center above horizon a little
        else
            sgl_translate(0.0f, 0.0f,
                          -(renderer_far_clip.f_ * 2 * 0.15)); // Draw center below horizon a little
    }
}

void RendererRevertSkyMatrices(void)
{
    /*
    sgl_matrix_mode_projection();
    sgl_pop_matrix();

    sgl_matrix_mode_modelview();
    sgl_pop_matrix();
    */
}