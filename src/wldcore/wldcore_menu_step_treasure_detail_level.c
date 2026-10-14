#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/pad.h"

/* Handler of the saved-record preview level: the phases mirror the
 * unexplored-land detail handler at 0x8007e6f4.
 *
 * Phase 1 waits for the render record the 32-bit value at 0x08 names to leave its 0x100 state; no push sets
 * that 32-bit value.
 * Phase 2, once file loading finishes, uploads the preview TIM's CLUT, binds
 * the level's render record to the VRAM image with its right edge at x -0x3e
 * and centerd on y 0x10, starts the progressive upload at 0x8007f998 and
 * publishes the record index, position and image rectangle in
 * g_wldcore_scrollable_text_window (extra_render_index, extra_base, extra_bounds). Phase
 * 3 advances that upload one step per frame. Phase 0 tears the level down once the fade started through system flag 4
 * and WORLD thread 14 finish (then opens the saved record through 0x8007fbf0 or 0x8007bab8), restores the parent
 * list-window level and reopens the treasure list when the text window finishes, and on button 0x10 selects the saved
 * record named by g_wldcore_treasure_sound_novel_ids[value].
 *
 * The record index is staged in `saved` before the depth store: the target
 * reloads the full 32-bit value for the 16-bit store instead of narrowing the load.
 * The button path's byte uses its own local; sharing `slot` changes the
 * register allocation. */
void wldcore_menu_step_treasure_detail_level(wldcore_menu_variable_detail_level_t* level) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut_position;
    point32_t* base;
    s32 render;
    s32 slot;
    s32 depth;
    s32 saved;
    s32 entry;

    switch (level->phase) {
    case WLDCORE_DETAIL_PHASE_LOAD:
        if (!(g_wldcore_window_render_records[level->saved_record_index].flags & WORLD_RENDER_RECORD_POP_IN)) {
            level->phase = WLDCORE_DETAIL_PHASE_BUILD;
        }
        break;
    case WLDCORE_DETAIL_PHASE_BUILD:
        if (g_main_file_still_loading != 0) {
            break;
        }
        GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
        LoadClut(image.clut, image.cx, image.cy);
        render = level->render_index;
        bounds.x = image.px;
        bounds.y = image.py;
        bounds.w = image.pw;
        bounds.h = image.ph;
        clut_position.x = image.cx;
        clut_position.y = image.cy;
        wldcore_window_init_vram_render_record(render, bounds, clut_position, WORLD_RENDER_RECORD_8BIT_CLUT);
        g_wldcore_window_render_records[render].priority = 0xA;
        g_wldcore_window_render_records[render].flags &= ~(WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_POP_IN);
        base = &g_wldcore_window_render_records[render].base;
        base->x = -0x3E - image.pw;
        base->y = 0x10 - (image.ph >> 1);
        wldcore_gfx_step_dissolve_image_upload(&image, 0);
        g_wldcore_scrollable_text_window.extra_render_index = render;
        g_wldcore_scrollable_text_window.extra_base = *base;
        g_wldcore_scrollable_text_window.extra_bounds = g_wldcore_window_render_records[render].bounds;
        level->phase = level->phase + 1;
        break;
    case WLDCORE_DETAIL_PHASE_POP_IN:
        level->timer = level->timer + 1;
        GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
        if (wldcore_gfx_step_dissolve_image_upload(&image, level->timer) == 0) {
            level->phase = WLDCORE_DETAIL_PHASE_SHOWN;
        }
        break;
    default:
        if ((g_main_system_flags & (MAIN_SYSTEM_FLAG_INPUT_BLOCKED | MAIN_SYSTEM_FLAG_FADING))
                == MAIN_SYSTEM_FLAG_INPUT_BLOCKED
            && thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
            slot = level->saved_record_index;
            g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            g_wldcore_window_record_count -= 4;
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 6;
            saved = level->saved_record_index;
            g_wldcore_menu_stack_depth--;
            g_wldcore_saved_record_index = saved;
            if (slot < 5 && g_main_saved_records[slot].section != 0) {
                wldcore_menu_push_sound_novel_resume_level(level->value);
                break;
            }
            wldcore_menu_push_sound_novel_level(level->value);
            break;
        }
        if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) == 0) {
            depth = g_wldcore_menu_stack_depth;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.main_window].flags
                ^= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.side_window].flags
                ^= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
                ^= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
                |= WORLD_RENDER_RECORD_POP_IN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.upper_window].flags
                ^= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count--;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.lower_window].flags
                ^= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_menu_stack_depth = depth - 1;
            wldcore_list_open_treasures();
            break;
        }
        if (g_wldcore_input_new_button_presses & PSX_PAD_TRIANGLE) {
            entry = g_wldcore_treasure_sound_novel_ids[level->value];
            if (entry != 0) {
                level->saved_record_index = entry - 1;
                if (entry - 1 >= 5 || g_main_saved_records[entry - 1].section == 0) {
                    wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
                }
                g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
                thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
                wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            }
        }
        break;
    }
}
