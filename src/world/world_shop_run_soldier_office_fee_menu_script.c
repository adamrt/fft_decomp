#include "fft/world.h"

void world_shop_run_soldier_office_fee_menu_script(void) {
    void (**callback_slot)(void);
    void (*previous_callback)(void);

    callback_slot = (void (**)(void))g_world_menu_script_callbacks;
    previous_callback = *callback_slot;
    *callback_slot = world_shop_get_soldier_office_entry_fee;
    world_menu_run_script_with_palette_mode(g_world_shop_soldier_office_fee_menu_script, 0, 0);
    *callback_slot = previous_callback;
}
