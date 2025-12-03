
#include "../../r_backend.h"
#include "g_game.h"
#include "i_defs_gl.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"

void SetupSkyMatrices(void);

static inline const char *SafeStr(const void *s)
{
    return s ? (const char *)s : "";
}

class GLRenderBackend : public RenderBackend
{
  private:
    void SetupMatrices2D(bool flip)
    {
        glViewport(0, 0, current_screen_width, current_screen_height);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        if (flip)
            glOrtho((float)current_screen_width, 0.0f, 0.0f, (float)current_screen_height, -1.0f, 1.0f);
        else
            glOrtho(0.0f, (float)current_screen_width, 0.0f, (float)current_screen_height, -1.0f, 1.0f);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void SetupWorldMatrices2D()
    {
        glViewport(view_window_x, view_window_y, view_window_width, view_window_height);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        if (fliplevels.d_)
            glOrtho((float)view_window_width, (float)view_window_x, (float)view_window_y, (float)view_window_height, -1.0f,
                    1.0f);
        else
            glOrtho((float)view_window_x, (float)view_window_width, (float)view_window_y, (float)view_window_height, -1.0f,
                    1.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void SetupMatrices3D()
    {
        glViewport(view_window_x, view_window_y, view_window_width, view_window_height);

        // calculate perspective matrix
        glMatrixMode(GL_PROJECTION);

        glLoadIdentity();

         if (fliplevels.d_)
            glFrustum(view_x_slope * renderer_near_clip.f_, -view_x_slope * renderer_near_clip.f_,
                    -view_y_slope * renderer_near_clip.f_, view_y_slope * renderer_near_clip.f_, renderer_near_clip.f_,
                    renderer_far_clip.f_);
        else
            glFrustum(-view_x_slope * renderer_near_clip.f_, view_x_slope * renderer_near_clip.f_,
                    -view_y_slope * renderer_near_clip.f_, view_y_slope * renderer_near_clip.f_, renderer_near_clip.f_,
                    renderer_far_clip.f_);

        // calculate look-at matrix
        glMatrixMode(GL_MODELVIEW);

        glLoadIdentity();
        glRotatef(270.0f - epi::DegreesFromBAM(view_vertical_angle), 1.0f, 0.0f, 0.0f);
        glRotatef(90.0f - epi::DegreesFromBAM(view_angle), 0.0f, 0.0f, 1.0f);
        glTranslatef(-view_x, -view_y, -view_z);
    }

  public:
    // CheckExtensions - Based on code by Bruce Lewis.
    void CheckExtensions()
    {
        // -ACB- 2004/08/11 Made local: these are not yet used elsewhere
        std::string glstr_version(SafeStr(glGetString(GL_VERSION)));
        std::string glstr_renderer(SafeStr(glGetString(GL_RENDERER)));
        std::string glstr_vendor(SafeStr(glGetString(GL_VENDOR)));

        LogPrint("OpenGL: Version: %s\n", glstr_version.c_str());
        LogPrint("OpenGL: Renderer: %s\n", glstr_renderer.c_str());
        LogPrint("OpenGL: Vendor: %s\n", glstr_vendor.c_str());
    }

    void Init()
    {
        LogPrint("OpenGL: Initialising...\n");
        CheckExtensions();

        // read implementation limits
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size_);

        LogPrint("OpenGL Max Texture Size: %d\n", max_texture_size_);

        RenderBackend::Init();
    }

    void CaptureScreen(int32_t width, int32_t height, int32_t stride, uint8_t *dest)
    {
        render_state->Flush();
        render_state->PixelZoom(1.0f, 1.0f);
        render_state->PixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (int32_t y = 0; y < height; y++)
        {
            render_state->ReadPixels(0, y, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, dest);
            dest += stride;
        }
    }

    void StartFrame(int32_t width, int32_t height)
    {
        EPI_UNUSED(width);
        EPI_UNUSED(height);
        frame_number_++;
    }

    void SwapBuffers()
    {
    }

    void FinishFrame()
    {
        for (auto itr = on_frame_finished_.begin(); itr != on_frame_finished_.end(); itr++)
        {
            (*itr)();
        }

        on_frame_finished_.clear();
    }

    void Resize(int32_t width, int32_t height)
    {
        EPI_UNUSED(width);
        EPI_UNUSED(height);
    }

    void Shutdown()
    {
    }

    void SetClearColor(RGBAColor color)
    {
        EPI_UNUSED(color);
    }

    void GetPassInfo(PassInfo &info)
    {
        info.width_  = 0;
        info.height_ = 0;
    }

    void BeginWorldRender()
    {
    }

    void FinishWorldRender()
    {
    }

    void SetRenderLayer(RenderLayer layer, bool clear_depth = false)
    {
        if (layer == kRenderLayerHUD)
        {
            SetupMatrices2D(false);
        }
        else if (layer == kRenderLayerSky)
        {
            SetupSkyMatrices();
        }
        else if (layer == kRenderLayerViewport)
        {
            SetupWorldMatrices2D();
        }
        else
        {
            SetupMatrices3D();
        }

        if (clear_depth)
        {
            glClear(GL_DEPTH_BUFFER_BIT);
        }
    }

    RenderLayer GetRenderLayer()
    {
        return kRenderLayerInvalid;
    }

    void Flush(int32_t commands, int32_t vertices)
    {
        EPI_UNUSED(commands);
        EPI_UNUSED(vertices);
    }

    void GetFrameStats(FrameStats &stats)
    {
        EPI_UNUSED(stats);
    }

    void OnContextSwitch()
    {
    }
};

static GLRenderBackend gl_render_backend;
RenderBackend         *render_backend = &gl_render_backend;
