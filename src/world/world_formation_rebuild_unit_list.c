#include "fft/world.h"
#include "psx/types.h"

/* Rebuild the formation unit list for the screen in use: every unit on the
 * formation screen, the monsters at the soldier office and the available
 * humans in the other shops. Also reconciles the five item category lists
 * and resets the cursor and scroll. */
void world_formation_rebuild_unit_list(void) {
    s32 mode;

    if (g_world_menu_context_id == WORLD_MENU_CONTEXT_FORMATION) {
        mode = WORLD_FORMATION_RECORD_LIST_ALL;
    } else {
        mode = WORLD_FORMATION_RECORD_LIST_MONSTERS;
        if (g_world_menu_context_id != WORLD_MENU_CONTEXT_SOLDIER_OFFICE) {
            mode = WORLD_FORMATION_RECORD_LIST_AVAILABLE_HUMANS;
        }
    }
    g_world_formation_unit_count = world_formation_build_record_list(0, g_world_unit_pointers, mode);
    item_rebuild_category_list(0);
    item_rebuild_category_list(1);
    item_rebuild_category_list(2);
    item_rebuild_category_list(3);
    item_rebuild_category_list(4);
    world_formation_init_cursor_and_scroll_state(0);
    g_world_formation_screen_running = 1;
}
