#include "fft/data.h"
#include "fft/world.h"
#include "psx/libc.h"

/*
 * Snapshot the selected formation unit after restoring its HP and MP.
 *
 * The three display-data groups are copied separately before the full unit
 * snapshot. O1 preserves the target's load/store order and direct array
 * reference in the final branch; O2 emits 364 bytes instead of 388.
 */
void world_formation_stage_selected_unit(void) {
    /* Preserve the target's unused 128-byte stack area. */
    u8 unused[128];
    world_formation_unit_t** units;
    s32 index;

    g_world_formation_selected_unit_index_latch = g_world_formation_selected_unit_index;
    index = g_world_formation_selected_unit_index;
    units = g_world_formation_unit_pointers;
    units[index]->hp = units[index]->max_hp;
    units[index]->mp = units[index]->max_mp;
    bcopy(units[index], &g_world_selected_unit_stat_summary, 0x22);
    bcopy(&units[g_world_formation_selected_unit_index]->name_index, &g_world_selected_unit_identity, 0xe);
    bcopy(&units[g_world_formation_selected_unit_index]->move, &g_world_selected_unit_stat_detail, 0x40);
    world_formation_copy_unit_to_temp(g_world_formation_selected_unit_index, 20);
    if (g_world_status_display_thread_params.flags == 0x80) {
        g_world_formation_unit_slide_pending
            = ((units[g_world_formation_selected_unit_index]->gender_flags >> 2) ^ 1) & 1;
    } else {
        g_world_status_display_thread_params.flags
            = (g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->gender_flags << 5) & 0x80;
    }
    world_menu_enable_all_order_entries();
}
