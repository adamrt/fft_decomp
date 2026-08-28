#include "fft/data.h"
#include "fft/world.h"
#include "psx/types.h"

void world_formation_rebuild_unit_list(void) {
    s32 mode;

    if (g_world_shop_id == -1) {
        mode = 0;
    } else {
        mode = 2;
        if (g_world_shop_id != 0x65) {
            mode = 1;
        }
    }
    g_world_formation_unit_count = world_formation_build_record_list(0, g_world_formation_unit_pointers, mode);
    world_item_reconcile_sorted_list(0);
    world_item_reconcile_sorted_list(1);
    world_item_reconcile_sorted_list(2);
    world_item_reconcile_sorted_list(3);
    world_item_reconcile_sorted_list(4);
    world_formation_init_cursor_and_scroll_state(0);
    g_world_formation_screen_running = 1;
}
