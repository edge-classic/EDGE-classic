#include "planes.h"

#include "../glx/hardext.h"
#include "loader.h"
#include "matrix.h"
#include "matvec.h"

static GLfloat planebuffer[6][4];

void APIENTRY_GL4ES gl4es_glClipPlanef(GLenum plane, const GLfloat *equation)
{
    /*
        When glClipPlane is called,
        equation is transformed by the inverse of the modelview matrix and stored in the resulting eye coordinates.
        Subsequent changes to the modelview matrix have no effect on the stored plane-equation components.
        If the dot product of the eye coordinates of a vertex with the stored plane equation components is positive or zero,
        the vertex is in with respect to that clipping plane. Otherwise, it is out.
    */

   // EDGE-CLASSIC: fix bad memory access, though this isn't perfect as could potentionally be overwritten, so if clip plane issues need to check here
   int planeidx = plane - GL_CLIP_PLANE0;
   memcpy(planebuffer[planeidx], equation, sizeof(GLfloat) * 4); 
   equation = planebuffer[planeidx];    

    PUSH_IF_COMPILING(glClipPlanef);    //TODO: fix that, equation is an array and should be copied before beeing put in the stack
    if((plane<GL_CLIP_PLANE0) || (plane>=GL_CLIP_PLANE0+hardext.maxplanes)) {
        errorShim(GL_INVALID_ENUM);
        return;
    }
    if(hardext.esversion==1) {
        LOAD_GLES2(glClipPlanef);
        errorGL();
        gles_glClipPlanef(plane, equation);
    } else {    // TODO: should fist compute the clipplane and compare to stored one before sending to hardware
        int p = plane-GL_CLIP_PLANE0;
        matrix_vector(getInvMVMat(), equation, glstate->planes[p]); //Tested, seems ok
        noerrorShim();
    }
}

AliasExport(void,glClipPlanef,,(GLenum pname, const GLfloat* params));