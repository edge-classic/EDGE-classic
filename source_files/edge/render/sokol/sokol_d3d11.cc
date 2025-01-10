
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#ifndef D3D11_NO_HELPERS
#define D3D11_NO_HELPERS
#endif
#include <d3d11.h>
#include <dxgi.h>
// DXGI_SWAP_EFFECT_FLIP_DISCARD is only defined in newer Windows SDKs, so don't depend on it
#define _SAPP_DXGI_SWAP_EFFECT_FLIP_DISCARD (4)
#include "epi.h"
#include "sokol_local.h"

#define _SOKOL_UNUSED(x) (void)(x)

#define SOKOL_ASSERT(c) EPI_ASSERT(c)

#define SOKOL_CLEAR_MEMORY(ptr, type, num) EPI_CLEAR_MEMORY(ptr, type, num)

#define _sapp_d3d11_Release(self) (self)->Release()
#define _sapp_win32_refiid(iid)   iid

#define _SAPP_SAFE_RELEASE(obj)                                                                                        \
    if (obj)                                                                                                           \
    {                                                                                                                  \
        _sapp_d3d11_Release(obj);                                                                                      \
        obj = 0;                                                                                                       \
    }

static const IID _sapp_IID_ID3D11Texture2D = {
    0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c}};
static const IID _sapp_IID_IDXGIDevice1 = {
    0x77db970f, 0x6276, 0x48ba, {0xba, 0x28, 0x07, 0x01, 0x43, 0xb4, 0x39, 0x2c}};
static const IID _sapp_IID_IDXGIFactory = {
    0x7b7166ec, 0x21c7, 0x44ae, {0xb2, 0x1a, 0xc9, 0xae, 0x32, 0x1a, 0xe3, 0x69}};

typedef struct
{
    HWND hwnd;
    bool is_win10_or_greater;
} _sapp_win32_t;

typedef struct
{
    ID3D11Device           *device;
    ID3D11DeviceContext    *device_context;
    ID3D11Texture2D        *rt;
    ID3D11RenderTargetView *rtv;
    ID3D11Texture2D        *msaa_rt;
    ID3D11RenderTargetView *msaa_rtv;
    ID3D11Texture2D        *ds;
    ID3D11DepthStencilView *dsv;
    DXGI_SWAP_CHAIN_DESC    swap_chain_desc;
    IDXGISwapChain         *swap_chain;
    IDXGIDevice1           *dxgi_device;
    bool                    use_dxgi_frame_stats;
    UINT                    sync_refresh_count;
} _sapp_d3d11_t;

typedef struct
{
    _sapp_win32_t win32;
    _sapp_d3d11_t d3d11;
    int           framebuffer_width;
    int           framebuffer_height;
    int           sample_count;
} _sapp_t;

static _sapp_t _sapp;

/* don't laugh, but this seems to be the easiest and most robust
   way to check if we're running on Win10

   From:
   https://github.com/videolan/vlc/blob/232fb13b0d6110c4d1b683cde24cf9a7f2c5c2ea/modules/video_output/win32/d3d11_swapchain.c#L263
*/
static bool _sapp_win32_is_win10_or_greater(void)
{
    HMODULE h = GetModuleHandleW(L"kernel32.dll");
    if (NULL != h)
    {
        return (NULL != GetProcAddress(h, "GetSystemCpuSetInformation"));
    }
    else
    {
        return false;
    }
}

static inline HRESULT _sapp_dxgi_GetBuffer(IDXGISwapChain *self, UINT Buffer, REFIID riid, void **ppSurface)
{
#if defined(__cplusplus)
    return self->GetBuffer(Buffer, riid, ppSurface);
#else
    return self->lpVtbl->GetBuffer(self, Buffer, riid, ppSurface);
#endif
}

static inline HRESULT _sapp_d3d11_QueryInterface(ID3D11Device *self, REFIID riid, void **ppvObject)
{
#if defined(__cplusplus)
    return self->QueryInterface(riid, ppvObject);
#else
    return self->lpVtbl->QueryInterface(self, riid, ppvObject);
#endif
}

