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

/* Return to the soldier office menu with message 0xf821, redrawing the gil,
 * fee and roster-count displays for this frame. */
void world_shop_reopen_soldier_office_menu_step(void) {
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xf821, 0);
    g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU;
    g_world_shop_hire_menu_initialized = 0;
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}
