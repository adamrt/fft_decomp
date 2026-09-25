#include "fft/wldcore.h"
#include "psx/pad.h"

/* Provisional: the text box handed to world_menu_display_text_entry_mode_1 (0x800fe7a4); the s32
 * pixel width at +0x08 is the destination render record's width. The 16-byte
 * size follows the target frame (box at sp+0x10, point at sp+0x20). */
typedef struct wldcore_history_text_origin {
    s16 x;
    s16 y;
    u8 unknown_04[4];
    s32 width;
    s32 unknown_0c;
} wldcore_history_text_origin_t;

/* Handler of the history-text level (type 0x17).
 *
 * Phase 1 steps the picture render record (g_wldcore_active_saved_record.picture_render_index) and g_wldcore_hud_color
 * from 0x80 down to 0x60 while the screen fade box brightens, in five steps. Phase 2 runs the reverse ramp and then, if
 * g_wldcore_active_saved_record.text_id names a text, clears the image buffer for the level's render record (width *
 * height / 8 words), renders text 0x8800 + g_wldcore_active_saved_record.text_id into it, uploads it and clears script
 * variables 0x5c-0x5e; it then pops the level and resumes WORLD thread 14.
 * Phase 0 scrolls the text history with up/down, hiding the arrow window at
 * either end, and starts phase 2 once button bit 0x80 is released.
 *
 * Load-bearing spellings: the nonzero phases are a switch nested under
 * `phase != 0`, matching the target's test order; each fade case holds
 * &g_wldcore_active_saved_record.picture_render_index in its own pointer local, and `shade` is shared between the
 * cases (a global pseudo, as in the target's $a2); the text box position is
 * read into `x`/`y` so the s32 point is loaded with `lw`; `unused` preserves
 * the 0x58-byte frame. */
void wldcore_menu_step_sound_novel_history_level(wldcore_menu_sound_novel_history_level_t* level) {
    wldcore_history_text_origin_t box;
    wldcore_point32_t point;
    u8 unused[0x20];
    u32* image;
    s16* index1;
    s16* index2;
    s32 shade;
    s32 i;
    s32 text_id;
    s32 step;
    s32 depth;
    s32 render_index;
    s32 x;
    s32 y;

    if (level->phase != 0) {
        switch (level->phase) {
        case 1:
            shade = 0x80 - level->step * 8;
            index1 = &g_wldcore_active_saved_record.picture_render_index;
            g_wldcore_window_render_records[*index1].red = shade;
            g_wldcore_window_render_records[*index1].green = shade;
            g_wldcore_window_render_records[*index1].blue = shade;
            g_wldcore_hud_color.r = shade;
            g_wldcore_hud_color.g = shade;
            g_wldcore_hud_color.b = shade;
            shade = level->step * 8;
            g_wldcore_screen_fade_state.boxes[0].r = shade;
            g_wldcore_screen_fade_state.boxes[0].g = shade;
            g_wldcore_screen_fade_state.boxes[0].b = shade;
            step = level->step + 1;
            level->step = step;
            if (step >= 5) {
                level->step = 0;
                level->phase = 0;
            }
            break;
        case 2:
            index2 = &g_wldcore_active_saved_record.picture_render_index;
            shade = level->step * 0x10 + 0x40;
            g_wldcore_window_render_records[*index2].red = shade;
            g_wldcore_window_render_records[*index2].green = shade;
            g_wldcore_window_render_records[*index2].blue = shade;
            g_wldcore_hud_color.r = shade;
            g_wldcore_hud_color.g = shade;
            g_wldcore_hud_color.b = shade;
            shade = 0x20 - level->step * 8;
            g_wldcore_screen_fade_state.boxes[0].r = shade;
            g_wldcore_screen_fade_state.boxes[0].g = shade;
            g_wldcore_screen_fade_state.boxes[0].b = shade;
            step = level->step + 1;
            level->step = step;
            if (step < 5) {
                break;
            }
            text_id = g_wldcore_active_saved_record.text_id;
            if (text_id != -1) {
                text_id += 0x8800;
                i = 0;
                image = g_wldcore_window_image_buffer;
                point.x = 8;
                point.y = 8;
                for (;;) {
                    render_index = level->render_index;
                    if (i >= g_wldcore_window_render_records[render_index].width
                            * g_wldcore_window_render_record_heights[render_index].value / 8) {
                        break;
                    }
                    i++;
                    *image = 0;
                    image++;
                }
                x = point.x;
                y = point.y;
                box.x = x;
                box.y = y;
                box.width = g_wldcore_window_render_records[level->render_index].width;
                world_menu_display_text_entry_mode_1(text_id, (s32)g_wldcore_window_image_buffer, (s32)&box);
                wldcore_window_load_image_record_to_vram(level->render_index, g_wldcore_window_image_buffer);
                world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
                world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
                world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
            }
            level->step = 0;
            level->phase = 0;
            g_wldcore_menu_result = 0x1C0;
            g_wldcore_screen_fade_state.flags[0] ^= 8;
            depth = g_wldcore_menu_stack_depth;
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count -= 3;
            g_wldcore_menu_stack_depth = depth - 1;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.first_window]
                .palette = 0;
            world_thread_resume(0xE);
            break;
        }
        return;
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0) {
        if (level->scroll < 3 && level->text_ids[level->scroll] != -1) {
            wldcore_window_draw_indexed_text((wldcore_window_text_table_t*)level, 4);
            level->scroll = level->scroll + 1;
            if (level->scroll == 3 || level->text_ids[level->scroll] == -1) {
                g_wldcore_window_records[level->upper_window].flags |= 0x10;
            } else {
                g_wldcore_window_records[level->upper_window].flags &= ~0x10;
            }
            g_wldcore_window_records[level->lower_window].flags &= ~0x10;
        }
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0 && level->scroll != 0) {
        wldcore_window_draw_indexed_text((wldcore_window_text_table_t*)level, 2);
        level->scroll = level->scroll - 1;
        if (level->scroll == 0) {
            g_wldcore_window_records[level->lower_window].flags |= 0x10;
        } else {
            g_wldcore_window_records[level->lower_window].flags &= ~0x10;
        }
        g_wldcore_window_records[level->upper_window].flags &= ~0x10;
    }
    if (!(g_wldcore_current_button_input & PSX_PAD_SQUARE)) {
        level->phase = 2;
    }
}
