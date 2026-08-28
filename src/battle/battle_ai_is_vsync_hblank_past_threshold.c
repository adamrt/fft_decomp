#include "psx/etc.h"

s32 battle_ai_is_vsync_hblank_past_threshold(void) {
    return VSync(1) >= 0x145;
}
