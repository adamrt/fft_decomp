#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* The menu-screen library's copy of menu_lerp_fixed12 (the same routine compiled twice). */
s32 world_screen_menu_lerp_fixed12(s32 from, s32 to, s32 t) {
    return ((to - from) * t) / ONE + from;
}
