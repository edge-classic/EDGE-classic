/*
    EDGE-Classic extensions to Sokol GFX

    Dasho: This moves as much as possible out of Sokol GFX itself so that 
    updating sokol_gfx.h is less of a headache. The one thing that must still
    be modified in sokol_gfx.h itself is adding the
    'ID3D11DepthStencilView* depth_stencil_view' member to the 'cur_pass' struct 
    within the _sg_d3d11_backend_t struct definition and assigning the 
    swapchain->d3d11.depth_stencil_view value to it when the d3d11 pass begins            
*/

SOKOL_GFX_API_DECL void sg_d3d11_clear_depth(float value);

SOKOL_GFX_API_DECL void sg_gl_clear_depth(float value);

SOKOL_GFX_API_DECL void sg_gl_read_pixels(int x, int y, int width, int height, int format, int type, void* data);

#ifdef SOKOL_GFX_IMPL

SOKOL_API_IMPL void sg_d3d11_clear_depth(float value) {
    #if defined(SOKOL_D3D11)
        _sg.d3d11.ctx->ClearDepthStencilView(_sg.d3d11.cur_pass.depth_stencil_view, D3D11_CLEAR_DEPTH, value, 0);
    #endif
}

SOKOL_API_IMPL void sg_gl_clear_depth(float value)
{
#if defined(SOKOL_GLCORE) || defined(SOKOL_GLES3)    
    float depth = value;
    glClearBufferfv(GL_DEPTH, 0, &depth);
#endif
}

SOKOL_API_IMPL void sg_gl_read_pixels(int x, int y, int width, int height, int format, int type, void* data)
{
#if defined(SOKOL_GLCORE) || defined(SOKOL_GLES3)  
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glReadPixels(x, y, width, height, format, type, data);
    _SG_GL_CHECK_ERROR();
#endif
}

#endif