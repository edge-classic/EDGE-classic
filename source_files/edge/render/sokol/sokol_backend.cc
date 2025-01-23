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

#include "epi.h"
#include "i_video.h"

// clang-format on

extern ConsoleVariable vsync;
void                   BSPStartThread();
void                   BSPStopThread();

constexpr int32_t kWorldStateInvalid = -1;

class SokolRenderBackend : public RenderBackend
{
  public:
    void SetupMatrices2D()
    {
        sgl_viewport(0, 0, current_screen_width, current_screen_height, false);

        sgl_matrix_mode_projection();
        sgl_load_identity();
        sgl_ortho(0.0f, (float)current_screen_width, 0.0f, (float)current_screen_height, -1.0f, 1.0f);

        sgl_matrix_mode_modelview();
        sgl_load_identity();
    }

    void SetupWorldMatrices2D()
    {
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

        sgl_set_context(context_);

        sg_pass_action pass_action;
        pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
        pass_action.colors[0].clear_value = {epi::GetRGBARed(clear_color_) / 255.0f,
                                             epi::GetRGBAGreen(clear_color_) / 255.0f,
                                             epi::GetRGBABlue(clear_color_) / 255.0f, 1.0f};

        pass_action.depth.load_action = SG_LOADACTION_CLEAR;
        pass_action.depth.clear_value = 1.0f;
        pass_action.stencil = {SG_LOADACTION_CLEAR, SG_STOREACTION_DONTCARE, 0};

        EPI_CLEAR_MEMORY(&pass_, sg_pass, 1);
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

        imgui_frame_desc_            = {0};
        imgui_frame_desc_.width      = width;
        imgui_frame_desc_.height     = height;
        imgui_frame_desc_.delta_time = 100;
        imgui_frame_desc_.dpi_scale  = 1;

        EPI_CLEAR_MEMORY(world_state_, WorldState, kRenderWorldMax);

        EPI_CLEAR_MEMORY(&render_state_, RenderState, 1);
        render_state_.world_state_ = kWorldStateInvalid;

        SetRenderLayer(kRenderLayerHUD);

        sg_begin_pass(&pass_);
    }

    void SwapBuffers()
    {
#ifdef SOKOL_D3D11
        sapp_d3d11_present(false, vsync.d_ ? 1 : 0);
#endif
    }

    void FinishFrame()
    {

        // World
        for (int32_t i = 0; i < kRenderWorldMax; i++)
        {
            if (!world_state_[i].used_)
            {
                break;
            }

            int32_t base_layer = kRenderLayerSky + i * 4 + 1;
            for (int32_t j = 0; j < 4; j++)
            {
                sgl_context_draw_layer(context_, base_layer + j);
            }
        }

        // Hud
        sgl_context_draw_layer(context_, kRenderLayerHUD + 1);

        // default layer
        sgl_context_draw_layer(context_, 0);

        sg_imgui_.caps_window.open        = false;
        sg_imgui_.buffer_window.open      = false;
        sg_imgui_.pipeline_window.open    = false;
        sg_imgui_.attachments_window.open = false;
        sg_imgui_.frame_stats_window.open = false;

        simgui_new_frame(&imgui_frame_desc_);
        sgimgui_draw(&sg_imgui_);

        simgui_render();

        sg_end_pass();
        sg_commit();

        for (auto itr = on_frame_finished_.begin(); itr != on_frame_finished_.end(); itr++)
        {
            (*itr)();
        }

        on_frame_finished_.clear();
    }

    void Resize(int32_t width, int32_t height)
    {
#ifdef SOKOL_D3D11
        deferred_resize        = true;
        deferred_resize_width  = width;
        deferred_resize_height = height;
#else
        EPI_UNUSED(width);
        EPI_UNUSED(height);
#endif
    }

    void Shutdown()
    {
#ifdef SOKOL_D3D11
        sapp_d3d11_destroy_device_and_swapchain();
#endif

        BSPStopThread();
    }

#if defined (SOKOL_GLCORE) || defined (SOKOL_GLES3)
    void CaptureScreenGL(int32_t width, int32_t height, int32_t stride, uint8_t *dest)
    {
        for (int32_t y = 0; y < height; y++)
        {
            render_state->ReadPixels(0, y, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, dest);
            dest += stride;
        }
    }
#endif

    void CaptureScreen(int32_t width, int32_t height, int32_t stride, uint8_t *dest)
    {
#if defined (SOKOL_GLCORE) || defined (SOKOL_GLES3)
        CaptureScreenGL(width, height, stride, dest);
#endif

#ifdef SOKOL_D3D11
        sapp_d3d11_capture_screen(width, height, stride, dest);
#endif
    }

