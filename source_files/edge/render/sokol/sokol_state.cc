
#include "epi.h"
#include "r_backend.h"
#include "r_state.h"
#include "sokol_images.h"
#include "sokol_local.h"
#include "sokol_pipeline.h"

static constexpr GLuint kGenTextureId       = 0x0000FFFF;
static constexpr GLuint kRenderStateInvalid = 0xFFFFFFFF;

struct MipLevel
{
    GLsizei width_;
    GLsizei height_;
    void   *pixels_;
};

struct TexInfo
{
    GLsizei width_;
    GLsizei height_;
    int64_t update_frame_;
};

class SokolRenderState : public RenderState
{
  public:
    void Enable(GLenum cap, bool enabled = true)
    {
        PassInfo pass_info;

        switch (cap)
        {
        case GL_TEXTURE_2D:
            break;
        case GL_FOG:
            enable_fog_ = enabled;
            break;
        case GL_ALPHA_TEST:
            alpha_test_ = enabled ? 0.1f : 0.0f;
            break;
        case GL_BLEND:
            break;
        case GL_CULL_FACE:
            break;
        case GL_SCISSOR_TEST:
            if (!enabled)
            {
                render_backend->GetPassInfo(pass_info);
                Scissor(0, 0, pass_info.width_, pass_info.height_);
            }
            break;
        case GL_LIGHTING:
            break;
        case GL_COLOR_MATERIAL:
            break;
        case GL_DEPTH_TEST:
            break;
        case GL_STENCIL_TEST:
            break;
        case GL_LINE_SMOOTH:
            break;
        case GL_NORMALIZE:
            break;
        case GL_POLYGON_SMOOTH:
            break;

        case GL_CLIP_PLANE0:
        case GL_CLIP_PLANE1:
        case GL_CLIP_PLANE2:
        case GL_CLIP_PLANE3:
        case GL_CLIP_PLANE4:
        case GL_CLIP_PLANE5:
            sgl_set_clipplane_enabled(cap - GL_CLIP_PLANE0, enabled);
            break;
        default:
            FatalError("Unknown GL State %i", cap);
        }
    }

    void Disable(GLenum cap)
    {
        PassInfo pass_info;

        switch (cap)
        {
        case GL_TEXTURE_2D:
            break;
        case GL_FOG:
            enable_fog_ = false;
            break;
        case GL_ALPHA_TEST:
            alpha_test_ = 0.0f;
            break;
        case GL_BLEND:
            break;
        case GL_CULL_FACE:
            break;
        case GL_SCISSOR_TEST:
            render_backend->GetPassInfo(pass_info);
            Scissor(0, 0, pass_info.width_, pass_info.height_);
            break;
        case GL_LIGHTING:
            break;
        case GL_COLOR_MATERIAL:
            break;
        case GL_DEPTH_TEST:
            break;
        case GL_STENCIL_TEST:
            break;
        case GL_LINE_SMOOTH:
            break;
        case GL_NORMALIZE:
            break;
        case GL_POLYGON_SMOOTH:
            break;

        case GL_CLIP_PLANE0:
        case GL_CLIP_PLANE1:
        case GL_CLIP_PLANE2:
        case GL_CLIP_PLANE3:
        case GL_CLIP_PLANE4:
        case GL_CLIP_PLANE5:
            sgl_set_clipplane_enabled(cap - GL_CLIP_PLANE0, false);
            break;
        default:
            FatalError("Unknown GL State %i", cap);
        }
    }

    void DepthMask(bool enable)
    {
        depth_mask_ = enable;
    }

    void DepthFunction(GLenum func)
    {
        depth_function_ = func;
    }

    void CullFace(GLenum mode)
    {
        EPI_UNUSED(mode);
    }

    void AlphaFunction(GLenum func, GLfloat ref)
    {
        EPI_UNUSED(func);
        EPI_UNUSED(ref);
    }

    void ActiveTexture(GLenum activeTexture)
    {
        EPI_UNUSED(activeTexture);
    }

    void Scissor(GLint x, GLint y, GLsizei width, GLsizei height)
    {
        // can't currently disable
        sgl_scissor_rect(x, y, width, height, true);
    }

    void PolygonOffset(GLfloat factor, GLfloat units)
    {
        EPI_UNUSED(factor);
        EPI_UNUSED(units);
    }

