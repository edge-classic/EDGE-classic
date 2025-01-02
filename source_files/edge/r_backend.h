
#pragma once
#include <functional>

#include "dm_defs.h"
#include "epi_color.h"

struct PassInfo
{
    int32_t width_;
    int32_t height_;
};
constexpr int32_t kRenderWorldMax = 8;

enum RenderLayer
{
    kRenderLayerHUD = 0,
    kRenderLayerSky,
    kRenderLayerSolid,
    kRenderLayerTransparent, // Transparent - additive renders on this layer
    kRenderLayerWeapon,
    kRenderLayerMax,
    kRenderLayerInvalid
};

typedef std::function<void()> FrameFinishedCallback;

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

    virtual void SetClearColor(RGBAColor color) = 0;

    virtual void StartFrame(int32_t width, int32_t height) = 0;

    virtual void SwapBuffers() = 0;

    virtual void FinishFrame() = 0;

    void OnFrameFinished(FrameFinishedCallback callback)
    {
        on_frame_finished_.push_back(callback);
    }

    virtual void BeginWorldRender() = 0;

    virtual void FinishWorldRender() = 0;

    virtual void SetRenderLayer(RenderLayer layer, bool clear_depth = false) = 0;

    virtual RenderLayer GetRenderLayer() = 0;

    void LockRenderUnits(bool locked) 
    {
        units_locked_ = locked;
    }

    bool RenderUnitsLocked() 
    {
        return units_locked_;
    }

    virtual void Resize(int32_t width, int32_t height) = 0;

    virtual void Shutdown() = 0;

    virtual void SoftInit();

    virtual void Init();

    virtual void GetPassInfo(PassInfo &info) = 0;

    virtual void CaptureScreen(int32_t width, int32_t height, int32_t stride, uint8_t *dest) = 0;

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
    bool units_locked_ = false;

    std::vector<FrameFinishedCallback> on_frame_finished_;
};

extern RenderBackend *render_backend;