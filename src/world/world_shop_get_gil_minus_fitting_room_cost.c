#include "fft/gfx.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

s32 world_shop_get_gil_minus_fitting_room_cost(s32 unused_row) {
    s32 gil;

    gil = world_shop_adjust_gil(0);
    return gil - world_shop_add_fitting_room_cost(0);
}
