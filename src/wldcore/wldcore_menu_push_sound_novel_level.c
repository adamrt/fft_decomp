#include "fft/wldcore.h"
#include "psx/pad.h"

/*
 * Pushes the sound-novel menu level (WLDCORE_MENU_LEVEL_SOUND_NOVEL).
 * `argument` is kept in the level and names the treasure detail reopened
 * when the novel ends; the novel read is the one in
 * g_wldcore_saved_record_index.
 *
 * Hides the context value display, clears VRAM and the scratch buffer, and
 * appends the level's display objects, all hidden: the two button prompts,
 * the VRAM strip record and the overlay. It then appends the novel's sound
 * effect data, restores any saved reading state into the first render record
 * (wldcore_sound_novel_restore_saved_state), blocks Cross, Square and Select
 * for the menu and starts the screen fade.
 */
void wldcore_menu_push_sound_novel_level(s32 argument) {
    RECT bounds;
    point32_t origin;
    world_animated_render_record_t* window1;
    world_animated_render_record_t* window2;
    world_animated_render_record_t* window4;
    s32 bar_render;
    s32 index;
    point32_t* pos1;
    point32_t* pos2;
    point32_t* pos3;
    point32_t* pos4;
    point32_t* render_pos;

    /* Held from entry: with every saved register taken, reload rematerialises
     * the constant beside its one use, which is the target's la placement. */
    render_pos = &g_wldcore_window_render_records[0].base;
    g_wldcore_saved_context_value_display_position = g_wldcore_context_value_display_position;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    wldcore_gfx_clear_vram_and_scratch(1);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.message = argument;

    bar_render = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.left_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_START_BUTTON;
    window1 = &g_wldcore_window_records[index];
    window1->priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    pos1 = &g_wldcore_window_records[index].position;
    pos1->x = -0x7A;
    pos1->y = 0x60;
    g_wldcore_window_records[index].palette = 0xA;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.right_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_SQUARE_BUTTON;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    window2 = &g_wldcore_window_records[index];
    window2->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    pos2 = &g_wldcore_window_records[index].position;
    pos2->x = 0x42;
    pos2->y = 0x60;
    g_wldcore_window_records[index].palette = 0xA;

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.render_index = index;
    bounds.x = 0x1E4;
    bounds.y = 0x1E8;
    bounds.w = 0x20;
    bounds.h = 8;
    origin.x = 0x20;
    origin.y = 0x1E8;
    wldcore_window_init_vram_render_record(index, bounds, origin, 0);
    pos3 = (point32_t*)((s32)render_pos + index * sizeof(wldcore_window_render_record_t));
    pos3->x = 0x34;
    pos3->y = 0x55;
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_UNTINTED;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.overlay_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_OVERLAY;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_UNTINTED;
    window4 = &g_wldcore_window_records[index];
    window4->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    pos4 = &g_wldcore_window_records[index].position;
    pos4->x = 0x54;
    pos4->y = 0x58;
    g_wldcore_window_records[index].palette = 0;

    suzuki_sound_append_resource(g_wldcore_sound_novel_sound_resource);
    wldcore_sound_novel_restore_saved_state(bar_render);
    g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_screen_fade_state.priorities[0] = 8;
    g_wldcore_screen_fade_state.flags[1] |= WLDCORE_SCREEN_FADE_BOX_VISIBLE;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel._unknown_20 = 1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.countdown = 0x270F;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.fade_timer = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.phase = WLDCORE_SOUND_NOVEL_PHASE_RUN;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_SOUND_NOVEL;
    g_wldcore_menu_stack_depth++;
    wldcore_fade_start_screen(0, 0x10);
}