    void Clear(GLbitfield mask)
    {
        EPI_UNUSED(mask);
    }

    void ClearColor(RGBAColor color)
    {
        EPI_UNUSED(color);
    }

    void FogMode(GLint fogMode)
    {
        fog_mode_ = fogMode;
    }

    void FogColor(RGBAColor color)
    {
        fog_color_ = color;
    }

    void FogStart(GLfloat start)
    {
        fog_start_ = start;
    }

    void FogEnd(GLfloat end)
    {
        fog_end_ = end;
    }

    void FogDensity(GLfloat density)
    {
        fog_density_ = density;
    }

    void GLColor(RGBAColor color)
    {
        EPI_UNUSED(color);
    }

    void BlendFunction(GLenum sfactor, GLenum dfactor)
    {
        EPI_UNUSED(sfactor);
        EPI_UNUSED(dfactor);
    }

    void TextureEnvironmentMode(GLint param)
    {
        EPI_UNUSED(param);
    }

    void TextureEnvironmentCombineRGB(GLint param)
    {
        EPI_UNUSED(param);
    }

    void TextureEnvironmentSource0RGB(GLint param)
    {
        EPI_UNUSED(param);
    }

    void MultiTexCoord(GLuint tex, const HMM_Vec2 *coords)
    {
        EPI_UNUSED(tex);
        EPI_UNUSED(coords);
    }

    void Hint(GLenum target, GLenum mode)
    {
        EPI_UNUSED(target);
        EPI_UNUSED(mode);
    }

    void LineWidth(float width)
    {
        EPI_UNUSED(width);
    }

    void DeleteTexture(const GLuint *tex_id)
    {
        sg_image img;
        img.id = *tex_id;

        auto itr = tex_infos_.find(img.id);
        if (itr != tex_infos_.end())
        {
            delete itr->second;
            tex_infos_.erase(itr);
        }

        DeleteImage(img);
    }

    void FrontFace(GLenum wind)
    {
        EPI_UNUSED(wind);
    }

    void ShadeModel(GLenum model)
    {
        EPI_UNUSED(model);
    }

    void ColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
    {
        EPI_UNUSED(red);
        EPI_UNUSED(green);
        EPI_UNUSED(blue);
        EPI_UNUSED(alpha);
    }

    void BindTexture(GLuint textureid)
    {
        if (generating_texture_)
        {
            if (textureid != kGenTextureId)
            {
                FatalError("Cannot bind to another texture during texture creation");
            }
        }

        texture_bound_ = textureid ? textureid : kRenderStateInvalid;
    }

    void GenTextures(GLsizei n, GLuint *textures)
    {
        EPI_UNUSED(n);
        generating_texture_ = true;
        texture_level_      = 0;

        texture_wrap_s_ = GL_CLAMP;
        texture_wrap_t_ = GL_CLAMP;

        *textures = kGenTextureId;
    }

    void TextureMinFilter(GLint param)
    {
        texture_min_filter_ = param;
    }

    void TextureMagFilter(GLint param)
    {
        texture_mag_filter_ = param;
    }

    void TextureWrapS(GLint param)
    {
        texture_wrap_s_ = param;
    }

    void TextureWrapT(GLint param)
    {
        texture_wrap_t_ = param;
    }

