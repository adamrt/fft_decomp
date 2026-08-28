#include "fft/wldcore.h"

/* Pushes the type-0x16 menu level: two window records (sequences 2 and 4)
 * and one render record showing text 0xb83a in a 0x3c x 0x30 image at
 * (-0x1e, -0x18), with the windows offset inside it. Resets the menu result
 * to -1, clears the fade box colour, sets fade flag 8 and clears the new
 * level's words 0x10..0x1c.
 *
 * The level is written through the save_confirm member of the stack-record
 * union; union field access keeps the target's per-field `$at` bases. The
 * fade state is written through a pointer, which keeps the
 * target's register base for the offset-0 flags word. The unused array
 * preserves the target's 0x68-byte frame. */
void wldcore_menu_push_sound_novel_quit_level(void) {
    wldcore_window_render_bounds16_t bounds;
    u8 unused[0x28];
    wldcore_point32_t* base;
    s32 index;
    s32 depth;
    wldcore_sortbox_state_t* fade;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].save_confirm.cursor_window = index;
    g_wldcore_window_records[index].sequence = 2;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].save_confirm.second_window = index;
    g_wldcore_window_records[index].sequence = 4;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].save_confirm.render_index = index;
    bounds.position.x = 0x80;
    bounds.dimensions.x = 0x3C;
    bounds.dimensions.y = 0x30;
    bounds.position.y = 0;
    wldcore_window_init_render_record_image(
        index, bounds.position, bounds.dimensions, 0, 0xB83A, g_wldcore_window_image_buffer);
    base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
    g_wldcore_window_render_records[index].priority = 8;
    depth = g_wldcore_menu_stack_depth;
    base->x = -((s16)bounds.dimensions.x / 2);
    base->y = -0x18;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].save_confirm.cursor_window].x
        = g_wldcore_window_render_records[index].base_x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].save_confirm.cursor_window].y
        = g_wldcore_window_render_records[index].base_y + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].save_confirm.second_window].x
        = g_wldcore_window_render_records[index].base_x + 4;
    g_wldcore_menu_result = -1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].save_confirm.second_window].y
        = g_wldcore_window_render_records[index].base_y - 2;
    g_wldcore_menu_stack_depth = depth + 1;
    fade = &g_wldcore_screen_fade_state;
    fade->boxes[0].r = 0;
    fade->boxes[0].g = 0;
    fade->boxes[0].b = 0;
    fade->flags[0] |= 8;
    g_wldcore_menu_stack_records_next[depth].save_confirm.choice = 0;
    g_wldcore_menu_stack_records_next[depth].save_confirm.field_18 = 0;
    g_wldcore_menu_stack_records_next[depth].save_confirm.fade_in = 0;
    g_wldcore_menu_stack_records_next[depth].save_confirm.fade_out = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_SOUND_NOVEL_QUIT;
}
