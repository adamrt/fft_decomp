#include "fft/thread.h"
#include "fft/wldcore.h"

/* Pushes the proposition participant list level (type 0x27) for
 * g_main_active_propositions[proposition].
 *
 * The 0x40 frame needs 32 bytes of locals the body never uses. The `- -`
 * spellings keep the target's operand order (render-record base first) that a
 * plain `+` of a loaded field loses to expand_binop's commutative swap. */
void wldcore_menu_push_participant_list_level(s32 proposition) {
    /* Target frame evidence establishes 32 bytes of otherwise unused locals. */
    u8 stack_scratch[32];
    s32 depth;
    s32 window;

    wldcore_unpack_proposition_row(
        g_wldcore_selected_proposition_row, g_main_active_propositions[proposition].proposition_id);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant_list.proposition = proposition;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_render.window_index]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .window_render.render_index]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    window = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant_list.cursor_window = window;
    g_wldcore_window_records[window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[window].priority = 8;
    g_wldcore_window_records[window].anim_counter = 0;
    g_wldcore_window_records[window].frame_index = 0;
    wldcore_list_build_proposition_detail_panel_image(
        &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth], proposition, 1);
    g_text_substitution_values[0]
        = g_main_active_propositions[proposition].proposition_id | TEXT_ID_PROPOSITION_NAME_BASE;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].participant_list.cursor_window].position.x
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth].participant_list.render_index].base.x
        + 0x38;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].participant_list.cursor_window].position.y
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth].participant_list.render_index].base.y
        - -(g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST].selected_index * 16)
        - -(g_wldcore_menu_stack_records_next[depth].participant_list.panel_rows * 16 + 0xE);
    g_wldcore_menu_stack_records_next[depth].participant_list.entry_count
        = g_main_active_propositions[proposition].participant_count;
    if (g_wldcore_menu_stack_records_next[depth].participant_list.entry_count
        < g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST].selected_index + 1) {
        g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST].selected_index = 0;
    }
    g_wldcore_menu_stack_records_next[depth].participant_list.cursor
        = g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST].selected_index;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_menu_stack_records_next[depth].participant_list.mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PARTICIPANT_LIST;
}
