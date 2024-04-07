
#include "r_state.h"

static RenderState state;

RenderState *GetRenderState()
{
    return &state;
}
