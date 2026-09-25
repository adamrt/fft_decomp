#include "fft/world.h"
#include "psx/types.h"

extern void world_formation_update_and_draw_unit_grid(s32, s32, s32, s32, s32, s32 (*)(void), s32);

/* Target 0x80138d78; sibling of world_shop_run_rename_unit_entry_step (0xF82A / 0x18). */
void world_shop_run_rename_unit_entry_step_2(void) {
    if (g_world_shop_rename_entry_2_initialized == 0) {
        g_world_formation_unit_banner_enabled = 0;
        world_menu_toggle_unit_status_banner(0);
        g_world_shop_rename_entry_2_initialized = 1;
    }
    world_formation_update_and_draw_unit_grid(
        1, 0, 0, 0, *(s16*)&g_world_thread_task_active, world_shop_rename_unit_get_unit_marker, 0);
    /* The target also passes a1 = 0 to this one-argument keyboard frame. */
    g_world_shop_entered_unit_name = (const u8*)((s32 (*)(u8*, s32))world_name_run_entry_keyboard_frame)(
        g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->name, 0);
    if (g_world_shop_entered_unit_name != 0) {
        g_world_shop_menu_step = 0x19;
        world_formation_set_unit_name(g_world_formation_selected_unit_index, g_world_shop_entered_unit_name);
        world_thread_start(2, world_text_message_box_thread);
        world_thread_set_parameters(2, 0x19, 0xF821, 0);
        g_world_shop_background_visible = 1;
        g_world_shop_rename_entry_2_initialized = 0;
    }
}
