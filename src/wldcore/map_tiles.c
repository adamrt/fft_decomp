/* Calendar month and day, the captured world frame, the map tile atlas, tile projection and culling, and clamped
 * scrolling. */

#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/unit.h"
#include "fft/window.h"
#include "fft/wldcore.h"
#include "fft/wldcore_map.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_proposition.h"
#include "fft/wldcore_script.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* 0-based day-of-year index (party_unit_t.birthday_day) to 1-based month
 * and day. */
void wldcore_calendar_calculate_month_and_day(s32 day_of_year, s32* output_month, s32* output_day) {
    s32 month = 0;
    s32 days_in_month;

    while (1) {
        days_in_month = g_unit_birthday_month_lengths[month];
        if (day_of_year < days_in_month) {
            break;
        }
        month += 1;
        day_of_year -= days_in_month;
        if (month >= 12) {
            break;
        }
    }
    *output_month = month + 1;
    *output_day = day_of_year + 1;
}

/* World render/present pass with the 3D map, plus the menu-window handoff.
 *
 * With `capture` == 0 it only drops the 0x10 visibility bit on the two active
 * window records and restores the saved system flags; otherwise it renders a
 * whole frame into both ordering tables, stores the composed image into the
 * scratch buffer, and re-arms the menu windows behind flag mask 0x3c20. */
