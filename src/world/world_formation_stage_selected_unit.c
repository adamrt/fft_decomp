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
    world_unit_t** units;
    s32 index;

    g_world_formation_selected_unit_index_latch = g_unit_selected_index;
    index = g_unit_selected_index;
    units = g_world_unit_pointers;
    units[index]->status.gauges[MENU_UNIT_STATUS_GAUGE_HP].value
        = units[index]->status.gauges[MENU_UNIT_STATUS_GAUGE_HP].max;
    units[index]->status.gauges[MENU_UNIT_STATUS_GAUGE_MP].value
        = units[index]->status.gauges[MENU_UNIT_STATUS_GAUGE_MP].max;
    bcopy(&units[index]->status, &g_menu_selected_unit_stat_summary, sizeof(menu_unit_status_record_t));
    bcopy(
        &units[g_unit_selected_index]->identity, &g_world_selected_unit_identity, sizeof(menu_unit_status_identity_t));
    bcopy(&units[g_unit_selected_index]->move, &g_world_selected_unit_stat_detail, 0x40);
    world_formation_copy_unit_to_temp(g_unit_selected_index, WORLD_FORMATION_PREVIEW_UNIT_INDEX);
    if (g_menu_status_display_thread_params.flags == MENU_STATUS_PANEL_HIDDEN) {
        g_world_formation_unit_slide_pending = !(units[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG);
    } else {
        g_menu_status_display_thread_params.flags
            = (g_world_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG) ? MENU_STATUS_PANEL_HIDDEN
                                                                                           : 0;
    }
    world_menu_enable_all_order_entries();
}
