#include "psx/gte.h"
#include "psx/types.h"

s32 battle_menu_lerp_fixed12(s32 start, s32 end, s32 factor) {
    return ((end - start) * factor) / ONE + start;
}
