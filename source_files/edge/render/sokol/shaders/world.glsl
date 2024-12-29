@ctype mat4 HMM_Mat4

@vs vs
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
layout(location = 2) out float fog_src; 
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
    fog_src = vertex.z;
    clipvertex0 = dot(vertex, clipplane0);
    clipvertex1 = dot(vertex, clipplane1);
    clipvertex2 = dot(vertex, clipplane2);
    clipvertex3 = dot(vertex, clipplane3);
    clipvertex4 = dot(vertex, clipplane4);
    clipvertex5 = dot(vertex, clipplane5);
}
@end

@fs fs
layout(binding=1) uniform state {
    int flags;
    float alpha_test;
    int cliplanes;
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
layout(location = 2) in float fog_src;
layout(location = 3) in float clipvertex0; 
layout(location = 4) in float clipvertex1; 
layout(location = 5) in float clipvertex2; 
layout(location = 6) in float clipvertex3; 
layout(location = 7) in float clipvertex4; 
layout(location = 8) in float clipvertex5; 

void main()
{
    float c = 0;
    if ((cliplanes & 1) == 1)
    {
        c += min(0.0, clipvertex0);
    }
    if ((cliplanes & 2) == 2)
    {
        c += min(0.0, clipvertex1);
    }
    if ((cliplanes & 4) == 4)
    {
        c += min(0.0, clipvertex2);
    }
    if ((cliplanes & 8) == 8)
    {
        c += min(0.0, clipvertex3);
    }
    if ((cliplanes & 16) == 16)
    {
        c += min(0.0, clipvertex4);
    }
    if ((cliplanes & 32) == 32)
    {
        c += min(0.0, clipvertex5);
    }

    if (c < 0)
    {
        discard;
    }

    vec4 fcolor = color;
    vec4 c0 = texture(sampler2D(tex0, smp0), uv.xy);
    if (alpha_test != 0 && c0.w < alpha_test)
    {
        discard;
    }

    if ((flags & 1) == 1)
    {
        vec4 c1 = texture(sampler2D(tex1, smp1), uv.zw);
        fcolor.rgb *= c0.rgb;
        fcolor *= c1;
    }
    else
    {
        fcolor *= c0;   
    }
    
    if ((flags & 2) == 2)
    {
        float fog_c = abs(fog_src);
        float fogf = clamp(exp(-fog_density * fog_c), 0., 1.);
        fcolor.rgb = mix(fog_color.rgb, fcolor.rgb, fogf);
    }
    fcolor.rgb = clamp(fcolor.rgb, vec3(0), vec3(1));    
    frag_color = fcolor;
}
@end

@program sgl vs fs