    void Init()
    {
#if SOKOL_GLES3
        LogPrint("Sokol GLES3: Initialising...\n");
#elif SOKOL_GLCORE
        LogPrint("Sokol GL: Initialising...\n");
#else
        LogPrint("Sokol D3D11: Initialising...\n");
#endif

        // TODO: should be able to query from sokol?
        max_texture_size_ = 4096;

        sg_environment env;
        EPI_CLEAR_MEMORY(&env, sg_environment, 1);
        env.defaults.color_format = SG_PIXELFORMAT_RGBA8;
        env.defaults.depth_format = SG_PIXELFORMAT_DEPTH;
        env.defaults.sample_count = 1;

#ifdef SOKOL_D3D11
        sapp_d3d11_init(program_window, current_screen_width, current_screen_height);
        env.d3d11.device         = sapp_d3d11_get_device();
        env.d3d11.device_context = sapp_d3d11_get_device_context();
#endif

        sg_desc desc;
        EPI_CLEAR_MEMORY(&desc, sg_desc, 1);
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
        EPI_CLEAR_MEMORY(&sgl_desc, sgl_desc_t, 1);
        sgl_desc.color_format       = SG_PIXELFORMAT_RGBA8;
        sgl_desc.depth_format       = SG_PIXELFORMAT_DEPTH;
        sgl_desc.sample_count       = 1;
        sgl_desc.pipeline_pool_size = 512;
        sgl_desc.logger.func        = slog_func;
        sgl_setup(&sgl_desc);

        // 2D
        sgl_context_desc_t context_desc_2d;
        EPI_CLEAR_MEMORY(&context_desc_2d, sgl_context_desc_t, 1);
        context_desc_2d.color_format = SG_PIXELFORMAT_RGBA8;
        context_desc_2d.depth_format = SG_PIXELFORMAT_DEPTH;
        context_desc_2d.sample_count = 1;
        context_desc_2d.max_commands = 256 * 1024;
        context_desc_2d.max_vertices = 1024 * 1024;

        context_ = sgl_make_context(&context_desc_2d);

        sgl_set_context(context_);

        // IMGUI
        simgui_desc_t imgui_desc = {0};
        imgui_desc.logger.func   = slog_func;
        simgui_setup(&imgui_desc);

        const sgimgui_desc_t sg_imgui_desc = {0};
        sgimgui_init(&sg_imgui_, &sg_imgui_desc);

        InitPipelines();
        InitImages();

        EPI_CLEAR_MEMORY(world_state_, WorldState, kRenderWorldMax);

        EPI_CLEAR_MEMORY(&render_state_, RenderState, 1);
        render_state_.world_state_ = kWorldStateInvalid;

        RenderBackend::Init();

        BSPStartThread();
    }

    // FIXME: go away!
    void GetPassInfo(PassInfo &info)
    {
        info.width_  = pass_.swapchain.width;
        info.height_ = pass_.swapchain.height;
    }

    void SetClearColor(RGBAColor color)
    {
        clear_color_ = color;
    }

    int32_t GetHUDLayer()
    {
        return kRenderLayerHUD;
    }

    virtual void SetRenderLayer(RenderLayer layer, bool clear_depth = false)
    {
        render_state_.layer_ = layer;

        if (layer == kRenderLayerHUD)
        {
            render_state_.sokol_layer_ = 1;
        }
        else
        {
            render_state_.sokol_layer_ = layer + render_state_.world_state_ * 4 + 1;
        }

        sgl_layer(render_state_.sokol_layer_);

        if (clear_depth)
        {
            sgl_clear_depth(1.0f);
        }
    }

    RenderLayer GetRenderLayer()
    {
        return render_state_.layer_;
    }

    void BeginWorldRender()
    {
        int32_t i = 0;
        for (; i < kRenderWorldMax; i++)
        {
            if (world_state_[i].active_)
            {
                FatalError("SokolRenderBackend: BeginWorldState called with active world");
            }

            if (!world_state_[i].used_)
            {
                break;
            }
        }

        if (i == kRenderWorldMax)
        {
            FatalError("SokolRenderBackend: BeginWorldState max worlds exceeded");
        }

        world_state_[i].active_    = true;
        world_state_[i].used_      = true;
        render_state_.world_state_ = i;
    }

    void FinishWorldRender()
    {
        render_state_.world_state_ = kWorldStateInvalid;

        int32_t i = 0;
        for (; i < kRenderWorldMax; i++)
        {
            if (world_state_[i].active_)
            {
                world_state_[i].active_ = false;
                break;
            }
        }

        if (i == kRenderWorldMax)
        {
            FatalError("SokolRenderBackend: FinishWorldState called with no active world render");
        }

        SetRenderLayer(kRenderLayerHUD);
        SetupMatrices2D();
    }

  private:
    struct WorldState
    {
        bool active_;
        bool used_;
    };

    struct RenderState
    {
        RenderLayer layer_;
        int32_t     sokol_layer_;
        int32_t     world_state_;
    };

    simgui_frame_desc_t imgui_frame_desc_;
    sgimgui_t           sg_imgui_;

    RGBAColor clear_color_ = kRGBABlack;

    sgl_context context_;

    RenderState render_state_;

    sg_pass pass_;

    WorldState world_state_[kRenderWorldMax];

#ifdef SOKOL_D3D11
    bool    deferred_resize        = false;
    int32_t deferred_resize_width  = 0;
    int32_t deferred_resize_height = 0;
#endif
};

static SokolRenderBackend sokol_render_backend;
RenderBackend            *render_backend = &sokol_render_backend;
