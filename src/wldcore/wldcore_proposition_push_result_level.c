#include "fft/wldcore.h"

/* Open the proposition-result panel for g_main_active_propositions[proposition]:
 * total the participants' rewards plus the bonus into g_wldcore_proposition_gil_amount, draw the
 * result text, participant names and per-participant rewards, start the
 * reporting unit's message on WORLD thread 14, and push a type-0x11 level.
 *
 * `x` keeps the first column in its own variable: its longer lifetime is what
 * lets loop.c hoist the constant into a saved register as the target does.
 * The excluded party and result words are read as g_wldcore_job_selection
 * members: the target keeps the result load behind the level-record stores,
 * which GCC 2.6.3 only does when both accesses are aggregate members. */
void wldcore_proposition_push_result_level(s32 proposition) {
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t point;
    s16 text_width;
    s16 text_rows;
    s32 index;
    s32 i;
    s32 message;
    s32 count;
    s32 depth;
    u16* panel_text;
    s32* party;
    wldcore_point32_t* base;
    s32 rows;

    count = g_main_active_propositions[proposition].participant_count;
    g_wldcore_proposition_gil_amount = 0;
    for (i = 0; i < count; i++) {
        g_wldcore_proposition_gil_amount += g_wldcore_job_selection.rows[2][i];
    }
    g_wldcore_proposition_gil_amount += g_wldcore_job_selection.reward_value;
    g_world_text_substitution_values[1] = g_wldcore_proposition_gil_amount;
    switch (g_wldcore_job_selection.reward_type) {
    case 1:
        g_world_text_substitution_values[0] = g_wldcore_job_selection.reward_index + 0x782E;
        break;
    case 2:
        g_world_text_substitution_values[0] = g_wldcore_job_selection.reward_index + 0x781B;
        break;
    case 3:
        g_world_text_substitution_values[0] = g_wldcore_job_selection.reward_index + 0x7800;
        break;
    default:
        g_world_text_substitution_values[0] = 0x7806;
        break;
    }
    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    panel_text = &g_wldcore_selected_proposition_row[0].fields.location;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_result.render_index = index;
    world_text_measure_pixels(&text_width, &text_rows, world_text_find_entry(*panel_text + 0xF7FF));
    bounds.position.x = 0;
    bounds.position.y = 0x60;
    bounds.dimensions.x = 0xF4;
    rows = text_rows + 2;
    bounds.dimensions.y = (count + rows) * 0x10 + 0x10;
    wldcore_window_build_render_record_image(
        index, bounds.position, bounds.dimensions, 0, g_wldcore_window_image_buffer);
    point.x = 8;
    point.y = text_rows * 0x10 + 8;
    wldcore_menu_display_text_entry(index, 0xB830, point, g_wldcore_window_image_buffer);
    point.x = 8;
    point.y = 8;
    wldcore_menu_display_text_entry(index, 0xB84E, point, g_wldcore_window_image_buffer);
    point.x = 0x44;
    point.y = 8;
    wldcore_menu_display_text_entry(index, *panel_text + 0xF7FF, point, g_wldcore_window_image_buffer);
    for (i = 0; i < count; i++) {
        s32 row = i + 2;
        s32 x = 0x38;

        point.y = (text_rows + row) * 0x10 + 8;
        point.x = x;
        wldcore_menu_display_text_entry(index, g_main_active_propositions[proposition].participant_indices[i] | 0x4000,
            point, g_wldcore_window_image_buffer);
    }
    for (i = 0; i < count; i++) {
        s32 row = i + 2;

        point.x = 0xA0;
        point.y = (text_rows + row) * 0x10 + 8;
        g_world_text_substitution_values[0] = g_wldcore_job_selection.rows[0][i];
        wldcore_menu_display_text_entry(index, 0xB831, point, g_wldcore_window_image_buffer);
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= 0x100;
    base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
    base->x = -0x7A;
    base->y = -0x2A;
    if (world_thread_is_running(0xE) == 0) {
        world_thread_start(0xE, world_text_message_box_thread);
    }
    party = &g_wldcore_job_selection.excluded_party_index;
    g_world_text_substitution_values[0] = *party + 0x4000;
    message = 0x9C;
    world_script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, world_get_party_unit_formation_sprite(*party));
    if (g_wldcore_job_selection.gate != 0) {
        message = 0xA2;
    }
    if (world_thread_is_running(0xE) == 0) {
        world_thread_start(0xE, world_text_message_box_thread);
    }
    world_thread_set_parameters(0xE, 0x19, wldcore_proposition_adjust_message_index(message, *party) + 0x8800, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_result.proposition = proposition;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_result.delay = 0;
    switch (g_wldcore_job_selection.result) {
    case 0:
        main_sound_play_2_sfx(0x4D, 0x4E);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_result.delay = 0x78;
        break;
    case 1:
        main_sound_play_2_sfx(0x4F, 0x50);
        break;
    case 2:
        wldcore_sound_play_effect(0x51);
        break;
    }
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].proposition_result.phase = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_RESULT;
    g_wldcore_menu_stack_depth = depth + 1;
}
