#include "fft/thread.h"
#include "fft/world.h"

/* Soldier office: the name entry keyboard of the Rename service; sibling of
 * world_shop_run_hire_name_entry_step, which goes on to the fee prompt (0xF82A). */
void world_shop_run_rename_name_entry_step(void) {
    if (g_world_shop_rename_name_entry_initialized == 0) {
        g_world_formation_unit_banner_enabled = 0;
        menu_toggle_unit_status_thread(0);
        g_world_shop_rename_name_entry_initialized = 1;
    }
    world_formation_update_and_draw_unit_grid(
        1, 0, 0, 0, g_menu_input_lock_mode, world_shop_rename_unit_get_unit_marker, 0);
    g_world_shop_entered_unit_name
        = world_name_run_entry_keyboard_frame(g_world_unit_pointers[g_unit_selected_index]->name, 0);
    if (g_world_shop_entered_unit_name != 0) {
        g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN;
        world_formation_set_unit_name(g_unit_selected_index, g_world_shop_entered_unit_name);
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF821, 0);
        g_world_shop_background_visible = 1;
        g_world_shop_rename_name_entry_initialized = 0;
    }
}
