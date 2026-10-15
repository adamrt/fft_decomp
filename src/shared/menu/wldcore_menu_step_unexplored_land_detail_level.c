#include "fft/wldcore.h"
#include "psx/libgs.h"

/* Handler of the unexplored-land detail level (type 0x19).
 *
 * Phase 1 waits for the render record the 32-bit value at 0x08 names to leave its 0x100 state; no push sets
 * that 32-bit value.
 * Phase 2, once file loading finishes, uploads the land picture's TIM pixels
 * and CLUT, binds the level's render record to the VRAM image at (-116, -34)
 * and makes it the scrollable text window's extra render record (position and
 * image rectangle). Phase 3 waits for that render record. Phase 0 waits for
 * the text window to finish, restores the parent list-window level's windows
 * and frame render record, pops the level and reopens the unexplored-land
 * list.
 *
 * The TIM widths are read unsigned (`lhu`), as Psy-Q's GsIMAGE declares
 * pw/ph. The text window's base and image rectangle are each copied as one
 * aggregate: the target loads both 32-bit values before either store. */
void wldcore_menu_step_unexplored_land_detail_level(wldcore_menu_variable_detail_level_t* level) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut_position;
    point32_t* base;
    s32* index;
    s32 render;
    s32 depth;

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
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
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
        base = &g_wldcore_window_render_records[render].base;
        base->x = -0x74;
        base->y = -0x22;
        g_wldcore_window_render_records[render].flags
            = (g_wldcore_window_render_records[render].flags & ~WORLD_RENDER_RECORD_HIDDEN)
            | WORLD_RENDER_RECORD_POP_IN;
        g_wldcore_scrollable_text_window.extra_render_index = render;
        g_wldcore_scrollable_text_window.extra_base = *base;
        g_wldcore_scrollable_text_window.extra_bounds = g_wldcore_window_render_records[render].bounds;
        level->phase = level->phase + 1;
        break;
    case WLDCORE_DETAIL_PHASE_POP_IN:
        if (!(g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN)) {
            level->phase = WLDCORE_DETAIL_PHASE_SHOWN;
        }
        break;
    default:
        index = &g_wldcore_scrollable_text_window.render_record_index;
        if (g_wldcore_window_render_records[*index].flags & WORLD_RENDER_RECORD_POP_IN) {
            break;
        }
        if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) != 0) {
            break;
        }
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
        wldcore_list_open_unexplored_lands();
        break;
    }
}
