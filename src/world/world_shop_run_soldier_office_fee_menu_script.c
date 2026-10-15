#include "fft/world.h"

/* Run the soldier office's fee display script with
 * world_shop_get_soldier_office_entry_fee installed as menu value callback 0,
 * then restore the previous callback. */
void world_shop_run_soldier_office_fee_menu_script(void) {
    menu_callback_t* callback_slot;
    s32 (*previous_callback)(s32);

    callback_slot = g_world_menu_script_callbacks;
    previous_callback = callback_slot->value;
    callback_slot->value = world_shop_get_soldier_office_entry_fee;
    menu_run_script_with_palette_mode_variant(&g_world_shop_soldier_office_fee_menu_script, 0, 0);
    callback_slot->value = previous_callback;
}