static inline HRESULT _sapp_d3d11_CreateRenderTargetView(ID3D11Device *self, ID3D11Resource *pResource,
                                                         const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
                                                         ID3D11RenderTargetView             **ppRTView)
{
#if defined(__cplusplus)
    return self->CreateRenderTargetView(pResource, pDesc, ppRTView);
#else
    return self->lpVtbl->CreateRenderTargetView(self, pResource, pDesc, ppRTView);
#endif
}

static inline HRESULT _sapp_d3d11_CreateTexture2D(ID3D11Device *self, const D3D11_TEXTURE2D_DESC *pDesc,
                                                  const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                  ID3D11Texture2D             **ppTexture2D)
{
#if defined(__cplusplus)
    return self->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
#else
    return self->lpVtbl->CreateTexture2D(self, pDesc, pInitialData, ppTexture2D);
#endif
}

static inline HRESULT _sapp_d3d11_CreateDepthStencilView(ID3D11Device *self, ID3D11Resource *pResource,
                                                         const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
                                                         ID3D11DepthStencilView             **ppDepthStencilView)
{
#if defined(__cplusplus)
    return self->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
#else
    return self->lpVtbl->CreateDepthStencilView(self, pResource, pDesc, ppDepthStencilView);
#endif
}

static inline HRESULT _sapp_dxgi_ResizeBuffers(IDXGISwapChain *self, UINT BufferCount, UINT Width, UINT Height,
                                               DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
#if defined(__cplusplus)
    return self->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);
#else
    return self->lpVtbl->ResizeBuffers(self, BufferCount, Width, Height, NewFormat, SwapChainFlags);
#endif
}

static inline HRESULT _sapp_dxgi_Present(IDXGISwapChain *self, UINT SyncInterval, UINT Flags)
{
#if defined(__cplusplus)
    return self->Present(SyncInterval, Flags);
#else
    return self->lpVtbl->Present(self, SyncInterval, Flags);
#endif
}

static inline HRESULT _sapp_dxgi_GetFrameStatistics(IDXGISwapChain *self, DXGI_FRAME_STATISTICS *pStats)
{
#if defined(__cplusplus)
    return self->GetFrameStatistics(pStats);
#else
    return self->lpVtbl->GetFrameStatistics(self, pStats);
#endif
}

static inline HRESULT _sapp_dxgi_SetMaximumFrameLatency(IDXGIDevice1 *self, UINT MaxLatency)
{
#if defined(__cplusplus)
    return self->SetMaximumFrameLatency(MaxLatency);
#else
    return self->lpVtbl->SetMaximumFrameLatency(self, MaxLatency);
#endif
}

static inline HRESULT _sapp_dxgi_GetAdapter(IDXGIDevice1 *self, IDXGIAdapter **pAdapter)
{
#if defined(__cplusplus)
    return self->GetAdapter(pAdapter);
#else
    return self->lpVtbl->GetAdapter(self, pAdapter);
#endif
}

static inline HRESULT _sapp_dxgi_GetParent(IDXGIObject *self, REFIID riid, void **ppParent)
{
#if defined(__cplusplus)
    return self->GetParent(riid, ppParent);
#else
    return self->lpVtbl->GetParent(self, riid, ppParent);
#endif
}

static inline HRESULT _sapp_dxgi_MakeWindowAssociation(IDXGIFactory *self, HWND WindowHandle, UINT Flags)
{
#if defined(__cplusplus)
    return self->MakeWindowAssociation(WindowHandle, Flags);
#else
    return self->lpVtbl->MakeWindowAssociation(self, WindowHandle, Flags);
#endif
}

