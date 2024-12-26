
#pragma once

#include "dm_defs.h"

struct PassInfo
{
    int32_t width_;
    int32_t height_;
};

class RenderBackend
{
  public:
    // Setup the GL matrices for drawing 2D stuff.
    virtual void SetupMatrices2D() = 0;

    // Setup the GL matrices for drawing 2D stuff within the "world" rendered by
    // HUDRenderWorld
    virtual void SetupWorldMatrices2D() = 0;

    // Setup the GL matrices for drawing 3D stuff.
    virtual void SetupMatrices3D() = 0;

    virtual void StartFrame(int32_t width, int32_t height) = 0;

    virtual void SwapBuffers() = 0;

    virtual void FinishFrame() = 0;

    virtual void Resize(int32_t width, int32_t height) = 0;

    virtual void Shutdown() = 0;

    virtual void SoftInit();

    virtual void Init();

    virtual void GetPassInfo(PassInfo& info) = 0;

    int64_t GetFrameNumber()
    {
        return frame_number_;
    }

    int32_t GetMaxTextureSize() const
    {
        return max_texture_size_;
    }

  protected:
    int32_t max_texture_size_ = 0;
    int64_t frame_number_;
};

extern RenderBackend *render_backend;