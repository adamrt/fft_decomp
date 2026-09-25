#include "fft/wldcore.h"

/* Pushes the proposition participant list level (type 0x27) for
 * g_main_active_propositions[proposition].
 *
 * The 0x40 frame needs 32 bytes of locals the body never uses. The `- -`
 * spellings keep the target's operand order (render-record base first) that a
 * plain `+` of a loaded field loses to expand_binop's commutative swap. The
 * volatile re-read of the clamped cursor keeps `depth + 1` out of the
 * clamp branch's delay slot, which the target leaves as a nop. */
void wldcore_menu_push_participant_list_level(s32 proposition) {
    /* Target frame evidence establishes 32 bytes of otherwise unused locals. */
    u8 stack_scratch[32];
    s32 depth;
    s32 window;
    s32 count;
    s32 cursor;

    wldcore_unpack_proposition_row(
        g_wldcore_selected_proposition_row, g_main_active_propositions[proposition].proposition_id);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.proposition = proposition;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_render.window_index]
        .flags |= 0x10;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .window_render.render_index]
        .flags |= 0x10;
    window = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair_render.first_window = window;
    g_wldcore_window_records[window].sequence = 2;
    g_wldcore_window_records[window].priority = 8;
    g_wldcore_window_records[window].anim_counter = 0;
    g_wldcore_window_records[window].frame_index = 0;
    wldcore_list_build_proposition_detail_panel_image(
        &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair_render, proposition, 1);
    g_world_text_substitution_values[0] = g_main_active_propositions[proposition].proposition_id | 0xD000;
    world_thread_set_parameters(0xE, 0x19, 0xB849, 0);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].window_pair_render.first_window].x
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth].window_pair_render.render_index]
              .base_x
        + 0x38;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].window_pair_render.first_window].y
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth].window_pair_render.render_index]
              .base_y
        - -(g_wldcore_menu_cursor_states[15].selected_index * 16)
        - -(g_wldcore_menu_stack_records_next[depth].window_pair_render._unknown_0c * 16 + 0xE);
    count = g_main_active_propositions[proposition].participant_count;
    g_wldcore_menu_stack_records_next[depth].list_window.entry_count = count;
    if (count < g_wldcore_menu_cursor_states[15].selected_index + 1) {
        g_wldcore_menu_cursor_states[15].selected_index = 0;
    }
    cursor = *(volatile s32*)&g_wldcore_menu_cursor_states[15].selected_index;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_menu_stack_records_next[depth].list_window.mode = 0;
    g_wldcore_menu_stack_records_next[depth].list_window.selected_entry = cursor;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PARTICIPANT_LIST;
}
