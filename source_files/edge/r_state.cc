
#include "r_state.h"

static RenderState state;

RenderState *RendererGetState() { return &state; }