static void _sapp_d3d11_create_device_and_swapchain(void)
{
    // FIXNE: this assumes display 0, which is also assumed in void StartupGraphics(void)
    SDL_DisplayMode info;
    SDL_GetDesktopDisplayMode(0, &info);

    DXGI_SWAP_CHAIN_DESC *sc_desc               = &_sapp.d3d11.swap_chain_desc;
    sc_desc->BufferDesc.Width                   = (UINT)_sapp.framebuffer_width;
    sc_desc->BufferDesc.Height                  = (UINT)_sapp.framebuffer_height;
    sc_desc->BufferDesc.Format                  = DXGI_FORMAT_B8G8R8A8_UNORM;
    sc_desc->BufferDesc.RefreshRate.Numerator   = info.refresh_rate ? info.refresh_rate : 60;
    sc_desc->BufferDesc.RefreshRate.Denominator = 1;
    sc_desc->OutputWindow                       = _sapp.win32.hwnd;
    sc_desc->Windowed                           = true;
    if (_sapp.win32.is_win10_or_greater)
    {
        sc_desc->BufferCount             = 2;
        sc_desc->SwapEffect              = (DXGI_SWAP_EFFECT)_SAPP_DXGI_SWAP_EFFECT_FLIP_DISCARD;
        _sapp.d3d11.use_dxgi_frame_stats = true;
    }
    else
    {
        sc_desc->BufferCount             = 1;
        sc_desc->SwapEffect              = DXGI_SWAP_EFFECT_DISCARD;
        _sapp.d3d11.use_dxgi_frame_stats = false;
    }
    sc_desc->SampleDesc.Count   = 1;
    sc_desc->SampleDesc.Quality = 0;
    sc_desc->BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    UINT create_flags           = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(SOKOL_DEBUG)
    create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL feature_level;
    HRESULT           hr = D3D11CreateDeviceAndSwapChain(NULL,                         /* pAdapter (use default) */
                                                         D3D_DRIVER_TYPE_HARDWARE,     /* DriverType */
                                                         NULL,                         /* Software */
                                                         create_flags,                 /* Flags */
                                                         NULL,                         /* pFeatureLevels */
                                                         0,                            /* FeatureLevels */
                                                         D3D11_SDK_VERSION,            /* SDKVersion */
                                                         sc_desc,                      /* pSwapChainDesc */
                                                         &_sapp.d3d11.swap_chain,      /* ppSwapChain */
                                                         &_sapp.d3d11.device,          /* ppDevice */
                                                         &feature_level,               /* pFeatureLevel */
                                                         &_sapp.d3d11.device_context); /* ppImmediateContext */
    _SOKOL_UNUSED(hr);
#if defined(SOKOL_DEBUG)
    if (!SUCCEEDED(hr))
    {
        // if initialization with D3D11_CREATE_DEVICE_DEBUG fails, this could be because the
        // 'D3D11 debug layer' stopped working, indicated by the error message:
        // ===
        // D3D11CreateDevice: Flags (0x2) were specified which require the D3D11 SDK Layers for Windows 10, but they are
        // not present on the system. These flags must be removed, or the Windows 10 SDK must be installed. Flags
        // include: D3D11_CREATE_DEVICE_DEBUG
        // ===
        //
        // ...just retry with the DEBUG flag switched off
        FatalError(WIN32_D3D11_CREATE_DEVICE_AND_SWAPCHAIN_WITH_DEBUG_FAILED);
        create_flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDeviceAndSwapChain(NULL,                         /* pAdapter (use default) */
                                           D3D_DRIVER_TYPE_HARDWARE,     /* DriverType */
                                           NULL,                         /* Software */
                                           create_flags,                 /* Flags */
                                           NULL,                         /* pFeatureLevels */
                                           0,                            /* FeatureLevels */
                                           D3D11_SDK_VERSION,            /* SDKVersion */
                                           sc_desc,                      /* pSwapChainDesc */
                                           &_sapp.d3d11.swap_chain,      /* ppSwapChain */
                                           &_sapp.d3d11.device,          /* ppDevice */
                                           &feature_level,               /* pFeatureLevel */
                                           &_sapp.d3d11.device_context); /* ppImmediateContext */
    }
#endif
    SOKOL_ASSERT(SUCCEEDED(hr) && _sapp.d3d11.swap_chain && _sapp.d3d11.device && _sapp.d3d11.device_context);

    // minimize frame latency, disable Alt-Enter
    hr = _sapp_d3d11_QueryInterface(_sapp.d3d11.device, _sapp_win32_refiid(_sapp_IID_IDXGIDevice1),
                                    (void **)&_sapp.d3d11.dxgi_device);
    if (SUCCEEDED(hr) && _sapp.d3d11.dxgi_device)
    {
        _sapp_dxgi_SetMaximumFrameLatency(_sapp.d3d11.dxgi_device, 1);
        IDXGIAdapter *dxgi_adapter = 0;
        hr                         = _sapp_dxgi_GetAdapter(_sapp.d3d11.dxgi_device, &dxgi_adapter);
        if (SUCCEEDED(hr) && dxgi_adapter)
        {
            IDXGIFactory *dxgi_factory = 0;
            hr = _sapp_dxgi_GetParent((IDXGIObject *)dxgi_adapter, _sapp_win32_refiid(_sapp_IID_IDXGIFactory),
                                      (void **)&dxgi_factory);
            if (SUCCEEDED(hr))
            {
                _sapp_dxgi_MakeWindowAssociation(dxgi_factory, _sapp.win32.hwnd,
                                                 DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_PRINT_SCREEN);
                _SAPP_SAFE_RELEASE(dxgi_factory);
            }
            else
            {
                FatalError("WIN32_D3D11_GET_IDXGIFACTORY_FAILED");
            }
            _SAPP_SAFE_RELEASE(dxgi_adapter);
        }
        else
        {
            FatalError("WIN32_D3D11_GET_IDXGIADAPTER_FAILED");
        }
    }
    else
    {
        FatalError("WIN32_D3D11_QUERY_INTERFACE_IDXGIDEVICE1_FAILED");
    }
}

