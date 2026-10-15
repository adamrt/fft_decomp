#include "fft/thread.h"
#include "fft/wldcore.h"

/* Pushes the active-proposition list level (type 0x26).
 *
 * Starts WORLD thread 14 on entry point 0xb848, re-shows the parent level's
 * cursor window and list panel (popping the panel in) when the panel is
 * hidden, then appends a cursor window and
 * three column-header windows (sequences 2 / 0x63 / 0x64 / 0x65) plus one
 * render record sized to g_main_save_proposition_count rows.  One row is
 * seeded per saved proposition, and the panel image is drawn in three passes:
 * the proposition name at x=8 (text 0xd000 | proposition_id), the participant
 * count at x=0x9c (text 0xb866) and the days remaining at x=0xb6
 * (text 0xb867, assigned_days - elapsed_days); both numeric columns go
 * through g_text_substitution_values[0].  Finally the saved cursor row
 * in g_wldcore_active_propositions_saved_cursor is clamped, the four windows are positioned relative to the
 * render record's origin, and the level is committed at depth + 1.
 *
 * The 0xb8 frame carries 0x50 bytes of locals the original never reads;
 * `dead_locals` reproduces them.
 *
 * The seeding loop's entry test is spelled `i < count` (the target's signed
 * `blez`) around a do-while that re-reads the u8 count, which gives the
 * hoisted bound copy the target keeps in $a0.  The cursor row is folded as
 * base_y - row * -16 + 14 to keep base_y as the first addu operand.
 */
void wldcore_list_open_active_propositions(void) {
    point32_t point;
    RECT bounds;
    point32_t* base;
    s32 index;
    s32 depth;
    s32 i;
    s32 temp;
    s32 dead_locals[20];

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);

    if (g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .proposition_list.title_window]
            .flags
        & WORLD_RENDER_RECORD_HIDDEN) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .proposition_list.cursor_window]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .proposition_list.title_window]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .proposition_list.title_window]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
    }

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    i = 0;
    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.title_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_PROPOSITION_TITLE_LABEL;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.count_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_PROPOSITION_COUNT_LABEL;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.days_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_PROPOSITION_DAYS_LABEL;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count = 0;
    while (i < g_main_save_proposition_count) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .proposition_list
            .entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count] = i;
        i++;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count++;
    }

    i = 0;
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.render_index = index;
    bounds.y = 0x40;
    bounds.x = 0;
    bounds.w = 0xD8;
    bounds.h = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, 2, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x60;
    base->y = -0x28;

    point.x = 8;
    point.y = 8;
    while (i < g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count) {
        window_display_text_entry_variant(index,
            g_main_active_propositions[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                           .proposition_list.entries[i]]
                    .proposition_id
                | TEXT_ID_PROPOSITION_NAME_BASE,
            point, g_wldcore_window_image_buffer);
        point.y += 0x10;
        i++;
    }

    point.x = 0x9C;
    point.y = 8;
    i = 0;
    while (i < g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count) {
        g_text_substitution_values[0]
            = g_main_active_propositions[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                             .proposition_list.entries[i]]
                  .participant_count;
        window_display_text_entry_variant(index, WLDCORE_TEXT_ID_VALUE, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
        i++;
    }

    point.x = 0xB6;
    point.y = 8;
    i = 0;
    while (i < g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count) {
        s32 entry = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entries[i];
        g_text_substitution_values[0]
            = g_main_active_propositions[entry].assigned_days - g_main_active_propositions[entry].elapsed_days;
        window_display_text_entry_variant(index, WLDCORE_TEXT_ID_TWO_DIGIT_VALUE, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
        i++;
    }

    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);

    depth = g_wldcore_menu_stack_depth;
    /* temp spans two blocks without crossing a call, so global allocation
     * gives the clamp flag and the cursor copy the target's v1. */
    temp = g_wldcore_menu_stack_records_next[depth].proposition_list.entry_count
        < g_wldcore_active_propositions_saved_cursor[0] + 1;
    if (temp) {
        g_wldcore_active_propositions_saved_cursor[0] = 0;
    }
    temp = g_wldcore_active_propositions_saved_cursor[0];
    g_wldcore_menu_stack_records_next[depth].proposition_list.cursor = temp;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.cursor_window].position.x
        = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.cursor_window].position.y
        = g_wldcore_window_render_records[index].base.y - g_wldcore_active_propositions_saved_cursor[0] * -0x10 + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.title_window].position.x
        = g_wldcore_window_render_records[index].base.x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.title_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.count_window].position.x
        = g_wldcore_window_render_records[index].base.x + 0x94;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.count_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.days_window].position.x
        = g_wldcore_window_render_records[index].base.x + 0xB2;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.days_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_ACTIVE_PROPOSITIONS;
}
