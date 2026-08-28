#include "fft/data.h"
#include "fft/world.h"
#include "psx/types.h"

void world_shop_run_rename_unit_entry_step(void) {
    if (g_world_shop_rename_entry_initialized == 0) {
        g_world_formation_unit_banner_enabled = 0;
        world_menu_toggle_unit_status_banner(0);
        g_world_shop_rename_entry_initialized = 1;
    }
    /* The target also passes a1 = 0 to this one-argument keyboard frame. */
    g_world_shop_entered_unit_name = (const u8*)((s32 (*)(u8*, s32))world_name_run_entry_keyboard_frame)(
        g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->name, 0);
    if (g_world_shop_entered_unit_name != 0) {
        g_world_shop_rename_entry_initialized = 0;
        g_world_text_substitution_values[0] = g_world_shop_service_fee;
        world_thread_start(2, world_text_message_box_thread);
        world_thread_set_parameters(2, 0x19, 0xF82A, 0);
        g_world_shop_menu_step = 0x18;
    }
}
