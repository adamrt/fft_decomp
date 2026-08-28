#include "fft/world.h"

/* Reconcile a sorted inventory category with the party's item ownership.
 *
 * Newly owned items go first; absent items are removed without reordering others.
 */
void world_item_reconcile_sorted_list(s32 list_index) {
    s32 item_id;
    s32 type;
    s32 category;

    for (item_id = 1; item_id < 254; item_id++) {
        type = world_item_get_menu_category((s16)item_id);
        category = 0;
        if (type != 0) {
            category = type - 1;
        }
        if (list_index == category) {
            if (world_item_count_owned_and_equipped((s16)item_id)) {
                if (!world_item_is_in_sorted_list(item_id, list_index)) {
                    world_item_prepend_sorted(item_id, list_index);
                }
            } else {
                world_item_remove_sorted(item_id, list_index);
            }
        }
    }
}
