
#include "r_state.h"

class GLRenderState : public RenderState
{
  public:
    void Enable(GLenum cap, bool enabled = true)
    {
        switch (cap)
        {
        case GL_TEXTURE_2D:
            if (enable_texture_2d_[active_texture_ - GL_TEXTURE0] == enabled)
                return;
            enable_texture_2d_[active_texture_ - GL_TEXTURE0] = enabled;
            break;
        case GL_FOG:
            if (enable_fog_ == enabled)
                return;
            enable_fog_ = enabled;
            break;
        case GL_ALPHA_TEST:
            if (enable_alpha_test_ == enabled)
                return;
            enable_alpha_test_ = enabled;
            break;
        case GL_BLEND:
            if (enable_blend_ == enabled)
                return;
            enable_blend_ = enabled;
            break;
        case GL_CULL_FACE:
            if (enable_cull_face_ == enabled)
                return;
            enable_cull_face_ = enabled;
            break;
        case GL_SCISSOR_TEST:
            if (enable_scissor_test_ == enabled)
                return;
            enable_scissor_test_ = enabled;
            break;
        case GL_LIGHTING:
            if (enable_lighting_ == enabled)
                return;
            enable_lighting_ = enabled;
            break;
        case GL_COLOR_MATERIAL:
            if (enable_color_material_ == enabled)
                return;
            enable_color_material_ = enabled;
            break;
        case GL_DEPTH_TEST:
            if (enable_depth_test_ == enabled)
                return;
            enable_depth_test_ = enabled;
            break;
        case GL_STENCIL_TEST:
            if (enable_stencil_test_ == enabled)
                return;
            enable_stencil_test_ = enabled;
            break;
        case GL_LINE_SMOOTH:
            if (enable_line_smooth_ == enabled)
                return;
            enable_line_smooth_ = enabled;
            break;
        case GL_NORMALIZE:
            if (enable_normalize_ == enabled)
                return;
            enable_normalize_ = enabled;
            break;
        case GL_CLIP_PLANE0:
        case GL_CLIP_PLANE1:
        case GL_CLIP_PLANE2:
        case GL_CLIP_PLANE3:
        case GL_CLIP_PLANE4:
        case GL_CLIP_PLANE5:
            if (enable_clip_plane_[cap - GL_CLIP_PLANE0] == enabled)
                return;
            enable_clip_plane_[cap - GL_CLIP_PLANE0] = enabled;
            break;
        case GL_POLYGON_SMOOTH:
            if (enable_polygon_smooth_ == enabled)
                return;
            enable_polygon_smooth_ = enabled;
            break;
        default:
            FatalError("Unknown GL State %i", cap);
        }

        if (enabled)
        {
            glEnable(cap);
        }
        else
        {
            glDisable(cap);
        }
    }

    void Disable(GLenum cap)
    {
        Enable(cap, false);
    }

    void DepthMask(bool enable)
    {
        if (depth_mask_ == enable)
        {
            return;
        }

        depth_mask_ = enable;
        glDepthMask(enable ? GL_TRUE : GL_FALSE);
    }

    void DepthFunction(GLenum func)
    {
        if (func == depth_function_)
        {
            return;
        }

        depth_function_ = func;

        glDepthFunc(depth_function_);
    }

    void CullFace(GLenum mode)
    {
        if (cull_face_ == mode)
        {
            return;
        }

        cull_face_ = mode;
        glCullFace(mode);
    }

    void AlphaFunction(GLenum func, GLfloat ref)
    {
        if (func == alpha_function_ && AlmostEquals(ref, alpha_function_reference_))
        {
            return;
        }

        alpha_function_           = func;
        alpha_function_reference_ = ref;

        glAlphaFunc(alpha_function_, alpha_function_reference_);
    }

    void ActiveTexture(GLenum activeTexture)
    {
        if (activeTexture == active_texture_)
        {
            return;
        }

        active_texture_ = activeTexture;
        glActiveTexture(active_texture_);        
    }

    void BindTexture(GLuint textureid)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;
        if (bind_texture_2d_[index] == textureid)
        {
            return;
        }

