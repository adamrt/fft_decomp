#include "psx/gte.h"
#include "psx/types.h"

s32 world_menu_lerp_fixed12_b(s32 from, s32 to, s32 t) {
    return ((to - from) * t) / ONE + from;
}