static void _sapp_d3d11_create_default_render_target(void)
{
    SOKOL_ASSERT(0 == _sapp.d3d11.rt);
    SOKOL_ASSERT(0 == _sapp.d3d11.rtv);
    SOKOL_ASSERT(0 == _sapp.d3d11.msaa_rt);
    SOKOL_ASSERT(0 == _sapp.d3d11.msaa_rtv);
    SOKOL_ASSERT(0 == _sapp.d3d11.ds);
    SOKOL_ASSERT(0 == _sapp.d3d11.dsv);

    HRESULT hr;

    /* view for the swapchain-created framebuffer */
    hr = _sapp_dxgi_GetBuffer(_sapp.d3d11.swap_chain, 0, _sapp_win32_refiid(_sapp_IID_ID3D11Texture2D),
                              (void **)&_sapp.d3d11.rt);
    SOKOL_ASSERT(SUCCEEDED(hr) && _sapp.d3d11.rt);
    hr = _sapp_d3d11_CreateRenderTargetView(_sapp.d3d11.device, (ID3D11Resource *)_sapp.d3d11.rt, NULL,
                                            &_sapp.d3d11.rtv);
    SOKOL_ASSERT(SUCCEEDED(hr) && _sapp.d3d11.rtv);

    /* common desc for MSAA and depth-stencil texture */
    D3D11_TEXTURE2D_DESC tex_desc;
    SOKOL_CLEAR_MEMORY(&tex_desc, D3D11_TEXTURE2D_DESC, 1);
    tex_desc.Width              = (UINT)_sapp.framebuffer_width;
    tex_desc.Height             = (UINT)_sapp.framebuffer_height;
    tex_desc.MipLevels          = 1;
    tex_desc.ArraySize          = 1;
    tex_desc.Usage              = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags          = D3D11_BIND_RENDER_TARGET;
    tex_desc.SampleDesc.Count   = (UINT)_sapp.sample_count;
    tex_desc.SampleDesc.Quality = (UINT)(_sapp.sample_count > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0);

    /* create MSAA texture and view if antialiasing requested */
    if (_sapp.sample_count > 1)
    {
        tex_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        hr              = _sapp_d3d11_CreateTexture2D(_sapp.d3d11.device, &tex_desc, NULL, &_sapp.d3d11.msaa_rt);
        SOKOL_ASSERT(SUCCEEDED(hr) && _sapp.d3d11.msaa_rt);
        hr = _sapp_d3d11_CreateRenderTargetView(_sapp.d3d11.device, (ID3D11Resource *)_sapp.d3d11.msaa_rt, NULL,
                                                &_sapp.d3d11.msaa_rtv);
        SOKOL_ASSERT(SUCCEEDED(hr) && _sapp.d3d11.msaa_rtv);
    }

    /* texture and view for the depth-stencil-surface */
    tex_desc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
    tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr                 = _sapp_d3d11_CreateTexture2D(_sapp.d3d11.device, &tex_desc, NULL, &_sapp.d3d11.ds);
    SOKOL_ASSERT(SUCCEEDED(hr) && _sapp.d3d11.ds);
    hr = _sapp_d3d11_CreateDepthStencilView(_sapp.d3d11.device, (ID3D11Resource *)_sapp.d3d11.ds, NULL,
                                            &_sapp.d3d11.dsv);
    SOKOL_ASSERT(SUCCEEDED(hr) && _sapp.d3d11.dsv);
}

