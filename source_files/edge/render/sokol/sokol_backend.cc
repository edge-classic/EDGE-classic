// clang-format off
#include "../../r_backend.h"
#include "sokol_local.h"
#include "sokol_imgui.h"
#include "sokol_gfx_imgui.h"
#include "sokol_pipeline.h"
#include "sokol_images.h"

#ifdef SOKOL_D3D11
#include "sokol_d3d11.h"
#endif

#include "i_video.h"

// clang-format on

class SokolRenderBackend : public RenderBackend
{
  public:
    void SetupMatrices2D()
    {
        sgl_set_context(context_2d_);

        sgl_viewport(0, 0, current_screen_width, current_screen_height, false);

        sgl_matrix_mode_projection();
        sgl_load_identity();
        sgl_ortho(0.0f, (float)current_screen_width, 0.0f, (float)current_screen_height, -1.0f, 1.0f);

        sgl_matrix_mode_modelview();
        sgl_load_identity();
    }

    void SetupWorldMatrices2D()
    {
        sgl_set_context(context_2d_);

        sgl_viewport(view_window_x, view_window_y, view_window_width, view_window_height, false);

        sgl_matrix_mode_projection();
        sgl_load_identity();
        sgl_ortho((float)view_window_x, (float)view_window_width, (float)view_window_y, (float)view_window_height,
                  -1.0f, 1.0f);

        sgl_matrix_mode_modelview();
        sgl_load_identity();
    }

    void SetupMatrices3D()
    {
        sgl_set_context(context_3d_);

        sgl_viewport(view_window_x, view_window_y, view_window_width, view_window_height, false);

        // calculate perspective matrix

        sgl_matrix_mode_projection();
        sgl_load_identity();

        sgl_frustum(-view_x_slope * renderer_near_clip.f_, view_x_slope * renderer_near_clip.f_,
                    -view_y_slope * renderer_near_clip.f_, view_y_slope * renderer_near_clip.f_, renderer_near_clip.f_,
                    renderer_far_clip.f_);

        // calculate look-at matrix
        sgl_matrix_mode_modelview();
        sgl_load_identity();
        sgl_rotate(sgl_rad(270.0f) - epi::RadiansFromBAM(view_vertical_angle), 1.0f, 0.0f, 0.0f);
        sgl_rotate(sgl_rad(90.0f) - epi::RadiansFromBAM(view_angle), 0.0f, 0.0f, 1.0f);
        sgl_translate(-view_x, -view_y, -view_z);
    }

    void StartFrame(int32_t width, int32_t height)
    {
        frame_number_++;
#ifdef SOKOL_D3D11
        if (deferred_resize)
        {
            deferred_resize = false;
            sapp_d3d11_resize_default_render_target(deferred_resize_width, deferred_resize_height);
        }
#endif

        FinalizeDeletedImages();

        sg_pass_action pass_action;
        pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
        pass_action.colors[0].clear_value = {0.0f, 0.0f, 0.0f, 1.0f};

        pass_action.depth.load_action = SG_LOADACTION_CLEAR;
        pass_action.depth.clear_value = 1.0f;

        pass_                          = {0};
        pass_.action                   = pass_action;
        pass_.swapchain.width          = width;
        pass_.swapchain.height         = height;
        pass_.swapchain.color_format   = SG_PIXELFORMAT_RGBA8;
        pass_.swapchain.depth_format   = SG_PIXELFORMAT_DEPTH;
        pass_.swapchain.gl.framebuffer = 0;

#ifdef SOKOL_D3D11
        pass_.swapchain.d3d11.render_view        = sapp_d3d11_get_render_view();
        pass_.swapchain.d3d11.resolve_view       = sapp_d3d11_get_resolve_view();
        pass_.swapchain.d3d11.depth_stencil_view = sapp_d3d11_get_depth_stencil_view();
#endif

        /*
                imgui_frame_desc_            = {0};
                imgui_frame_desc_.width      = width;
                imgui_frame_desc_.height     = height;
                imgui_frame_desc_.delta_time = 100;
                imgui_frame_desc_.dpi_scale  = 1;
        */

        sg_begin_pass(&pass_);
    }

    void SwapBuffers()
    {
#ifdef SOKOL_D3D11
        sapp_d3d11_present(false);
#endif
    }

