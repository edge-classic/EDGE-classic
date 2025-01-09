// https://webgl2fundamentals.org/webgl/lessons/webgl-fog.html
@ctype mat4 HMM_Mat4

@vs vs
// This is required on D3D to address clipspace issues with 3d model rendering using an ortho projection, D3D and GL disagree on clipspace
// https://github.com/floooh/sokol-tools/blob/master/docs/sokol-shdc.md#glsl_options-hlsl_options-msl_options
@hlsl_options fixup_clipspace
layout(binding=0) uniform vs_params {
    mat4 mvp;
    mat4 tm;
    mat4 mv;
    vec4 clipplane0;
    vec4 clipplane1;
    vec4 clipplane2;
    vec4 clipplane3;
    vec4 clipplane4;
    vec4 clipplane5;
};
layout(location = 0) in vec4 position;
layout(location = 1) in vec4 texcoords;
layout(location = 2) in vec4 color0;
layout(location = 3) in float psize;
layout(location = 0) out vec4 uv;
layout(location = 1) out vec4 color;
layout(location = 2) out vec3  vpos;
layout(location = 3) out float clipvertex0;
layout(location = 4) out float clipvertex1;
layout(location = 5) out float clipvertex2;
layout(location = 6) out float clipvertex3;
layout(location = 7) out float clipvertex4;
layout(location = 8) out float clipvertex5;

void main()
{
    vec4 vertex = mv * position;
    gl_Position = mvp * position;
    gl_PointSize = psize;
    uv = texcoords;
    // FIXME: texture matrix currently disabled
    //uv = tm * vec4(texcoords.xy, 0.0, 1.0);
    color = color0;
    
    clipvertex0 = dot(vertex, clipplane0);
    clipvertex1 = dot(vertex, clipplane1);
    clipvertex2 = dot(vertex, clipplane2);
    clipvertex3 = dot(vertex, clipplane3);
    clipvertex4 = dot(vertex, clipplane4);
    clipvertex5 = dot(vertex, clipplane5);

    vpos = vertex.xyz;
}
@end

@fs fs

#define FOG_NONE 0
#define FOG_LINEAR 1
#define FOG_EXP 2
#define LOG2 1.442695

layout(binding=1) uniform state {
    int flags;
    int layer;
    float alpha_test;
    int clipplanes;
    int fog_mode;
    vec4 fog_color;
    float fog_density;
    float fog_start;
    float fog_end;
    float fog_scale;
};

layout(binding=0) uniform texture2D tex0;
layout(binding=0) uniform sampler smp0;
layout(binding=1) uniform texture2D tex1;
layout(binding=1) uniform sampler smp1;

layout(location = 0) out vec4 frag_color;

layout(location = 0) in vec4 uv;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 vpos;
layout(location = 3) in float clipvertex0;
layout(location = 4) in float clipvertex1;
layout(location = 5) in float clipvertex2;
layout(location = 6) in float clipvertex3;
layout(location = 7) in float clipvertex4;
layout(location = 8) in float clipvertex5;

void main()
{
    float c = 0;
    if ((clipplanes & 1) == 1)
    {
        c += min(0.0, clipvertex0);
    }
    if ((clipplanes & 2) == 2)
    {
        c += min(0.0, clipvertex1);
    }
    if ((clipplanes & 4) == 4)
    {
        c += min(0.0, clipvertex2);
    }
    if ((clipplanes & 8) == 8)
    {
        c += min(0.0, clipvertex3);
    }
    if ((clipplanes & 16) == 16)
    {
        c += min(0.0, clipvertex4);
    }
    if ((clipplanes & 32) == 32)
    {
        c += min(0.0, clipvertex5);
    }

    if (c < 0)
    {
        discard;
    }

    vec4 c0 = texture(sampler2D(tex0, smp0), uv.xy);
    if (alpha_test != 0 && c0.w < alpha_test)
    {
        discard;
    }

    vec4 fcolor = color;

    float fogf = 0.0;
    if (fog_mode != FOG_NONE)
    {
        float fog_dist = length(vpos);

        if (fog_mode == FOG_LINEAR)
        {
            fogf = clamp(smoothstep(fog_start, fog_end, fog_dist), 0, 1);
            
        }
        else
        {
            //fogf = 1.0f - clamp(exp(-fog_density * fog_dist), 0.0, 1.0);
            fogf = 1. - clamp(exp2(-fog_density * fog_density * fog_dist * fog_dist * LOG2), 0, 1);
        }        
    }

    if ((flags & 1) == 1)
    {
        fcolor *= c0;        
        fcolor *= texture(sampler2D(tex1, smp1), uv.zw);                        

        if (fogf > 0.0)
        {   
            fcolor = mix(fcolor, fog_color, fogf);
        }

        
    }
    else
    {
         fcolor *= c0;
         if (fogf > 0.0)
         {
            fcolor.rgb = mix(fcolor, fog_color, fogf).rgb;
         }
    }

    frag_color = fcolor;
}
@end

@program sgl vs fs