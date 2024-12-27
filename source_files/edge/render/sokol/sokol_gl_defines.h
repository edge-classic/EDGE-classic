//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#pragma once

typedef unsigned char GLboolean;
typedef int           GLint;
typedef unsigned int  GLuint;
typedef float         GLfloat;
typedef double        GLdouble;

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;

typedef int GLsizei;

#define GL_FALSE 0
#define GL_TRUE  1

#define GL_NEAREST 0x2600
#define GL_LINEAR  0x2601

#define GL_TEXTURE_2D         0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800

#define GL_UNPACK_ALIGNMENT 0x0CF5

#define GL_ALPHA 0x1906

#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_DEPTH_TEST       0x0B71
#define GL_STENCIL_TEST     0x0B90

#define GL_UNSIGNED_BYTE 0x1401

#define GL_POLYGON_SMOOTH 0x0B41

#define GL_QUADS          0x0007
#define GL_TRIANGLES      0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN   0x0006
#define GL_QUAD_STRIP     0x0008
#define GL_POLYGON        0x0009

#define GL_NORMALIZE 0x0BA1

#define GL_MODULATE 0x2100

#define GL_SCISSOR_TEST 0x0C11

#define GL_LINES       0x0001
#define GL_LINE_SMOOTH 0x0B20

#define GL_PROJECTION 0x1701

#define GL_RGB  0x1907
#define GL_RGBA 0x1908

#define GL_CLIP_PLANE0 0x3000
#define GL_CLIP_PLANE1 0x3001
#define GL_CLIP_PLANE2 0x3002
#define GL_CLIP_PLANE3 0x3003
#define GL_CLIP_PLANE4 0x3004
#define GL_CLIP_PLANE5 0x3005

#define GL_GREATER 0x0204
#define GL_LEQUAL  0x0203

#define GL_TEXTURE0 0x84C0

#define GL_FOG         0x0B60
#define GL_FOG_DENSITY 0x0B62
#define GL_FOG_END     0x0B64
#define GL_FOG_START   0x0B63
#define GL_FOG_COLOR   0x0B66

#define GL_ALPHA_TEST     0x0BC0
#define GL_BLEND          0x0BE2
#define GL_CULL_FACE      0x0B44
#define GL_LIGHTING       0x0B50
#define GL_COLOR_MATERIAL 0x0B57

#define GL_COMBINE_RGB 0x8571

#define GL_TEXTURE_ENV      0x2300
#define GL_TEXTURE_ENV_MODE 0x2200

#define GL_EXP    0x0800
#define GL_GEQUAL 0x0206
#define GL_ONE    1
#define GL_ZERO   0

#define GL_SRC_ALPHA           0x0302
#define GL_DST_COLOR           0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_SRC_COLOR           0x0300

#define GL_FRONT 0x0404
#define GL_BACK  0x0405

#define GL_CLAMP         0x2900
#define GL_REPEAT        0x2901
#define GL_CLAMP_TO_EDGE 0x812F

#define GL_REPLACE  0x1E01
#define GL_COMBINE  0x8570
#define GL_PREVIOUS 0x8578

#define GL_TEXTURE  0x1702
#define GL_TEXTURE1 0x84C1

#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_NEAREST_MIPMAP_LINEAR  0x2702
#define GL_LINEAR_MIPMAP_LINEAR   0x2703
#define GL_LINEAR_MIPMAP_NEAREST  0x2701

#define GL_SMOOTH 0x1D01
#define GL_CW     0x0900

#define GL_FOG_HINT 0x0C54
#define GL_NICEST   0x1102

#define GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
