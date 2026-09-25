#include "fft/wldcore.h"

/* Pushes the active-proposition list level (type 0x26).
 *
 * Starts WORLD thread 14 on entry point 0xb848, hides the parent level's
 * cursor window and content render record, then appends a cursor window and
 * three column-header windows (sequences 2 / 0x63 / 0x64 / 0x65) plus one
 * render record sized to g_main_save_proposition_count rows.  One row is
 * seeded per saved proposition, and the panel image is drawn in three passes:
 * the proposition name at x=8 (text 0xd000 | proposition_id), the participant
 * count at x=0x9c (text 0xb866) and the days remaining at x=0xb6
 * (text 0xb867, assigned_days - elapsed_days); both numeric columns go
 * through g_world_text_substitution_values[0].  Finally the saved cursor row
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
#define LEVEL  g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list
#define PARENT g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].proposition_list

void wldcore_list_open_active_propositions(void) {
    wldcore_point32_t point;
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t* base;
    s32 index;
    s32 depth;
    s32 i;
    s32 temp;
    s32 dead_locals[20];

    world_thread_set_parameters(0xE, 0x19, 0xB848, 0);

    if (g_wldcore_window_render_records[PARENT.title_window].flags & 0x10) {
        g_wldcore_window_records[PARENT.cursor_window].flags &= ~0x10;
        g_wldcore_window_render_records[PARENT.title_window].flags &= ~0x10;
        g_wldcore_window_render_records[PARENT.title_window].flags |= 0x100;
    }

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    LEVEL.cursor_window = index;
    g_wldcore_window_records[index].sequence = 2;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    i = 0;
    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    LEVEL.title_window = index;
    g_wldcore_window_records[index].sequence = 0x63;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    LEVEL.count_window = index;
    g_wldcore_window_records[index].sequence = 0x64;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    LEVEL.days_window = index;
    g_wldcore_window_records[index].sequence = 0x65;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    LEVEL.entry_count = 0;
    while (i < g_main_save_proposition_count) {
        LEVEL.entries[LEVEL.entry_count] = i;
        i++;
        LEVEL.entry_count++;
    }

    i = 0;
    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    LEVEL.render_index = index;
    bounds.position.y = 0x40;
    bounds.position.x = 0;
    bounds.dimensions.x = 0xD8;
    bounds.dimensions.y = LEVEL.entry_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(
        index, bounds.position, bounds.dimensions, 2, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].flags |= 0x100;
    base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
    base->x = -0x60;
    base->y = -0x28;

    point.x = 8;
    point.y = 8;
    while (i < LEVEL.entry_count) {
        wldcore_menu_display_text_entry(index,
            g_main_active_propositions[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                           .proposition_list.entries[i]]
                    .proposition_id
                | 0xD000,
            point, g_wldcore_window_image_buffer);
        point.y += 0x10;
        i++;
    }

    point.x = 0x9C;
    point.y = 8;
    i = 0;
    while (i < LEVEL.entry_count) {
        g_world_text_substitution_values[0]
            = g_main_active_propositions[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                             .proposition_list.entries[i]]
                  .participant_count;
        wldcore_menu_display_text_entry(index, 0xB866, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
        i++;
    }

    point.x = 0xB6;
    point.y = 8;
    i = 0;
    while (i < LEVEL.entry_count) {
        s32 entry = LEVEL.entries[i];
        g_world_text_substitution_values[0]
            = g_main_active_propositions[entry].assigned_days - g_main_active_propositions[entry].elapsed_days;
        wldcore_menu_display_text_entry(index, 0xB867, point, g_wldcore_window_image_buffer);
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
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.cursor_window].x
        = g_wldcore_window_render_records[index].base_x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.cursor_window].y
        = g_wldcore_window_render_records[index].base_y - g_wldcore_active_propositions_saved_cursor[0] * -0x10 + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.title_window].x
        = g_wldcore_window_render_records[index].base_x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.title_window].y
        = g_wldcore_window_render_records[index].base_y - 2;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.count_window].x
        = g_wldcore_window_render_records[index].base_x + 0x94;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.count_window].y
        = g_wldcore_window_render_records[index].base_y - 2;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.days_window].x
        = g_wldcore_window_render_records[index].base_x + 0xB2;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_list.days_window].y
        = g_wldcore_window_render_records[index].base_y - 2;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_ACTIVE_PROPOSITIONS;
}
