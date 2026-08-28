#include "fft/world.h"

/* Structural twin of world_shop_run_soldier_office_fee_menu_script (0x80138ea4): install a menu value
 * callback, run the menu script, then restore the previous callback. */
void world_shop_run_obtain_gil_menu(void) {
    s32 (**callback_slot)(s32);
    s32 (*previous_callback)(s32);

    callback_slot = g_world_menu_script_callbacks;
    previous_callback = *callback_slot;
    *callback_slot = world_shop_obtain_gil;
    world_menu_run_script_with_palette_mode(g_world_shop_obtain_gil_menu_script, 0, 0);
    *callback_slot = previous_callback;
}