static void _sapp_d3d11_destroy_default_render_target(void)
{
    _SAPP_SAFE_RELEASE(_sapp.d3d11.rt);
    _SAPP_SAFE_RELEASE(_sapp.d3d11.rtv);
    _SAPP_SAFE_RELEASE(_sapp.d3d11.msaa_rt);
    _SAPP_SAFE_RELEASE(_sapp.d3d11.msaa_rtv);
    _SAPP_SAFE_RELEASE(_sapp.d3d11.ds);
    _SAPP_SAFE_RELEASE(_sapp.d3d11.dsv);
}

void sapp_d3d11_resize_default_render_target(int32_t width, int32_t height)
{
    if (_sapp.d3d11.swap_chain)
    {
        if (_sapp.framebuffer_width == width && _sapp.framebuffer_height == height)
        {
            return;
        }

        _sapp.framebuffer_width  = width;
        _sapp.framebuffer_height = height;

        _sapp_d3d11_destroy_default_render_target();
        _sapp_dxgi_ResizeBuffers(_sapp.d3d11.swap_chain, _sapp.d3d11.swap_chain_desc.BufferCount,
                                 (UINT)_sapp.framebuffer_width, (UINT)_sapp.framebuffer_height,
                                 DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        _sapp_d3d11_create_default_render_target();
    }
}

void sapp_d3d11_present(bool do_not_wait, int swap_interval)
{
    UINT flags = 0;
    if (_sapp.win32.is_win10_or_greater && do_not_wait)
    {
        /* this hack/workaround somewhat improves window-movement and -sizing
            responsiveness when rendering is controlled via WM_TIMER during window
            move and resize on NVIDIA cards on Win10 with recent drivers.
        */
        flags = DXGI_PRESENT_DO_NOT_WAIT;
    }
    _sapp_dxgi_Present(_sapp.d3d11.swap_chain, (UINT)swap_interval, flags);
}

void sapp_d3d11_init(SDL_Window *window, int32_t width, int32_t height)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    _sapp.win32.hwnd                = wmInfo.info.win.window;
    _sapp.framebuffer_width         = width;
    _sapp.framebuffer_height        = height;
    _sapp.sample_count              = 1;
    _sapp.win32.is_win10_or_greater = _sapp_win32_is_win10_or_greater();

    _sapp_d3d11_create_device_and_swapchain();
    _sapp_d3d11_create_default_render_target();
}

