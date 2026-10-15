#include "fft/menu.h"
#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/pad.h"

/* Pushes the town/location service list level (type 4) for menu id `menu`.
 *
 * Loads the location's picture, appends the cursor window (window kind
 * menu + 0x37) and, when a picture is present, uploads both its TIM and the
 * script picture at g_wldcore_picture_buffer to VRAM, binds a render record to the image
 * and adds a second framing window at (-8, 0x10). It then measures every
 * entry's text through text_find_entry / text_measure_pixels to
 * size the panel, copies the entry values into g_wldcore_list_entry_values with
 * cleared row flags, clamps the cursor to at most four visible rows, and
 * starts the panel render state centerd on the measured width.
 *
 * The identical `menu += 0x37` arms split the block for the scheduler (jump2
 * cross-jumps them back together), keeping the add after the picture call so
 * reorg puts it in the wait call's delay slot. The label window's position is
 * stored through a stride-36 view based at &records->x, as the target forms
 * (records + 0x18) + index * 36. The unused array reproduces the 0xb8 frame.
 */
void wldcore_menu_push_location_menu_level(s32 menu) {
    GsIMAGE image;
    RECT bounds;
    point32_t origin;
    wldcore_window_panel_layout_t layout;
    s16 text_width;
    s16 text_rows;
    s32 index;
    s32 count;
    s32 depth;
    s16 widest;
    s32 value;
    s32 kind;
    u32 flags;
    s32 i;
    CVECTOR* color;
    world_animated_render_record_t* records;
    wldcore_location_list_level_t* level;
    point32_t* base;
    wldcore_window_xy_t* position;

    g_wldcore_map_projection_state.marker.sub_kind
        = wldcore_proposition_load_picture(g_wldcore_map_projection_state.marker.kind);
    if (g_wldcore_map_projection_state.marker.sub_kind != 0) {
        menu += 0x37;
    } else {
        menu += 0x37;
    }
    file_wait_for_pending_variant();
    flags = g_main_system_flags | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    level = &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list;
    level->extra_render = -1;
    level->label_window = -1;
    level->frame_render = -1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.cursor_window = -1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.mode = 0;
    g_main_system_flags = flags;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.cursor_window = index;
    records = g_wldcore_window_records;
    /* Direct record.color access changes the column base and instruction scheduling. */
    color = &records->color;
    g_wldcore_window_records[index].priority = 0xB;
    g_wldcore_window_records[index].sequence = menu;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    ((CVECTOR*)((u8*)color + index * sizeof(g_wldcore_window_records[0])))->r = 0;
    ((CVECTOR*)((u8*)color + index * sizeof(g_wldcore_window_records[0])))->g = 0;
    ((CVECTOR*)((u8*)color + index * sizeof(g_wldcore_window_records[0])))->b = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.mode = 8;

    if (g_wldcore_map_projection_state.marker.sub_kind != 0) {
        GsGetTimInfo(g_wldcore_location_frame_tim + 1, &image);
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
        LoadClut(image.clut, image.cx, image.cy);
        GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
        LoadClut(image.clut, image.cx, image.cy);

        index
            = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.frame_render = index;
        bounds.x = image.px;
        bounds.y = image.py;
        bounds.w = image.pw;
        bounds.h = image.ph;
        origin.x = image.cx;
        origin.y = image.cy;
        wldcore_window_init_vram_render_record(index, bounds, origin, WORLD_RENDER_RECORD_8BIT_CLUT);
        base = &g_wldcore_window_render_records[index].base;
        g_wldcore_window_render_records[index].priority = 0xB;
        base->x = -0x40;
        base->y = -0x4C;
        g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;

        index = window_append_animated_record_variant(
            g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.label_window = index;
        g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_LOCATION_PICTURE_BORDER;
        g_wldcore_window_records[index].priority = 0xB;
        records[index].anim_counter = 0;
        g_wldcore_window_records[index].frame_index = 0;
        position = (wldcore_window_xy_t*)&records->position.x;
        position[index].x = -8;
        position[index].y = 0x10;
        ((CVECTOR*)((u8*)color + index * sizeof(g_wldcore_window_records[0])))->r = 0;
        ((CVECTOR*)((u8*)color + index * sizeof(g_wldcore_window_records[0])))->g = 0;
        ((CVECTOR*)((u8*)color + index * sizeof(g_wldcore_window_records[0])))->b = 0;
    }

    widest = 0;
    count = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.entry_count;
    for (i = 0; i < count; i++) {
        text_measure_pixels(&text_width, &text_rows,
            text_find_entry(g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.entries[i]));
        if (widest < text_width) {
            widest = text_width;
        }
    }
    widest += 8;

    for (i = 0; i < count; i++) {
        value = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.entries[i];
        g_wldcore_list_entry_values[i] = value;
        g_wldcore_list_row_flags[i] = 0;
    }

    i = 4;
    if (count < 4) {
        i = count;
    }
    layout.text_width = widest;
    layout.visible_rows = i;
    origin.x = 0x80 - (((layout.text_width + 0x18) & 0xFFFC) >> 1);
    if (g_wldcore_map_projection_state.marker.sub_kind != 0) {
        origin.y = 0x80;
    } else {
        origin.y = 0x70 - i * 8;
    }
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_LOCATION_MENU, count, i);
    wldcore_window_init_panel_render_state(
        0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.entry_count, &layout, &origin);
    g_wldcore_window_panel_render_state.vram_origin.x = 0x240;
    g_wldcore_window_panel_render_state.header_id = 0;
    g_wldcore_window_panel_render_state.vram_origin.y = 0x100;

    if (g_wldcore_map_projection_state.marker.sub_kind != 0) {
        s32 unused[6]; /* frame padding: the target frame is 0xb8 */

        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                     .location_list.cursor_window]
            .position.x = -4;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                     .location_list.cursor_window]
            .position.y = 0;
    } else {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                     .location_list.cursor_window]
            .position.x = -4;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                     .location_list.cursor_window]
            .position.y = g_wldcore_window_render_records[index].base.y - 8;
    }

    depth = g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_stack_records_next[depth].location_list.delay = 8;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_FREEZE_ANIMATION;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_LOCATION_MENU;
    g_wldcore_menu_stack_depth = depth + 1;
}