    void FinishTextures(GLsizei n, GLuint *textures)
    {
        EPI_UNUSED(n);
        if (!mip_levels_.size())
        {
            FatalError("FinishTextures: No mip levels defined");
        }

        uint8_t bpp = 0;
        switch (texture_format_)
        {
        case SG_PIXELFORMAT_RGBA8:
            bpp = 4;
            break;
        case SG_PIXELFORMAT_R8:
            bpp = 1;
            break;
        default:
            bpp = 0;
            break;
        }

        if (!bpp)
        {
            FatalError("Unknown texture format");
        }

        sg_image_data img_data;
        EPI_CLEAR_MEMORY(&img_data, sg_image_data, 1);

        sg_image_desc img_desc;
        EPI_CLEAR_MEMORY(&img_desc, sg_image_desc, 1);
        img_desc.usage         = texture_usage_;
        img_desc.width         = mip_levels_[0].width_;
        img_desc.height        = mip_levels_[0].height_;
        img_desc.pixel_format  = texture_format_;
        img_desc.num_mipmaps   = (int)mip_levels_.size();

        if (texture_usage_ != SG_USAGE_DYNAMIC)
        {
            int mip = 0;
            for (auto itr = mip_levels_.begin(); itr != mip_levels_.end(); itr++, mip++)
            {
                sg_range range;
                range.ptr                 = itr->pixels_;
                range.size                = itr->width_ * itr->height_ * bpp;
                img_data.subimage[0][mip] = range;
            }

            img_desc.data = img_data;
        }

        sg_image image = sg_make_image(&img_desc);
        *textures      = image.id;

        sg_sampler_desc sampler_desc;
        EPI_CLEAR_MEMORY(&sampler_desc, sg_sampler_desc, 1);

        sampler_desc.wrap_u = SG_WRAP_REPEAT;
        sampler_desc.wrap_v = SG_WRAP_REPEAT;

        if (texture_wrap_s_ == GL_CLAMP || texture_wrap_s_ == GL_CLAMP_TO_EDGE)
        {
            sampler_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        }
        if (texture_wrap_t_ == GL_CLAMP || texture_wrap_t_ == GL_CLAMP_TO_EDGE)
        {
            sampler_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        }

        // filtering
        sampler_desc.mag_filter    = SG_FILTER_NEAREST;
        sampler_desc.min_filter    = SG_FILTER_NEAREST;
        sampler_desc.mipmap_filter = SG_FILTER_NEAREST;

        switch (texture_min_filter_)
        {
        case GL_NEAREST:
        case GL_NEAREST_MIPMAP_NEAREST:
            sampler_desc.min_filter = SG_FILTER_NEAREST;
            break;
        case GL_LINEAR:
        case GL_NEAREST_MIPMAP_LINEAR:
            sampler_desc.min_filter = SG_FILTER_LINEAR;
            break;
        }

        switch (texture_mag_filter_)
        {
        case GL_NEAREST:
        case GL_NEAREST_MIPMAP_NEAREST:
            sampler_desc.mag_filter = SG_FILTER_NEAREST;
            break;
        case GL_LINEAR:
        case GL_NEAREST_MIPMAP_LINEAR:
            sampler_desc.mag_filter = SG_FILTER_LINEAR;
            break;
        }

        RegisterImageSampler(image.id, &sampler_desc);

        if (texture_usage_ != SG_USAGE_DYNAMIC)
        {
            for (auto itr = mip_levels_.begin(); itr != mip_levels_.end(); itr++)
            {
                if (itr->pixels_)
                {
                    free(itr->pixels_);
                }
            }
        }

        TexInfo *info = new TexInfo;

        info->width_         = mip_levels_[0].width_;
        info->height_        = mip_levels_[0].height_;
        info->update_frame_  = 0;
        tex_infos_[image.id] = info;

        mip_levels_.clear();
        generating_texture_ = false;

        texture_level_  = 0;
        texture_format_ = SG_PIXELFORMAT_NONE;
        texture_usage_  = SG_USAGE_IMMUTABLE;
    }

