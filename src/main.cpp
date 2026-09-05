#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <emscripten/emscripten.h>
#include "DFPSR/api/drawAPI.h"
#include "DFPSR/api/imageAPI.h"

namespace {
#include "engine/scene_state.inc"
#include "engine/geometry.inc"
#include "engine/controls.inc"
}

#include "engine/exports.inc"
