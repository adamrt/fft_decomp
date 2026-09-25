#include "fft/wldcore.h"

/* Build the image of the proposition detail panel for g_main_active_propositions[proposition]:
 * the elapsed/assigned day counts and the location go into text substitution
 * words 0-2, and one 16-pixel row is drawn per participant.
 *
 * The panel's row count is the measured row count of text g_wldcore_selected_proposition_row[0].fields.location +
 * 0xf7ff plus two, kept in the level record at +0x0c.
 *
 * The target stores base_x/base_y through one pointer to the pair, and reads
 * every progress field through the array form g_main_active_propositions[proposition], which
 * is what produces its four-instruction ASPSX $at indexed loads.
 *
 * `panel_text` keeps the text-id source's address in a saved register across
 * both reads, as the target does. Assigning bounds.position.x before
 * position.y is what puts the two by-value halves in the target's registers. */
void wldcore_list_build_proposition_detail_panel_image(
    wldcore_menu_window_pair_render_level_t* level, s32 proposition, s32 coordinate_mode) {
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t point;
    s16 text_width;
    s16 text_rows;
    s32 index;
    s32 rows;
    s32 i;
    u16* panel_text;
    wldcore_point32_t* base;

    panel_text = &g_wldcore_selected_proposition_row[0].fields.location;
    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    level->render_index = index;
    world_text_measure_pixels(&text_width, &text_rows, world_text_find_entry(*panel_text + 0xF7FF));
    rows = text_rows + 2;
    level->panel_rows = rows;
    g_world_text_substitution_values[0] = g_main_active_propositions[proposition].elapsed_days;
    g_world_text_substitution_values[1] = g_main_active_propositions[proposition].assigned_days;
    g_world_text_substitution_values[2] = g_main_active_propositions[proposition].location | TEXT_ID_SECTION_9000_BASE;
    bounds.position.x = 0;
    bounds.position.y = 0x60;
    bounds.dimensions.x = 0xF4;
    bounds.dimensions.y = (g_main_active_propositions[proposition].participant_count + rows) * 0x10 + 0x10;
    wldcore_window_build_render_record_image(
        index, bounds.position, bounds.dimensions, coordinate_mode, g_wldcore_proposition_detail_image_buffer);
    point.x = 8;
    point.y = text_rows * 0x10 + 8;
    wldcore_menu_display_text_entry(index, 0xB821, point, g_wldcore_proposition_detail_image_buffer);
    point.x = 8;
    point.y = 8;
    wldcore_menu_display_text_entry(index, 0xB84E, point, g_wldcore_proposition_detail_image_buffer);
    point.x = 0x44;
    point.y = 8;
    wldcore_menu_display_text_entry(index, *panel_text + 0xF7FF, point, g_wldcore_proposition_detail_image_buffer);
    for (i = 0; i < g_main_active_propositions[proposition].participant_count; i++) {
        s32 row = i + 2;

        point.x = 0x3C;
        point.y = (text_rows + row) * 0x10 + 8;
        wldcore_menu_display_text_entry(index,
            g_main_active_propositions[proposition].participant_indices[i] | TEXT_ID_UNIT_NAME_BASE, point,
            g_wldcore_proposition_detail_image_buffer);
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_proposition_detail_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].flags |= 0x100;
    base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
    base->x = -0x7A;
    base->y = -0x28;
}
