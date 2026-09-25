#include "fft/wldcore.h"
#include "psx/gs.h"

void world_gs_gettiminfo(u32* tim, GsIMAGE* image);
void wldcore_list_open_unexplored_lands(void);

/* Handler of the unexplored-land detail level (type 0x19).
 *
 * Phase 1 waits for the render record at field_08 to leave its 0x100 state.
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
 * aggregate: the target loads both words before either store. */
void wldcore_menu_step_unexplored_land_detail_level(wldcore_menu_variable_detail_level_t* level) {
    GsIMAGE image;
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t clut_position;
    wldcore_point32_t* base;
    s32* index;
    s32 render;
    s32 depth;

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
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
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
        base = (wldcore_point32_t*)&g_wldcore_window_render_records[render].base_x;
        base->x = -0x74;
        base->y = -0x22;
        g_wldcore_window_render_records[render].flags = (g_wldcore_window_render_records[render].flags & ~0x10) | 0x100;
        g_wldcore_scrollable_text_window.extra_render_index = render;
        *(wldcore_point32_t*)&g_wldcore_scrollable_text_window.extra_base_x = *base;
        *(wldcore_window_render_bounds16_t*)&g_wldcore_scrollable_text_window.extra_x
            = *(wldcore_window_render_bounds16_t*)&g_wldcore_window_render_records[render].x;
        level->phase = level->phase + 1;
        break;
    case 3:
        if (!(g_wldcore_window_render_records[level->render_index].flags & 0x100)) {
            level->phase = 0;
        }
        break;
    default:
        index = &g_wldcore_scroll_text_render_record_index;
        if (g_wldcore_window_render_records[*index].flags & 0x100) {
            break;
        }
        if (wldcore_text_is_window_finished((wldcore_text_scrollable_window_t*)((u8*)index - 0x44)) != 0) {
            break;
        }
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.main_window].flags ^= 0x10;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.side_window].flags ^= 0x10;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
            ^= 0x10;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
            |= 0x100;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.upper_window].flags ^= 0x10;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count--;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.lower_window].flags ^= 0x10;
        g_wldcore_menu_stack_depth = depth - 1;
        wldcore_list_open_unexplored_lands();
        break;
    }
}