    void TexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
                    GLenum format, GLenum type, const void *pixels, RenderUsage usage = kRenderUsageImmutable)
    {
        EPI_UNUSED(format);
        EPI_UNUSED(type);
        EPI_UNUSED(target);
        EPI_UNUSED(border);

        sg_pixel_format sg_texture_format = SG_PIXELFORMAT_NONE;

        texture_usage_ = usage == kRenderUsageImmutable ? SG_USAGE_IMMUTABLE : SG_USAGE_DYNAMIC;

        uint8_t bpp = 0;
        switch (internalformat)
        {
        case GL_RGB:
            // https://github.com/floooh/sokol/pull/111
            FatalError("GL_RGB is only supported by OpenGL, promote to GL_RGBA before calling TexImage2D");
            break;
        case GL_RGBA:
            sg_texture_format = SG_PIXELFORMAT_RGBA8;
            bpp               = 4;
            break;
        case GL_ALPHA:
            FatalError("GL_ALPHA is only supported by OpenGL, promote to GL_RGBA before calling TexImage2D");
            break;
        default:
            FatalError("Unknown texture format");
        }

        // Texture Generation

        if (generating_texture_)
        {
            if (texture_level_ > level)
            {
                FatalError("TexImage2D: texture levels must be sequential");
            }

            if (texture_bound_ != kGenTextureId)
            {
                FatalError("TexImage2D: texture_bound_ != kGenTextureId during texture generation");
            }

            texture_format_ = sg_texture_format;

            MipLevel mip_level;
            mip_level.width_  = width;
            mip_level.height_ = height;
            mip_level.pixels_ = nullptr;
            if (pixels)
            {
                mip_level.pixels_ = malloc(width * height * bpp);
                memcpy(mip_level.pixels_, pixels, width * height * bpp);
            }
            mip_levels_.push_back(mip_level);
            return;
        }

        // Texture Update

        if (texture_bound_ == kGenTextureId)
        {
            FatalError("TexImage2D: texture_bound_ == kGenTextureId on update");
        }

        int64_t backend_frame = render_backend->GetFrameNumber();
        auto    itr           = tex_infos_.find(texture_bound_);
        if (itr == tex_infos_.end())
        {
            FatalError("TexImage2D: Attempting to update missing texture");
        }

        if (itr->second->update_frame_ == backend_frame)
        {
            FatalError("TexImage2D: Cannot update a texture twice on the same frame");
        }

        if (itr->second->width_ != width || itr->second->height_ != height)
        {
            FatalError("TexImage2D: Dimension mismatch on texture update");
        }

        itr->second->update_frame_ = backend_frame;

        sg_image_data image_data;
        EPI_CLEAR_MEMORY(&image_data, sg_image_data, 1);
        sg_range      range;
        range.ptr                 = pixels;
        range.size                = width * height * bpp;
        image_data.subimage[0][0] = range;

        sg_image img;
        img.id = texture_bound_;

        sg_update_image(img, &image_data);
    }

    void PixelStorei(GLenum pname, GLint param)
    {
        EPI_UNUSED(pname);
        EPI_UNUSED(param);
    }

    void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)
    {
        EPI_UNUSED(x);
        EPI_UNUSED(y);
        EPI_UNUSED(width);
        EPI_UNUSED(height);
        EPI_UNUSED(format);
        EPI_UNUSED(type);
        EPI_UNUSED(pixels);
    }

    void PixelZoom(GLfloat xfactor, GLfloat yfactor)
    {
        EPI_UNUSED(xfactor);
        EPI_UNUSED(yfactor);
    }

    void Flush()
    {
    }

    void ClipPlane(GLenum plane, GLdouble *equation)
    {
        sgl_set_clipplane(int(plane) - int(GL_CLIP_PLANE0), float(equation[0]), float(equation[1]), float(equation[2]),
                          float(equation[3]));
    }

    void SetPipeline(uint32_t flags)
    {
        uint32_t pipeline_flags = 0;
        if (depth_mask_)
            pipeline_flags |= kPipelineDepthWrite;
        if (depth_function_ == GL_GREATER)
            pipeline_flags |= kPipelineDepthGreater;

        pipeline_flags |= flags;

        sgl_context context = sgl_get_context();
        sgl_load_pipeline(GetPipeline(context, pipeline_flags));

        float fogr = float(epi::GetRGBARed(fog_color_)) / 255.0f;
        float fogg = float(epi::GetRGBAGreen(fog_color_)) / 255.0f;
        float fogb = float(epi::GetRGBABlue(fog_color_)) / 255.0f;

        sgl_set_fog(enable_fog_, fogr, fogg, fogb, 1, fog_density_, fog_start_, fog_end_, 1);

        sgl_set_alpha_test(alpha_test_);
    }

    // state
    GLenum depth_function_;
    bool   depth_mask_;

    bool      enable_fog_;
    GLint     fog_mode_;
    GLfloat   fog_start_;
    GLfloat   fog_end_;
    GLfloat   fog_density_;
    RGBAColor fog_color_;

    GLfloat alpha_test_;

    // texture creation
    bool                                    generating_texture_ = false;
    GLint                                   texture_level_;
    sg_pixel_format                         texture_format_ = SG_PIXELFORMAT_NONE;
    sg_usage                                texture_usage_  = SG_USAGE_IMMUTABLE;
    std::vector<MipLevel>                   mip_levels_;
    std::unordered_map<uint32_t, TexInfo *> tex_infos_;

    GLuint texture_bound_ = kRenderStateInvalid;

    GLint texture_min_filter_;
    GLint texture_mag_filter_;
    GLint texture_wrap_s_;
    GLint texture_wrap_t_;
};

static SokolRenderState state;
RenderState            *render_state = &state;
