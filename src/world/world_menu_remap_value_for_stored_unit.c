#include "fft/world.h"
#include "psx/types.h"

/* Provisional: 5-byte remap record, 8 entries at g_world_menu_value_remap_table. */
typedef struct world_menu_value_remap {
    u8 menu_index;  /* 0x00 */
    u8 replacement; /* 0x01 */
    u8 matches[3];  /* 0x02 */
} world_menu_value_remap_t;

extern world_menu_value_remap_t g_world_menu_value_remap_table[8];

/* Target 0x800f302c. When menu_index is in the 12-entry lookup table, remaps
 * the stored unit's row value through the matching remap record and copies
 * it into the menu entry's 0x38 field. */
void world_menu_remap_value_for_stored_unit(s32 menu_index) {
    s32 i;
    s32 k;
    s32 m;
    u8* row;
    world_menu_value_remap_t* remap;

    for (i = 0; i < 12; i++) {
        if (g_world_menu_command_maps[i].menu_id == menu_index) {
            break;
        }
    }
    if (i == 12) {
        return;
    }
    row = g_world_menu_unit_selection_rows[g_world_unit_view_battle_id].bytes;
    for (k = 0, remap = g_world_menu_value_remap_table; k < 8; remap++, k++) {
        if (menu_index == g_world_menu_value_remap_table[k].menu_index) {
            /* Keep the signed index bound used by the target. */
            for (m = 2; m < 5; m++) {
                if (*row == remap->matches[m - 2]) {
                    *row = g_world_menu_value_remap_table[k].replacement;
                    break;
                }
            }
        }
    }
    g_world_menu_thread_menu_data[menu_index].selected_index = *row;
}
