#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Handler of the history-text level (type 0x17).
 *
 * Phase 1 steps the picture render record (g_wldcore_active_saved_record.picture_render_index) and g_wldcore_hud_color
 * from 0x80 down to 0x60 while the screen fade box brightens, in five steps. Phase 2 runs the reverse ramp and then, if
 * g_wldcore_active_saved_record.text_id names a text, clears the image buffer for the level's render record (width *
 * height / 8 32-bit values), renders text 0x8800 + g_wldcore_active_saved_record.text_id into it, uploads it and clears
 * script variables 0x5c-0x5e; it then pops the level and resumes WORLD thread 14. Phase 0 scrolls the text history with
 * up/down, hiding the arrow window at either end, and starts phase 2 once button bit 0x80 is released.
 *
 * Load-bearing spellings: the nonzero phases are a switch nested under
 * `phase != 0`, matching the target's test order; each fade case holds
 * &g_wldcore_active_saved_record.picture_render_index in its own pointer local, and `shade` is shared between the
 * cases (a global pseudo, as in the target's $a2); the text box position is
 * read into `x`/`y` so the s32 point is loaded with `lw`; `unused` preserves
 * the 0x58-byte frame. */
void wldcore_menu_step_sound_novel_history_level(wldcore_menu_sound_novel_history_level_t* level) {
    glyph_blit_t box;
    point32_t point;
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

    if (level->phase != WLDCORE_SOUND_NOVEL_HISTORY_PHASE_BROWSE) {
        switch (level->phase) {
        case WLDCORE_SOUND_NOVEL_HISTORY_PHASE_DIM:
            shade = 0x80 - level->step * 8;
            index1 = &g_wldcore_active_saved_record.picture_render_index;
            g_wldcore_window_render_records[*index1].color.r = shade;
            g_wldcore_window_render_records[*index1].color.g = shade;
            g_wldcore_window_render_records[*index1].color.b = shade;
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
                level->phase = WLDCORE_SOUND_NOVEL_HISTORY_PHASE_BROWSE;
            }
            break;
        case WLDCORE_SOUND_NOVEL_HISTORY_PHASE_RESTORE:
            index2 = &g_wldcore_active_saved_record.picture_render_index;
            shade = level->step * 0x10 + 0x40;
            g_wldcore_window_render_records[*index2].color.r = shade;
            g_wldcore_window_render_records[*index2].color.g = shade;
            g_wldcore_window_render_records[*index2].color.b = shade;
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
                text_id += TEXT_ID_WORLD_MESSAGE_BASE;
                i = 0;
                image = g_wldcore_window_image_buffer;
                point.x = 8;
                point.y = 8;
                for (;;) {
                    render_index = level->render_index;
                    if (i >= g_wldcore_window_render_records[render_index].bounds.w
                            * g_wldcore_window_render_records[render_index].bounds.h / 8) {
                        break;
                    }
                    i++;
                    *image = 0;
                    image++;
                }
                x = point.x;
                y = point.y;
                box.rect.x = x;
                box.rect.y = y;
                box.stride = g_wldcore_window_render_records[level->render_index].bounds.w;
                menu_display_text_entry_past_choice_marks(text_id, g_wldcore_window_image_buffer, &box);
                wldcore_window_load_image_record_to_vram(level->render_index, g_wldcore_window_image_buffer);
                script_set_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
                script_set_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
                script_set_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
            }
            level->step = 0;
            level->phase = WLDCORE_SOUND_NOVEL_HISTORY_PHASE_BROWSE;
            g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
            g_wldcore_screen_fade_state.flags[0] ^= WLDCORE_SCREEN_FADE_BOX_VISIBLE;
            depth = g_wldcore_menu_stack_depth;
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count -= 3;
            g_wldcore_menu_stack_depth = depth - 1;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.first_window]
                .palette = 0;
            thread_resume(WLDCORE_THREAD_SLOT_MESSAGE);
            break;
        }
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->scroll < 3 && level->text_ids[level->scroll] != -1) {
            wldcore_window_draw_indexed_text(level, 4);
            level->scroll = level->scroll + 1;
            if (level->scroll == 3 || level->text_ids[level->scroll] == -1) {
                g_wldcore_window_records[level->upper_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
            } else {
                g_wldcore_window_records[level->upper_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            }
            g_wldcore_window_records[level->lower_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        }
    }
    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0 && level->scroll != 0) {
        wldcore_window_draw_indexed_text(level, 2);
        level->scroll = level->scroll - 1;
        if (level->scroll == 0) {
            g_wldcore_window_records[level->lower_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
        } else {
            g_wldcore_window_records[level->lower_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        }
        g_wldcore_window_records[level->upper_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    }
    if (!(g_input_current_buttons & PSX_PAD_SQUARE)) {
        level->phase = WLDCORE_SOUND_NOVEL_HISTORY_PHASE_RESTORE;
    }
}
