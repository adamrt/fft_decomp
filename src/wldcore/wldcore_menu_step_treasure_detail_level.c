#include "fft/main_file.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/gpu.h"
#include "psx/gs.h"
#include "psx/pad.h"

extern wldcore_window_render_bounds16_t g_wldcore_scroll_text_extra_rect;

void world_gs_gettiminfo(u32* tim, GsIMAGE* image);
s32 wldcore_gfx_step_dissolve_image_upload(GsIMAGE* im, s32 step);
void wldcore_menu_push_sound_novel_level(s32 mode);
void wldcore_menu_push_sound_novel_resume_level(s32 mode);
void wldcore_list_open_treasures(void);

/* Handler of the saved-record preview level: the phases mirror the
 * unexplored-land detail handler at 0x8007e6f4.
 *
 * Phase 1 waits for the render record at field_08 to leave its 0x100 state.
 * Phase 2, once file loading finishes, uploads the preview TIM's CLUT, binds
 * the level's render record to the VRAM image with its right edge at x -0x3e
 * and centred on y 0x10, starts the progressive upload at 0x8007f998 and
 * publishes the record index, position and image rectangle at
 * g_wldcore_scroll_text_extra_render_index/g_wldcore_scroll_text_extra_position/g_wldcore_scroll_text_extra_rect. Phase
 * 3 advances that upload one step per frame. Phase 0 tears the level down once the fade started through system flag 4
 * and WORLD thread 14 finish (then opens the saved record through 0x8007fbf0 or 0x8007bab8), restores the parent
 * list-window level and reopens the treasure list when the text window finishes, and on button 0x10 selects the saved
 * record named by g_wldcore_treasure_sound_novel_ids[value].
 *
 * The record index is staged in `saved` before the depth store: the target
 * reloads the full word for the halfword store instead of narrowing the load.
 * The button path's byte uses its own local; sharing `slot` changes the
 * register allocation. */
void wldcore_menu_step_treasure_detail_level(wldcore_menu_variable_detail_level_t* level) {
    GsIMAGE image;
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t clut_position;
    wldcore_point32_t* base;
    s32 render;
    s32 slot;
    s32 depth;
    s32 saved;
    s32 entry;

    switch (level->phase) {
    case 1:
        if (!(g_wldcore_window_render_records[level->field_08].flags & 0x100)) {
            level->phase = 2;
        }
        break;
    case 2:
        if (g_main_file_still_loading != 0) {
            break;
        }
        world_gs_gettiminfo(g_wldcore_picture_buffer + 1, &image);
        LoadClut(image.clut, image.cx, image.cy);
        render = level->render_index;
        bounds.position.x = image.px;
        bounds.position.y = image.py;
        bounds.dimensions.x = image.pw;
        bounds.dimensions.y = image.ph;
        clut_position.x = image.cx;
        clut_position.y = image.cy;
        wldcore_window_init_vram_render_record(render, bounds.position, bounds.dimensions, clut_position, 4);
        g_wldcore_window_render_records[render].priority = 0xA;
        g_wldcore_window_render_records[render].flags &= ~0x110;
        base = (wldcore_point32_t*)&g_wldcore_window_render_records[render].base_x;
        base->x = -0x3E - image.pw;
        base->y = 0x10 - (image.ph >> 1);
        wldcore_gfx_step_dissolve_image_upload(&image, 0);
        g_wldcore_scroll_text_extra_render_index = render;
        g_wldcore_scroll_text_extra_position = *base;
        g_wldcore_scroll_text_extra_rect
            = *(wldcore_window_render_bounds16_t*)&g_wldcore_window_render_records[render].x;
        level->phase = level->phase + 1;
        break;
    case 3:
        level->timer = level->timer + 1;
        world_gs_gettiminfo(g_wldcore_picture_buffer + 1, &image);
        if (wldcore_gfx_step_dissolve_image_upload(&image, level->timer) == 0) {
            level->phase = 0;
        }
        break;
    default:
        if ((g_main_system_flags & 0xC) == 4 && world_thread_is_running(0xE) == 0) {
            slot = level->field_08;
            g_main_system_flags ^= 4;
            g_wldcore_window_record_count -= 4;
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 6;
            saved = level->field_08;
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
                ^= 0x10;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.side_window].flags
                ^= 0x10;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
                ^= 0x10;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
                |= 0x100;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.upper_window].flags
                ^= 0x10;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count--;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.lower_window].flags
                ^= 0x10;
            g_wldcore_menu_stack_depth = depth - 1;
            wldcore_list_open_treasures();
            break;
        }
        if (g_wldcore_new_button_presses & PSX_PAD_TRIANGLE) {
            entry = g_wldcore_treasure_sound_novel_ids[level->value];
            if (entry != 0) {
                level->field_08 = entry - 1;
                if (entry - 1 >= 5 || g_main_saved_records[entry - 1].section == 0) {
                    wldcore_fade_start_screen(2, 0x10);
                }
                g_main_system_flags |= 4;
                world_thread_set_parameters(0xE, 0, -1, 0);
                wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            }
        }
        break;
    }
}