void wldcore_gfx_toggle_captured_world_frame(s32 capture) {
    RECT display_rect;
    s32 buffer_index;

    if (capture == 0) {
        s32 map_window_index;
        s32 menu_window_index;

        map_window_index = g_wldcore_map_window_index;
        menu_window_index = g_wldcore_menu_stack_types[0];
        g_wldcore_window_records[map_window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_main_system_flags = g_wldcore_saved_system_flags | MAIN_SYSTEM_FLAG_DRAW_MAP;
        g_wldcore_window_records[menu_window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        return;
    }

    DrawSync(0);
    VSync(0);
    GsSwapDispBuff();
    buffer_index = GsGetActiveBuff();
    g_active_graphics_buffer_index = buffer_index;
    GsSetWorkBase(g_wldcore_gfx_world_primitive_buffers + buffer_index * 0xE000);
    GsClearOt(0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_map_project_and_cull_tiles();
    wldcore_gfx_draw_projected_map_tiles(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_draw_visible_routes(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_project_and_cull_dots();
    wldcore_map_update_camera_direction_and_projection();
    wldcore_gfx_draw_context_value_display(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_draw_calendar_date(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_proposition_submit_marker_request(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    window_dispatch_record_list(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index],
        g_wldcore_window_aux_render_object_queue, g_wldcore_window_aux_render_object_count);
    window_dispatch_record_list(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index],
        g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    wldcore_restore_previous_stack();
    GsSortClear(0, 0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    GsDrawOt(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    GsDrawOt(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    DrawSync(0);
    wldcore_gfx_set_display_rect(g_active_graphics_buffer_index, &display_rect);
    StoreImage(&display_rect, g_wldcore_scratch_buffer);
    DrawSync(0);
    g_wldcore_loaded_background_set = -1;
    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_menu_run_world_frame(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    {
        s32 map_window_index;
        s32 menu_window_index;
        u32 saved_system_flags;

        saved_system_flags = g_main_system_flags;
        menu_window_index = g_wldcore_menu_stack_types[0];
        map_window_index = g_wldcore_map_window_index;
        g_wldcore_saved_system_flags = saved_system_flags;
        g_wldcore_window_records[map_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_main_system_flags = saved_system_flags | MAIN_SYSTEM_FLAG_HIDE_MAP_ROUTES
            | MAIN_SYSTEM_FLAG_HIDE_DISPLAY_OBJECTS | MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE
            | MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
        g_wldcore_window_records[menu_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        window_dispatch_record_list(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index],
            g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    }
    GsDrawOt(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    DrawSync(0);
}

/* World-map tile atlas setup, run once from wldcore_init_subsystems between
 * window_init_record_pools() and
 * wldcore_system_init_world_menus_and_date(). Because the pools were just
 * zeroed, the two append calls here return records 0 and 1.
 *
 * Layout:
 *  - Origin, scale and rotation form g_wldcore_map_projection_view at
 *    0x8009f2c0. Their aliases retain this initializer's absolute stores;
 *    the projection routine anchors its shared record view at rotation.
 *  - The 24-byte tile source table is reached through the pointer cell
 *    g_wldcore_map_tile_descriptors. The target indexes it by 32-bit value
 *    (base + k*4, k += 6), not by record, so it is an s32* with a 32-bit element index.
 *  - Element 0 of g_wldcore_menu_stack_types (0x800d4580) receives the second
 *    window-record index; why a record index is stored there is unknown.
 *
 * Shapes the bytes depend on:
 *  - `.map_x`/`.map_y` are written through one loop-invariant s32* to element
 *    0's `.map_x`, as pos[i * 6] / pos[i * 6 + 1]. That makes the address a
 *    strength_reduce giv, initialized at the row head with the target's
 *    lui/addiu/addu triple and carried with += 24. Two array-form stores each
 *    pay a full lui/addiu/addu/sw site; a source-level induction variable
 *    hoists the init into the outer preheader and is one instruction short.
 *  - The window block uses the same mixed spelling: `.x`/`.y` through one
 *    handle and `.anim_counter` off the record base, while
 *    `.sequence`/`.priority`/`.frame_index` stay array-form.
 *  - A separate index local per append call deletes two `move v1,v0` copies.
 *  - Each window-record group has its own pointer pair (wpos/rec and
 *    wpos2/rec2). One shared pair is long-lived and materializes both
 *    addresses into a2/a3 ahead of the first call; split pairs are short-lived
 *    temps, giving the target's per-group `addu v0,v1,s0` / `addu v0,v1,s1`
 *    while s0 (the `.x` column) and s1 (the record base, CSE'd as
 *    `addiu s1,s0,-0x18`) stay live across both groups.
 *  - `y` is initialized before `k` and `i`: the target emits `li t3,-192`
 *    first in the outer preheader.
 */
void wldcore_map_init_tile_atlas(void) {
    s32* descriptor;
    point32_t* wpos;
    point32_t* wpos2;
    world_animated_render_record_t* rec;
    world_animated_render_record_t* rec2;
    s32* pos;
    s32 map_window_index;
    s32 menu_window_index;
    s32 k;
    s32 i;
    s32 row;
    s32 col;
    s32 x;
    s32 y;
    s32 width;
    s32 height;
    s32 clut_x;
    s32 clut_y;
    s32 tpage_x;
    s32 tpage_y;

    g_wldcore_map_projection_scale.vx = ONE;
    g_wldcore_map_projection_scale.vy = ONE;
    g_wldcore_map_projection_scale.vz = ONE;
    g_wldcore_map_projection_origin.vx = 0;
    g_wldcore_map_projection_origin.vy = 0;
    g_wldcore_map_projection_origin.vz = 0;
    g_wldcore_map_projection_rotation.vx = 0;
    g_wldcore_map_projection_rotation.vy = 0;
    g_wldcore_map_projection_rotation.vz = 0;

    descriptor = g_wldcore_map_tile_descriptors;
    if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_RESTART)) {
        y = -192;
        k = 0;
        i = 0;
        pos = &g_wldcore_map_projected_tiles[0].map_x;
        for (row = 0; row < 13; row++) {
            x = -256;
            for (col = 0; col < 17; col++) {
                tpage_x = descriptor[k];
                tpage_y = descriptor[k + 1];
                clut_x = descriptor[k + 2];
                clut_y = descriptor[k + 3];
                width = descriptor[k + 4];
                height = descriptor[k + 5];
                pos[i * 6] = x;
                pos[i * 6 + 1] = y;
                if (width != 0 && height != 0) {
                    g_wldcore_map_projected_tiles[i].flags = GetTPage(1, 0, tpage_x, tpage_y) & 0xff;
                    g_wldcore_map_projected_tiles[i].texture
                        = (clut_x << 24) | ((clut_y & 0xff) << 16) | ((width & 0xff) << 8) | (height & 0xff);
                } else {
                    g_wldcore_map_projected_tiles[i].flags = WLDCORE_MAP_TILE_EMPTY;
                }
                i++;
                x += width;
                k += 6;
            }
            y += height;
        }
    }

    map_window_index
        = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_map_window_index = map_window_index;
    g_wldcore_window_records[map_window_index].sequence = WLDCORE_ANIM_SEQUENCE_MAP_FRAME;
    g_wldcore_window_records[map_window_index].priority = 13;
    wpos = &g_wldcore_window_records[map_window_index].position;
    rec = &g_wldcore_window_records[map_window_index];
    wpos->x = 0;
    wpos->y = 8;
    rec->anim_counter = 0;
    g_wldcore_window_records[map_window_index].frame_index = 0;

    menu_window_index
        = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_types[0] = menu_window_index;
    g_wldcore_window_records[menu_window_index].sequence = WLDCORE_ANIM_SEQUENCE_ROOT_MENU_FRAME;
    g_wldcore_window_records[menu_window_index].priority = 13;
    wpos2 = &g_wldcore_window_records[menu_window_index].position;
    rec2 = &g_wldcore_window_records[menu_window_index];
    wpos2->x = -4;
    wpos2->y = 8;
    rec2->anim_counter = 0;
    g_wldcore_window_records[menu_window_index].frame_index = 0;

    wldcore_map_project_and_cull_tiles();
}

/* Builds the map camera matrix and projects the 13x17 grid of tile corners,
 * storing each screen position and setting flag 0x100 on corners that fall
 * outside the visible window; empty tiles (flag 0x200) keep their flags. */
void wldcore_map_project_and_cull_tiles(void) {
    MATRIX matrix;
    SVECTOR position;
    VECTOR projected;
    long flag;
    s32 i;
    wldcore_projected_entry_t* entry;
    wldcore_map_projection_view_t* view;
    SVECTOR* rotation;
    s32 flags;
    s32 updated;
    s32 coordinate;

    /* Retail keeps the camera base at rotation; the record base changes its argument instructions. */
    rotation = &g_wldcore_map_projection_rotation;
    view = (wldcore_map_projection_view_t*)((u8*)rotation - offsetof(wldcore_map_projection_view_t, rotation));
    RotMatrix(rotation, &matrix);
    TransMatrix(&matrix, &view->origin);
    ScaleMatrix(&matrix, &view->scale);
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);

    i = 0;
    entry = g_wldcore_map_projected_tiles;
    for (; i < 221; i++) {
        coordinate = g_wldcore_map_projected_tiles[i].map_x;
        position.vx = coordinate;
        coordinate = g_wldcore_map_projected_tiles[i].map_y;
        position.vz = 0;
        position.vy = coordinate;
        RotTrans(&position, &projected, &flag);
        g_wldcore_map_projected_tiles[i].screen_x = projected.vx;
        g_wldcore_map_projected_tiles[i].screen_y = projected.vy;
        flags = entry->flags;
        if (!(flags & WLDCORE_MAP_TILE_EMPTY)) {
            if (projected.vx < -0xa0 || projected.vx > 0x80 || projected.vy < -0x98 || projected.vy > 0x78) {
                updated = flags | WLDCORE_MAP_TILE_OFF_SCREEN;
            } else {
                updated = flags & ~WLDCORE_MAP_TILE_OFF_SCREEN;
            }
            entry->flags = updated;
        }
        entry++;
    }
}

/* Needs aspsx 2.21: four sites that reach the typed global array keep their
 * `addiu at,at,<offset>` under 2.21 and lose it under 2.34.
 *
 * Walks g_wldcore_map_projected_tiles, the table that
 * wldcore_map_project_and_cull_tiles fills: each tile reads its own entry's
 * flags, texture and .screen_x/.screen_y, and the .screen_x of the next
 * entry, so the accesses are written against `index` and `index + 1`.
 *
 * The 32-bit texture value packs u, v, width and height from the top byte down; a
 * width or height that would run past texel 255 is shortened by one. */

#define WLDCORE_MAP_TILE_ROWS    12
#define WLDCORE_MAP_TILE_COLUMNS 16
#define WLDCORE_MAP_ROW_STRIDE   17
#define WLDCORE_MAP_DOT_CLUT_Y   0x1e0

/* Sorts the world map's projected tile grid into `ot` as textured quads.
 * Each tile spans from its own projected corner to the next column's x and
 * the next row's y; a tile flagged off screen or empty is skipped. All tiles
 * share the map CLUT and the current screen tint. */
void wldcore_gfx_draw_projected_map_tiles(GsOT* ot) {
    POLY_FT4 poly;
    CVECTOR color;
    s32 row;
    s32 column;
    s32 index;

    SetPolyFT4(&poly);
    color.r = 0x80;
    color.g = 0x80;
    color.b = 0x80;
    wldcore_gfx_copy_color_with_tint(&color, &color);
    setRGB0(&poly, color.r, color.g, color.b);
    poly.clut = GetClut(0, WLDCORE_MAP_DOT_CLUT_Y);

    for (row = 0; row < WLDCORE_MAP_TILE_ROWS; row++) {
        index = row * WLDCORE_MAP_ROW_STRIDE;

        poly.y0 = poly.y1 = g_wldcore_map_projected_tiles[index].screen_y;
        poly.y2 = poly.y3 = g_wldcore_map_projected_tiles[index + WLDCORE_MAP_ROW_STRIDE].screen_y;

        for (column = 0; column < WLDCORE_MAP_TILE_COLUMNS; column++, index++) {
            s32 flags = g_wldcore_map_projected_tiles[index].flags;
            u32 texture;
            s32 u;
            s32 v;
            s32 w;
            s32 h;

            if (flags & (WLDCORE_MAP_TILE_OFF_SCREEN | WLDCORE_MAP_TILE_EMPTY)) {
                continue;
            }
            poly.tpage = flags & WLDCORE_MAP_TILE_TPAGE_MASK;
            poly.x0 = poly.x2 = g_wldcore_map_projected_tiles[index].screen_x;
            poly.x1 = poly.x3 = g_wldcore_map_projected_tiles[index + 1].screen_x;

            texture = g_wldcore_map_projected_tiles[index].texture;
            u = (texture & 0xff000000) >> 24;
            v = (texture & 0x00ff0000) >> 16;
            w = (texture & 0x0000ff00) >> 8;
            if (u + w >= 0x100) {
                w--;
            }
            h = texture & 0x000000ff;
            if (v + h >= 0x100) {
                h--;
            }
            poly.u0 = poly.u2 = u;
            poly.v0 = poly.v1 = v;
            poly.u1 = poly.u3 = u + w;
            poly.v2 = poly.v3 = v + h;
            GsSortPoly(&poly, ot, 2);
        }
    }
}

/* Target 0x8006abc8. */
void wldcore_map_apply_clamped_scroll(point32_t* amount, VECTOR* position) {
    wldcore_map_apply_clamped_horizontal_scroll(&amount->x, &position->vx);
    wldcore_map_apply_clamped_vertical_scroll(amount, position);
}

/* Apply a (negated) scroll amount to a position clamped to [-116, 128] (the
 * same x range wldcore_map_negate_and_clamp_coordinates clamps to) and leave the unapplied
 * remainder in *amount. */
void wldcore_map_apply_clamped_horizontal_scroll(s32* amount, s32* position) {
    s32 delta = -(*amount);
    s32 old_position;
    s32 value;

    (*amount) = delta;
    if (delta < 0) {
        old_position = *position;
        value = old_position + delta;
        if (value < WLDCORE_SCROLL_MIN_X) {
            value = WLDCORE_SCROLL_MIN_X;
        }
        (*amount) = delta - (value - old_position);
        (*position) = value;
    }

    if ((*amount) > 0) {
        old_position = *position;
        value = old_position + (*amount);
        if (value >= (WLDCORE_SCROLL_MAX_X + 1)) {
            value = WLDCORE_SCROLL_MAX_X;
        }
        (*amount) -= value - old_position;
        (*position) = value;
    }

    (*amount) = -(*amount);
}

/* Vertical counterpart of wldcore_map_apply_clamped_horizontal_scroll at
 * 0x8006ac08: the same shape applied to the vertical amount and projection origin,
 * clamped to [-64, 80], the y range wldcore_map_negate_and_clamp_coordinates uses. */
void wldcore_map_apply_clamped_vertical_scroll(point32_t* amount, VECTOR* position) {
    s32 delta = -amount->y;
    s32 old_position;
    s32 value;

    amount->y = delta;
    if (delta < 0) {
        old_position = position->vy;
        value = old_position + delta;
        if (value < WLDCORE_SCROLL_MIN_Y) {
            value = WLDCORE_SCROLL_MIN_Y;
        }
        amount->y = delta - (value - old_position);
        position->vy = value;
    }

    if (amount->y > 0) {
        old_position = position->vy;
        value = old_position + amount->y;
        if (value >= (WLDCORE_SCROLL_MAX_Y + 1)) {
            value = WLDCORE_SCROLL_MAX_Y;
        }
        amount->y -= value - old_position;
        position->vy = value;
    }

    amount->y = -amount->y;
}
