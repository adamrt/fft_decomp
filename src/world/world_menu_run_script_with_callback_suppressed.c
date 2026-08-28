#include "fft/world.h"

#define WORLD_MENU_SCRIPT_PALETTE_BYTE 0x29

void world_menu_run_script_with_callback_suppressed(void) {
    s32 (**callback_slot)(s32);
    s32 (*previous_callback)(s32);
    u8 palette;

    callback_slot = &g_world_menu_script_callbacks[0];
    /* Opaque slot address: keeps the first argument setup ahead of the saved
     * callback load and the clear after the palette load, as in the target. */
    __asm__("" : "=r"(callback_slot) : "0"(callback_slot));
    previous_callback = *callback_slot;
    palette = g_world_shop_party_unit_count;
    *callback_slot = 0;
    g_world_shop_roster_count_menu_script[WORLD_MENU_SCRIPT_PALETTE_BYTE] = palette;
    world_menu_run_script_with_palette_mode(g_world_shop_roster_count_menu_script, 0, 0);
    *callback_slot = previous_callback;
}