    void FinishFrame()
    {
        sgl_context_draw(context_3d_);
        sgl_context_draw(context_2d_);

        /*
        sg_imgui_.caps_window.open        = false;
        sg_imgui_.buffer_window.open      = false;
        sg_imgui_.pipeline_window.open    = false;
        sg_imgui_.attachments_window.open = false;
        sg_imgui_.frame_stats_window.open = false;

        simgui_new_frame(&imgui_frame_desc_);
        sgimgui_draw(&sg_imgui_);

        simgui_render();
        */

        sg_end_pass();
        sg_commit();
    }

    void Resize(int32_t width, int32_t height)
    {
#ifdef SOKOL_D3D11
        deferred_resize        = true;
        deferred_resize_width  = width;
        deferred_resize_height = height;
#endif
    }

    void Shutdown()
    {
#ifdef SOKOL_D3D11
        sapp_d3d11_destroy_device_and_swapchain();
#endif
    }

    void Init()
    {
        LogPrint("Sokol: Initialising...\n");

        // TODO: should be able to query from sokol?
        max_texture_size_ = 4096;

        sg_environment env;
        memset(&env, 0, sizeof(env));
        env.defaults.color_format = SG_PIXELFORMAT_RGBA8;
        env.defaults.depth_format = SG_PIXELFORMAT_DEPTH;
        env.defaults.sample_count = 1;

#ifdef SOKOL_D3D11
        sapp_d3d11_init(program_window, current_screen_width, current_screen_height);
        env.d3d11.device         = sapp_d3d11_get_device();
        env.d3d11.device_context = sapp_d3d11_get_device_context();
#endif

        sg_desc desc{0};
        desc.environment        = env;
        desc.logger.func        = slog_func;
        desc.pipeline_pool_size = 512;
        desc.image_pool_size    = 8192;

        sg_setup(&desc);

        if (!sg_isvalid())
        {
            FatalError("Sokol invalid");
        }

        sgl_desc_t sgl_desc;
        memset(&sgl_desc, 0, sizeof(sgl_desc));
        sgl_desc.color_format       = SG_PIXELFORMAT_RGBA8;
        sgl_desc.depth_format       = SG_PIXELFORMAT_DEPTH;
        sgl_desc.sample_count       = 1;
        sgl_desc.pipeline_pool_size = 512;
        sgl_desc.logger.func        = slog_func;
        sgl_setup(&sgl_desc);

        // 2D
        sgl_context_desc_t context_desc_2d = {0};
        context_desc_2d.color_format       = SG_PIXELFORMAT_RGBA8;
        context_desc_2d.depth_format       = SG_PIXELFORMAT_DEPTH;
        context_desc_2d.sample_count       = 1;
        context_desc_2d.max_commands       = 16 * 1024;
        context_desc_2d.max_vertices       = 128 * 1024;

        context_2d_ = sgl_make_context(&context_desc_2d);

        // 3D
        sgl_context_desc_t context_desc_3d = {0};
        context_desc_3d.color_format       = SG_PIXELFORMAT_RGBA8;
        context_desc_3d.depth_format       = SG_PIXELFORMAT_DEPTH;
        context_desc_3d.sample_count       = 1;
        context_desc_3d.max_commands       = 32 * 1024;
        context_desc_3d.max_vertices       = 256 * 1024;

        context_3d_ = sgl_make_context(&context_desc_3d);

        /*
                // IMGUI
                simgui_desc_t imgui_desc = {0};
                imgui_desc.logger.func   = slog_func;
                simgui_setup(&imgui_desc);

                const sgimgui_desc_t sg_imgui_desc = {0};
                sgimgui_init(&sg_imgui_, &sg_imgui_desc);
        */
        InitPipelines();
        InitImages();

        RenderBackend::Init();
    }

    void GetPassInfo(PassInfo &info)
    {
        info.width_ = pass_.swapchain.width;
        info.height_ = pass_.swapchain.height;
    }

  private:
    bool    deferred_resize        = false;
    int32_t deferred_resize_width  = 0;
    int32_t deferred_resize_height = 0;

    simgui_frame_desc_t imgui_frame_desc_;
    sgimgui_t           sg_imgui_;

    // 2D
    sgl_context context_2d_;

    // 3D
    sgl_context context_3d_;

    sg_pass pass_;
};

static SokolRenderBackend sokol_render_backend;
RenderBackend            *render_backend = &sokol_render_backend;
