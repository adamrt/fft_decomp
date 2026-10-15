#include "fft/menu.h"
#include "fft/wldcore.h"

/* Build the image of the proposition detail panel for g_main_active_propositions[proposition]:
 * the elapsed/assigned day counts and the location go into text substitution
 * 32-bit values 0-2, and one 16-pixel row is drawn per participant.
 *
 * Both menu variants share the detail render index and row count.
 *
 * The panel's row count is the measured row count of text g_wldcore_selected_proposition_row[0].name_text +
 * 0xf7ff plus two, kept in the level record at +0x0c.
 *
 * The target stores base_x/base_y through one pointer to the pair, and reads
 * every progress field through the array form g_main_active_propositions[proposition], which
 * is what produces its four-instruction ASPSX $at indexed loads.
 *
 * `panel_text` keeps the text-id source's address in a saved register across
 * both reads, as the target does. Assigning bounds.x before
 * position.y is what puts the two by-value halves in the target's registers. */
void wldcore_list_build_proposition_detail_panel_image(
    wldcore_menu_stack_record_t* level, s32 proposition, s32 coordinate_mode) {
    RECT bounds;
    point32_t point;
    s16 text_width;
    s16 text_rows;
    s32 index;
    s32 rows;
    s32 i;
    u16* panel_text;
    point32_t* base;

    panel_text = &g_wldcore_selected_proposition_row[0].name_text;
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    level->proposition_detail_panel.render_index = index;
    text_measure_pixels(
        &text_width, &text_rows, text_find_entry(*panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1)));
    rows = text_rows + 2;
    level->proposition_detail_panel.panel_rows = rows;
    g_text_substitution_values[0] = g_main_active_propositions[proposition].elapsed_days;
    g_text_substitution_values[1] = g_main_active_propositions[proposition].assigned_days;
    g_text_substitution_values[2] = g_main_active_propositions[proposition].location | TEXT_ID_LOCATION_NAME_BASE;
    bounds.x = 0;
    bounds.y = 0x60;
    bounds.w = 0xF4;
    bounds.h = (g_main_active_propositions[proposition].participant_count + rows) * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, coordinate_mode, g_wldcore_proposition_detail_image_buffer);
    point.x = 8;
    point.y = text_rows * 0x10 + 8;
    window_display_text_entry_variant(
        index, WLDCORE_TEXT_ID_JOB_CONDITION_LABELS, point, g_wldcore_proposition_detail_image_buffer);
    point.x = 8;
    point.y = 8;
    window_display_text_entry_variant(
        index, WLDCORE_TEXT_ID_JOB_CONTENTS_LABEL, point, g_wldcore_proposition_detail_image_buffer);
    point.x = 0x44;
    point.y = 8;
    window_display_text_entry_variant(index, *panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1), point,
        g_wldcore_proposition_detail_image_buffer);
    for (i = 0; i < g_main_active_propositions[proposition].participant_count; i++) {
        s32 row = i + 2;

        point.x = 0x3C;
        point.y = (text_rows + row) * 0x10 + 8;
        window_display_text_entry_variant(index,
            g_main_active_propositions[proposition].participant_indices[i] | TEXT_ID_UNIT_NAME_BASE, point,
            g_wldcore_proposition_detail_image_buffer);
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_proposition_detail_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x7A;
    base->y = -0x28;
}
