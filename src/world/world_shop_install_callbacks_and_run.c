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

/* Run the fitting room's cost display script with its three menu value
 * callbacks installed (gil, fitting cost, gil minus cost); the script gets
 * no input while a menu is busy. */
void world_shop_install_callbacks_and_run(void) {
    g_world_menu_script_callbacks[0].value = world_shop_adjust_gil;
    g_world_menu_script_callbacks[1].value = world_shop_add_fitting_room_cost;
    g_world_menu_script_callbacks[2].value = world_shop_get_gil_minus_fitting_room_cost;
    menu_run_script_with_palette_mode_variant(
        &g_world_shop_fitting_cost_menu_script, g_menu_input_primary_repeat, world_menu_is_busy(0));
}