        bind_texture_2d_[index] = textureid;
        glBindTexture(GL_TEXTURE_2D, textureid);        
    }

    void Scissor(GLint x, GLint y, GLsizei width, GLsizei height)
    {
        glScissor(x, y, width, height);
    }

    void PolygonOffset(GLfloat factor, GLfloat units)
    {
        if (factor == polygon_offset_factor_ && units == polygon_offset_units_)
        {
            return;
        }

        polygon_offset_factor_ = factor;
        polygon_offset_units_  = units;
        glPolygonOffset(polygon_offset_factor_, polygon_offset_units_);        
    }

    void Clear(GLbitfield mask)
    {
        if (mask & GL_DEPTH_BUFFER_BIT)
        {
            DepthMask(true);
        }
        glClear(mask);
    }

    void ClearColor(RGBAColor color)
    {
        if (color == clear_color_)
        {
            return;
        }

        clear_color_ = color;
        glClearColor(epi::GetRGBARed(clear_color_) / 255.0f, epi::GetRGBAGreen(clear_color_) / 255.0f,
                     epi::GetRGBABlue(clear_color_) / 255.0f, epi::GetRGBAAlpha(clear_color_) / 255.0f);        
    }

    void FogMode(GLint fogMode)
    {
        if (fog_mode_ == fogMode)
        {
            return;
        }

        fog_mode_ = fogMode;
        glFogi(GL_FOG_MODE, fog_mode_);        
    }

    void FogColor(RGBAColor color)
    {
        if (fog_color_ == color)
        {
            return;
        }

        fog_color_ = color;

        float gl_fc[4] = {epi::GetRGBARed(fog_color_) / 255.0f, epi::GetRGBAGreen(fog_color_) / 255.0f,
                          epi::GetRGBABlue(fog_color_) / 255.0f, epi::GetRGBAAlpha(fog_color_) / 255.0f};
        glFogfv(GL_FOG_COLOR, gl_fc);        
    }

    void FogStart(GLfloat start)
    {
        if (fog_start_ == start)
        {
            return;
        }

        fog_start_ = start;
        glFogf(GL_FOG_START, fog_start_);        
    }

    void FogEnd(GLfloat end)
    {
        if (fog_end_ == end)
        {
            return;
        }

        fog_end_ = end;
        glFogf(GL_FOG_END, fog_end_);        
    }

    void FogDensity(GLfloat density)
    {
        if (fog_density_ == density)
        {
            return;
        }

        fog_density_ = density;
        glFogf(GL_FOG_DENSITY, fog_density_);        
    }

    void GLColor(RGBAColor color)
    {
        if (color == gl_color_)
        {
            return;
        }

        gl_color_ = color;
        glColor4ub(epi::GetRGBARed(color), epi::GetRGBAGreen(color), epi::GetRGBABlue(color), epi::GetRGBAAlpha(color));
    }

    void BlendFunction(GLenum sfactor, GLenum dfactor)
    {
        if (blend_source_factor_ == sfactor && blend_destination_factor_ == dfactor)
        {
            return;
        }

        blend_source_factor_      = sfactor;
        blend_destination_factor_ = dfactor;
        glBlendFunc(blend_source_factor_, blend_destination_factor_);
    }

    void TextureEnvironmentMode(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        if (texture_environment_mode_[index] == param)
        {
            return;
        }

        texture_environment_mode_[index] = param;
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texture_environment_mode_[index]);        
    }

    void TextureEnvironmentCombineRGB(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        if (texture_environment_combine_rgb_[index] == param)
        {
            return;
        }

        texture_environment_combine_rgb_[index] = param;
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, texture_environment_combine_rgb_[index]);
    }

    void TextureEnvironmentSource0RGB(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        if (texture_environment_source_0_rgb_[index] == param)
        {
            return;
        }

        texture_environment_source_0_rgb_[index] = param;
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, texture_environment_source_0_rgb_[index]);
    }

    void TextureMinFilter(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        texture_min_filter_[index] = param;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_min_filter_[index]);
    }

    void TextureMagFilter(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        texture_mag_filter_[index] = param;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_mag_filter_[index]);
    }

    void TextureWrapS(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        // We do it regardless of the cached value; functions should check
        // texture environments against the appropriate unordered_map and
        // know if a change needs to occur
        texture_wrap_s_[index] = param;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture_wrap_s_[index]);
    }

    void TextureWrapT(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        // We do it regardless of the cached value; functions should check
        // texture environments against the appropriate unordered_map and
        // know if a change needs to occur
        texture_wrap_t_[index] = param;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture_wrap_t_[index]);
    }

    void MultiTexCoord(GLuint tex, const HMM_Vec2 *coords)
    {
        if (enable_texture_2d_[tex - GL_TEXTURE0] == false)
            return;
        if (tex == GL_TEXTURE0 && enable_texture_2d_[1] == false)
            glTexCoord2fv((GLfloat *)coords);
        else
            glMultiTexCoord2fv(tex, (GLfloat *)coords);
    }

    void Hint(GLenum target, GLenum mode)
    {
        glHint(target, mode);
    }

    void LineWidth(float width)
    {
        if (AlmostEquals(width, line_width_))
        {
            return;
        }
        line_width_ = width;
        glLineWidth(line_width_);
    }

    float GetLineWidth()
    {
        return line_width_;
    }

    void DeleteTexture(const GLuint *tex_id)
    {
        if (tex_id && *tex_id > 0)
        {
            texture_clamp_s.erase(*tex_id);
            texture_clamp_t.erase(*tex_id);
            glDeleteTextures(1, tex_id);
            // We don't need to actually perform a texture bind,
            // but these should be cleared out to ensure
            // we aren't mistakenly using a tex_id that does not
            // correlate to the same texture anymore
            bind_texture_2d_[0] = 0;
            bind_texture_2d_[1] = 0;
        }
    }

    void FrontFace(GLenum wind)
    {
        if (front_face_ == wind)
        {
            return;
        }

        front_face_ = wind;
        glFrontFace(wind);
    }

    void ShadeModel(GLenum model)
    {
        if (shade_model_ == model)
        {
            return;
        }

        shade_model_ = model;
        glShadeModel(model);
    }

    void ColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
    {
        glColorMask(red, green, blue, alpha);
    }

    void GenTextures(GLsizei n, GLuint *textures)
    {
        glGenTextures(n, textures);
    }

    void TexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
                    GLenum format, GLenum type, const void *pixels, RenderUsage usage = kRenderUsageImmutable)
    {
        EPI_UNUSED(usage);
        glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    }

    void PixelStorei(GLenum pname, GLint param)
    {
        glPixelStorei(pname, param);
    }

    void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)
    {
        glReadPixels(x, y, width, height, format, type, pixels);
    }

    void PixelZoom(GLfloat xfactor, GLfloat yfactor)
    {
        glPixelZoom(xfactor, yfactor);
    }

    void Flush()
    {
        glFlush();
    }

    void ClipPlane(GLenum plane, GLdouble *equation)
    {
        glClipPlane(plane, equation);
    }

    void FinishTextures(GLsizei n, GLuint *textures)
    {
        EPI_UNUSED(n);
        EPI_UNUSED(textures);
    }

    void SetPipeline(uint32_t flags)
    {
        EPI_UNUSED(flags);
    }
    
    void OnContextSwitch()
    {

    }

    void Reset()
    {

    }

    // Might need to add more here since the state's scope has expanded - Dasho
    void ResetGLState()
    {
        Disable(GL_BLEND);
        BlendFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        Disable(GL_ALPHA_TEST);

        DepthMask(true);

        CullFace(GL_BACK);
        Disable(GL_CULL_FACE);

        Disable(GL_FOG);

        PolygonOffset(0, 0);

        for (int i = 0; i < 2; i++)
        {
            bind_texture_2d_[i]                  = 0;
            texture_environment_mode_[i]         = 0;
            texture_environment_combine_rgb_[i]  = 0;
            texture_environment_source_0_rgb_[i] = 0;
            texture_wrap_t_[i]                   = 0;
        }
    }

    int frameStateChanges_ = 0;

  private:
    bool   enable_blend_;
    GLenum blend_source_factor_;
    GLenum blend_destination_factor_;

    bool   enable_cull_face_;
    GLenum cull_face_;

    GLenum front_face_;

    GLenum shade_model_;

    bool enable_scissor_test_;

    bool enable_clip_plane_[6];

    RGBAColor clear_color_;

    // texture
    bool enable_texture_2d_[2];

    GLint texture_environment_mode_[2];
    GLint texture_environment_combine_rgb_[2];
    GLint texture_environment_source_0_rgb_[2];
    GLint texture_min_filter_[2];
    GLint texture_mag_filter_[2];
    GLint texture_wrap_s_[2];
    GLint texture_wrap_t_[2];

    GLuint bind_texture_2d_[2];
    GLenum active_texture_ = GL_TEXTURE0;

    bool   enable_depth_test_;
    bool   depth_mask_;
    GLenum depth_function_;

    GLfloat polygon_offset_factor_;
    GLfloat polygon_offset_units_;

    bool    enable_alpha_test_;
    GLenum  alpha_function_;
    GLfloat alpha_function_reference_;

    bool enable_lighting_;

    bool enable_color_material_;

    bool enable_stencil_test_;

    bool  enable_line_smooth_;
    float line_width_;

    bool enable_normalize_;

    bool enable_polygon_smooth_;

    bool      enable_fog_;
    GLint     fog_mode_;
    GLfloat   fog_start_;
    GLfloat   fog_end_;
    GLfloat   fog_density_;
    RGBAColor fog_color_;

    RGBAColor gl_color_;
};

static GLRenderState state;
RenderState         *render_state = &state;
