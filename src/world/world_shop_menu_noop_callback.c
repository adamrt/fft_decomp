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

/* The on_change callback of the shop's quantity range (the record at 0x801950d8 whose max is
 * g_world_shop_quantity_limit): nothing to do. */
void world_shop_menu_noop_callback(void) {
}
