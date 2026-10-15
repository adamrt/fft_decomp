#include "fft/script.h"
#include "fft/wldcore.h"

/* Builds the town menu list level that wldcore_bar_handle_menu_input steps: row kinds 0
 * (rumors) and 4 (leave) always, 1 (wldcore_list_open_available_propositions) when script variable 0x91
 * is set, 2 (wldcore_list_open_completed_propositions) when wldcore_proposition_count_completed for the
 * current location is nonzero, and 3 (proposition list) when
 * g_main_save_proposition_count differs from the count for location -1. The
 * rows are drawn as text 0xb800 + kind into a 0x48-wide render record at
 * (-0x78, -0x28); the remembered row g_wldcore_bar_menu_saved_cursor (cursor state 1) is reset if
 * out of range and both windows are placed on it.
 *
 * `unused` reproduces the target's 0x78-byte frame. The single `render`
 * access raises `index` above `level` in allocation priority. */
void wldcore_bar_build_menu_rows(wldcore_menu_list_window_level_t* level) {
    s32 index;
    point32_t point;
    RECT bounds;
    s32 unused[8];
    menu_cursor_state_t* cursor;
    point32_t* base;
    wldcore_window_render_record_t* render;
    s32 i;

    index = level->main_window;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    index = level->side_window;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_LIST_SIDE_LABEL;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    level->entries[0] = WLDCORE_BAR_MENU_ROW_RUMORS;
    level->entry_count = 1;
    if (script_get_variable(EVENT_SCRIPT_VAR_PROPOSITIONS_ENABLED) != 0) {
        level->entries[1] = WLDCORE_BAR_MENU_ROW_AVAILABLE_PROPOSITIONS;
        level->entry_count++;
    }
    if (wldcore_proposition_count_completed(g_wldcore_map_projection_state.marker.kind) != 0) {
        level->entries[level->entry_count] = WLDCORE_BAR_MENU_ROW_COMPLETED_PROPOSITIONS;
        level->entry_count++;
    }
    if (g_main_save_proposition_count != wldcore_proposition_count_completed(-1)) {
        level->entries[level->entry_count] = WLDCORE_BAR_MENU_ROW_PROPOSITIONS;
        level->entry_count++;
    }
    level->entries[level->entry_count] = WLDCORE_BAR_MENU_ROW_LEAVE;
    level->entry_count++;
    index = level->frame_render;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x48;
    bounds.h = level->entry_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < level->entry_count; i++) {
        window_display_text_entry_variant(
            index, level->entries[i] + TEXT_ID_WORLD_MENU_BASE, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);
    cursor = &g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_BAR_MENU];
    if (level->entry_count < cursor->selected_index + 1) {
        cursor->selected_index = 0;
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.selected_entry = cursor->selected_index;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;
    render = &g_wldcore_window_render_records[index];
    g_wldcore_window_records[level->main_window].position.x = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[level->main_window].position.y
        = g_wldcore_window_render_records[index].base.y + cursor->selected_index * 0x10 + 0xE;
    g_wldcore_window_records[level->side_window].position.x = render->base.x + 3;
    g_wldcore_window_records[level->side_window].position.y = g_wldcore_window_render_records[index].base.y - 2;
}