const void *sapp_d3d11_get_device(void)
{
    return _sapp.d3d11.device;
}

const void *sapp_d3d11_get_device_context(void)
{
    return _sapp.d3d11.device_context;
}

const void *sapp_d3d11_get_swap_chain(void)
{
    return _sapp.d3d11.swap_chain;
}

const void *sapp_d3d11_get_render_view(void)
{
    if (_sapp.sample_count > 1)
    {
        SOKOL_ASSERT(_sapp.d3d11.msaa_rtv);
        return _sapp.d3d11.msaa_rtv;
    }
    else
    {
        SOKOL_ASSERT(_sapp.d3d11.rtv);
        return _sapp.d3d11.rtv;
    }
}

const void *sapp_d3d11_get_resolve_view(void)
{
    if (_sapp.sample_count > 1)
    {
        SOKOL_ASSERT(_sapp.d3d11.rtv);
        return _sapp.d3d11.rtv;
    }
    else
    {
        return 0;
    }
}

const void *sapp_d3d11_get_depth_stencil_view(void)
{
    return _sapp.d3d11.dsv;
}

void sapp_d3d11_destroy_device_and_swapchain(void)
{
    _SAPP_SAFE_RELEASE(_sapp.d3d11.swap_chain);
    _SAPP_SAFE_RELEASE(_sapp.d3d11.dxgi_device);
    _SAPP_SAFE_RELEASE(_sapp.d3d11.device_context);
    _SAPP_SAFE_RELEASE(_sapp.d3d11.device);
}

// D3D11CalcSubresource only exists with helpers enabled
static UINT sapp_d3d11_calcsubresource(UINT mip_slice, UINT array_slice, UINT mip_levels)
{
    return mip_slice + array_slice * mip_levels;
}

void sapp_d3d11_capture_screen(int32_t width, int32_t height, int32_t stride, uint8_t *dest)
{
    ID3D11Texture2D *pSurface;
    HRESULT hr = _sapp.d3d11.swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&pSurface));
    SOKOL_ASSERT(SUCCEEDED(hr) && pSurface);
    if (pSurface)
    {
        // const int    width  = static_cast<int>(m_window->Bounds.Width * m_dpi / 96.0f);
        // const int    height = static_cast<int>(m_window->Bounds.Height * m_dpi / 96.0f);

        ID3D11Texture2D *pNewTexture = NULL;

        D3D11_TEXTURE2D_DESC description;
        pSurface->GetDesc(&description);
        description.BindFlags      = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        description.Usage          = D3D11_USAGE_STAGING;
        uint32_t subresource       = sapp_d3d11_calcsubresource(0, 0, 0);

        hr = _sapp.d3d11.device->CreateTexture2D(&description, NULL, &pNewTexture);
        SOKOL_ASSERT(SUCCEEDED(hr));
        if (pNewTexture)
        {
            _sapp.d3d11.device_context->CopyResource(pNewTexture, pSurface);
            D3D11_MAPPED_SUBRESOURCE resource;
            hr = _sapp.d3d11.device_context->Map(pNewTexture, subresource, D3D11_MAP_READ_WRITE, 0, &resource);
            SOKOL_ASSERT(SUCCEEDED(hr));

            const uint8_t *source = static_cast<const uint8_t *>(resource.pData);
            for (int i = 0; i < height; ++i)
            {
                uint8_t *d = dest;
                for (int j = 0; j < width; j++)
                {
                    // swizzle
                    const uint8_t *src = &source[(height - i - 1) * resource.RowPitch + (j * 4)];
                    d[0]               = src[2];
                    d[1]               = src[1];
                    d[2]               = src[0];
                    d[3]               = src[3];
                    d += 4;
                }

                dest += stride;
            }
        }

        _sapp.d3d11.device_context->Unmap(pNewTexture, subresource);

        _SAPP_SAFE_RELEASE(pNewTexture);
    }

    _SAPP_SAFE_RELEASE(pSurface);
}
