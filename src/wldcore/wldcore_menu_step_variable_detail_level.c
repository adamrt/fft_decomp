#include "fft/wldcore.h"

/* Per-frame step of the brave-story detail level.
 *
 * Phase 1 waits for the character's portrait file, phase 2 sets up the shared
 * scrollable text window for text 0x8800 + entry and binds the
 * portrait render record to the texture rectangle of the portrait POLY_FT4 (the
 * portrait id selects a 16x16 cell of the 0x280,0x1f0 page). Phase 3 waits for
 * the text window's render record and returns to phase 0, which pops the level
 * back to the brave-story list once the text window has finished scrolling.
 *
 * The loading test reads g_main_file_cd_state.state (the same word as
 * g_main_file_still_loading): only a struct-member load stays behind the
 * delay store. The unused RECT reserves the 8 bytes that put the descriptor
 * at sp+0x18, and page_x keeps the target's (column + 0x280) association. */
void wldcore_menu_step_variable_detail_level(wldcore_menu_variable_detail_level_t* level) {
    RECT rect;
    POLY_FT4 portrait;
    s32 entry;
    s32 character;
    s32 render_index;
    s32 depth;
    s32 value;
    s32 page_x;
    urect16_t* bounds;
    rgb8_t* color;
    wldcore_point32_t* base;

    switch (level->phase) {
    case 1:
        level->timer = level->timer + 1;
        if (g_main_file_cd_state.state == 0) {
            level->phase = level->phase + 1;
        }
        return;
    case 2:
        if (level->timer < 8 || world_menu_get_event_state_flag() == 0) {
            level->timer = level->timer + 1;
            return;
        }
        world_script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, 0xFF);
        entry = level->value;
        g_wldcore_scrollable_text_window.image_y = 0x70;
        g_wldcore_scrollable_text_window.base.x = -0x7A;
        g_wldcore_scrollable_text_window.base.y = -0x28;
        g_wldcore_scrollable_text_window.text_width = 0xDC;
        g_wldcore_scrollable_text_window.rows_per_page = 8;
        g_wldcore_scrollable_text_window.priority = 9;
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_coordinate_mode = 1;
        character = entry >> 4;
        g_wldcore_scrollable_text_window.text_id = (entry & 0xFF) | 0x8800;
        if (character == 0) {
            g_wldcore_scrollable_text_window.text_substitutions[0] = g_world_text_substitution_values[0];
            g_wldcore_scrollable_text_window.text_substitutions[1] = g_main_brave_story_character_ages[0];
        } else {
            g_wldcore_scrollable_text_window.text_substitutions[0] = g_main_brave_story_character_ages[character];
            g_wldcore_scrollable_text_window.text_substitutions[1] = -1;
        }
        g_wldcore_scrollable_text_window.extra_render_index = -1;
        wldcore_text_init_scrollable_window(&g_wldcore_scrollable_text_window);
        value = world_script_get_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
        render_index = level->render_index;
        if (value != 0xFF) {
            world_gfx_set_portrait_poly_texture(&portrait, value);
            page_x = 0x280;
            g_wldcore_window_render_records[render_index].clut_x = ((value / 40) << 6) + ((value % 4) * 0x10 + page_x);
            g_wldcore_window_render_records[render_index].clut_y = ((value % 40) / 4) + 0x1F0;
            g_wldcore_window_render_records[render_index].tpage = portrait.tpage;
            /* The rectangle at +0x28 and the colour triple at +0x30 are written
             * through their own addresses; the rectangle and the base point are
             * then copied as blocks into the text window's extra-record fields. */
            bounds = (urect16_t*)&g_wldcore_window_render_records[render_index].x;
            bounds->x = portrait.u0;
            bounds->y = portrait.v0;
            bounds->w = portrait.u1 - portrait.u0;
            bounds->h = portrait.v2 - portrait.v0;
            color = (rgb8_t*)&g_wldcore_window_render_records[render_index].red;
            color->r = 0x80;
            color->g = 0x80;
            color->b = 0x80;
            g_wldcore_window_render_records[render_index].anim_counter = 0;
            base = (wldcore_point32_t*)&g_wldcore_window_render_records[render_index].base_x;
            base->x = -0x72;
            base->y = -0x22;
            g_wldcore_window_render_records[render_index].priority = 9;
            g_wldcore_scrollable_text_window.extra_render_index = render_index;
            *(wldcore_point32_t*)&g_wldcore_scrollable_text_window.extra_base_x = *base;
            *(urect16_t*)&g_wldcore_scrollable_text_window.extra_x = *bounds;
        } else {
            g_wldcore_window_render_records[render_index].flags |= 8;
        }
        level->phase = level->phase + 1;
        return;
    case 3:
        if (!(g_wldcore_window_render_records[g_wldcore_scrollable_text_window.render_record_index].flags & 0x100)) {
            g_wldcore_window_render_records[level->render_index].flags &= ~0x10;
            level->phase = 0;
        }
        return;
    default:
        if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) == 0) {
            depth = g_wldcore_menu_stack_depth;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
                |= 0x100;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.main_window].flags
                &= ~0x10;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.side_window].flags
                &= ~0x10;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
                &= ~0x10;
            g_wldcore_menu_stack_depth = depth - 1;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.upper_window].flags
                &= ~0x10;
            g_wldcore_window_render_record_count = g_wldcore_window_render_record_count - 1;
            g_wldcore_window_render_object_count = g_wldcore_window_render_object_count - 1;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.lower_window].flags
                &= ~0x10;
            wldcore_list_open_set_script_variables_03c0_03ff();
        }
    }
}
