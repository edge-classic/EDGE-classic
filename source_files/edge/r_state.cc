
#include "r_state.h"

static gl_state_c state;

gl_state_c* RGL_GetState()
{
    return &state;
}
