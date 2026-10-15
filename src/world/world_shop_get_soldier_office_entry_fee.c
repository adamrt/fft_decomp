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

/* Menu callback: the hiring fee for the highlighted row of the hire menu,
 * 1500 for row 0, 1400 for row 1 and 0 otherwise. */
s32 world_shop_get_soldier_office_entry_fee(s32 unused_row) {
    s16 mode = g_world_shop_hire_menu_thread_data.selected_index;

    if (mode == 0) {
        return 0x5DC;
    }
    return (mode == 1) ? 0x578 : 0;
}
