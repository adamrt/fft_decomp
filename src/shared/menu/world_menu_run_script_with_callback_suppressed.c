#include "fft/world.h"

/* Display the party unit count directly while value callback 0 is disabled. */
void world_menu_run_script_with_callback_suppressed(void) {
    s32 (*previous_callback)(s32);

    previous_callback = g_world_menu_script_callbacks[0].value;
    g_world_menu_script_callbacks[0].value = 0;
    g_world_shop_roster_count_menu_script.unit_count.value = g_world_shop_party_unit_count;
    /* The serialized script prefix shares the bytecode interpreter's instruction view. */
    menu_run_script_with_palette_mode_variant((menu_instruction_t*)&g_world_shop_roster_count_menu_script, 0, 0);
    g_world_menu_script_callbacks[0].value = previous_callback;
}
