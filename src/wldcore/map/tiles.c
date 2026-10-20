/* The captured world-map frame and the tile atlas, projection, culling and clamped map scrolling. */

#include "fft/calendar.h"
#include "fft/display_object.h"
#include "fft/gfx.h"
#include "fft/location.h"
#include "fft/system.h"
#include "fft/world_map.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Values of world_map_projected_tile_t.flags. */
typedef enum world_map_tile_flag {
    WORLD_MAP_TILE_TPAGE_MASK = 0x0ff,
    WORLD_MAP_TILE_OFF_SCREEN = 0x100, /* set by the projection pass */
    WORLD_MAP_TILE_EMPTY = 0x200       /* the atlas has no tile here */
} world_map_tile_flag_e;

/* Provisional: an entry of the projected-tile table (g_world_map_projected_tiles), filled by
 * world_map_tile_atlas_init and projected by the RotTrans pass of world_map_project_tiles. */
typedef struct world_map_projected_tile {
    s32 flags;        /* 0x00; world_map_tile_flag_e */
    s32 texture;      /* 0x04; packed texture rectangle: u << 24 | v << 16 | w << 8 | h */
    point32_t map;    /* 0x08; tile corner before projection, copied to RotTrans vx/vy */
    point32_t screen; /* 0x10; projected position, culled before conversion to GPU coordinates */
} world_map_projected_tile_t;
typedef char world_map_projected_tile_size_must_be_24[sizeof(world_map_projected_tile_t) == 24 ? 1 : -1];

/* g_world_map_flags as saved by world_map_backdrop_mode_set (world_map_flags_e). */
extern u32 g_world_map_saved_system_flags;
extern world_map_projected_tile_t g_world_map_projected_tiles[];

static VECTOR g_world_map_projection_scale = { 0 };
static SVECTOR g_world_map_projection_rotation = { 0 };

/* The map is a grid of 12x16 tiles, projected through its 13x17 corners. */
#define WORLD_MAP_TILE_ROWS    12
#define WORLD_MAP_TILE_COLUMNS 16
#define WORLD_MAP_ROW_STRIDE   17
#define WORLD_MAP_CORNER_COUNT ((WORLD_MAP_TILE_ROWS + 1) * WORLD_MAP_ROW_STRIDE)
#define WORLD_MAP_TILE_CLUT_Y  0x1e0

/* Freeze the world map behind a full-screen menu, or release it.
 *
 * With `capture` set it draws one whole map frame into both ordering tables,
 * stores the image in g_world_map_scratch_buffer as the backdrop and draws the
 * menu frame over it; then it saves the system flags and hides the map and
 * root menu windows and everything the map pass draws (routes, display
 * objects, context value, date and marker). With `capture` 0 the two windows
 * are shown again and the saved flags come back with WORLD_MAP_FLAG_DRAW_MAP
 * set, so the next frame redraws the map. */
void world_map_backdrop_mode_set(s32 capture) {
    RECT display_rect;
    s32 buffer_index;

    if (capture == 0) {
        s32 map_window_index;
        s32 menu_window_index;

        map_window_index = g_world_map_window_index;
        menu_window_index = g_world_map_cursor_window;
        g_display_object_animated_pool[map_window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_world_map_flags = g_world_map_saved_system_flags | WORLD_MAP_FLAG_DRAW_MAP;
        g_display_object_animated_pool[menu_window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
        return;
    }

    DrawSync(0);
    VSync(0);
    GsSwapDispBuff();
    buffer_index = GsGetActiveBuff();
    g_gfx_active_buffer_index = buffer_index;
    GsSetWorkBase(g_world_map_primitive_buffers + buffer_index * 0xE000);
    GsClearOt(0, 0, &g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsClearOt(0, 0, &g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_switch_to_stack(getScratchAddr(255));
    world_map_project_tiles();
    world_map_tiles_draw(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    world_map_routes_draw(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    world_map_project_dots();
    world_map_marker_update();
    hud_value_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    calendar_date_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    location_label_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    display_object_gs_list_draw(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index], g_display_object_aux_queue,
        g_display_object_aux_queue_count);
    display_object_gs_list_draw(
        &g_world_map_ordering_tables[g_gfx_active_buffer_index], g_display_object_queue, g_display_object_queue_count);
    world_map_restore_previous_stack();
    GsSortClear(0, 0, 0, &g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsDrawOt(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsDrawOt(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    DrawSync(0);
    world_map_display_rect_set(g_gfx_active_buffer_index, &display_rect);
    StoreImage(&display_rect, g_world_map_scratch_buffer);
    DrawSync(0);
    g_picture_background_loaded_set = -1;
    GsClearOt(0, 0, &g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_menus_update(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    {
        s32 map_window_index;
        s32 menu_window_index;
        u32 saved_system_flags;

        saved_system_flags = g_world_map_flags;
        menu_window_index = g_world_map_cursor_window;
        map_window_index = g_world_map_window_index;
        g_world_map_saved_system_flags = saved_system_flags;
        g_display_object_animated_pool[map_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_world_map_flags = saved_system_flags | WORLD_MAP_FLAG_HIDE_MAP_ROUTES | WORLD_MAP_FLAG_HIDE_DISPLAY_OBJECTS
            | WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE | WORLD_MAP_FLAG_HIDE_CALENDAR_DATE
            | WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
        g_display_object_animated_pool[menu_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        display_object_gs_list_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index], g_display_object_queue,
            g_display_object_queue_count);
    }
    GsDrawOt(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    DrawSync(0);
}

/* World-map tile atlas setup, run once from subsystems_init between
 * display_object_pools_init() and
 * world_map_menu_state_init(). Because the allocation counters were just
 * reset, the two append calls here return records 0 and 1.
 *
 * Layout:
 *  - Origin, scale and rotation form g_world_map_projection_view. Their
 *    scale and rotation retain their initialized-data definitions;
 *    the projection routine anchors its shared record view at rotation.
 *  - The 24-byte tile source table is reached through the pointer cell
 *    g_world_map_tile_descriptors. The target indexes it by 32-bit value
 *    (base + k*4, k += 6), not by record, so it is an s32* with a 32-bit element index.
 *  - g_world_map_cursor_window receives the root menu frame's record index
 *    and precedes the menu-stack type array. The root level has depth 1
 *    and occupies physical type slot 0.
 *
 * Shapes the bytes depend on:
 *  - `.map.x`/`.map.y` are written through one loop-invariant s32* to element
 *    0's `.map.x`, as pos[i * 6] / pos[i * 6 + 1]. That makes the address a
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
void world_map_tile_atlas_init(void) {
    s32* descriptor;
    point32_t* wpos;
    point32_t* wpos2;
    display_object_animated_t* rec;
    display_object_animated_t* rec2;
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
    s32 u;
    s32 v;
    s32 tpage_x;
    s32 tpage_y;

    g_world_map_projection_scale.vx = ONE;
    g_world_map_projection_scale.vy = ONE;
    g_world_map_projection_scale.vz = ONE;
    g_world_map_projection_view.origin.vx = 0;
    g_world_map_projection_view.origin.vy = 0;
    g_world_map_projection_view.origin.vz = 0;
    g_world_map_projection_rotation.vx = 0;
    g_world_map_projection_rotation.vy = 0;
    g_world_map_projection_rotation.vz = 0;

    descriptor = g_world_map_tile_descriptors;
    if (!(g_world_map_flags & WORLD_MAP_FLAG_RESTART)) {
        y = -192;
        k = 0;
        i = 0;
        pos = &g_world_map_projected_tiles[0].map.x;
        for (row = 0; row < WORLD_MAP_TILE_ROWS + 1; row++) {
            x = -256;
            for (col = 0; col < WORLD_MAP_ROW_STRIDE; col++) {
                tpage_x = descriptor[k];
                tpage_y = descriptor[k + 1];
                u = descriptor[k + 2];
                v = descriptor[k + 3];
                width = descriptor[k + 4];
                height = descriptor[k + 5];
                pos[i * 6] = x;
                pos[i * 6 + 1] = y;
                if (width != 0 && height != 0) {
                    g_world_map_projected_tiles[i].flags = GetTPage(1, 0, tpage_x, tpage_y) & 0xff;
                    g_world_map_projected_tiles[i].texture
                        = (u << 24) | ((v & 0xff) << 16) | ((width & 0xff) << 8) | (height & 0xff);
                } else {
                    g_world_map_projected_tiles[i].flags = WORLD_MAP_TILE_EMPTY;
                }
                i++;
                x += width;
                k += 6;
            }
            y += height;
        }
    }

    map_window_index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_world_map_window_index = map_window_index;
    g_display_object_animated_pool[map_window_index].sequence = DISPLAY_OBJECT_ANIM_MAP_FRAME;
    g_display_object_animated_pool[map_window_index].priority = 13;
    wpos = &g_display_object_animated_pool[map_window_index].position;
    rec = &g_display_object_animated_pool[map_window_index];
    wpos->x = 0;
    wpos->y = 8;
    rec->anim_counter = 0;
    g_display_object_animated_pool[map_window_index].frame_index = 0;

    menu_window_index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_world_map_cursor_window = menu_window_index;
    g_display_object_animated_pool[menu_window_index].sequence = DISPLAY_OBJECT_ANIM_ROOT_MENU_FRAME;
    g_display_object_animated_pool[menu_window_index].priority = 13;
    wpos2 = &g_display_object_animated_pool[menu_window_index].position;
    rec2 = &g_display_object_animated_pool[menu_window_index];
    wpos2->x = -4;
    wpos2->y = 8;
    rec2->anim_counter = 0;
    g_display_object_animated_pool[menu_window_index].frame_index = 0;

    world_map_project_tiles();
}

/* Builds the map camera matrix and projects the 13x17 grid of tile corners,
 * storing each screen position and setting WORLD_MAP_TILE_OFF_SCREEN on
 * corners that fall outside the visible window; empty tiles
 * (WORLD_MAP_TILE_EMPTY) keep their flags. */
void world_map_project_tiles(void) {
    MATRIX matrix;
    SVECTOR position;
    VECTOR projected;
    s32 flag;
    s32 i;
    world_map_projected_tile_t* entry;
    world_map_projection_view_t* view;
    SVECTOR* rotation;
    s32 flags;
    s32 updated;
    s32 coordinate;

    /* Retail keeps the camera base at rotation; the record base changes its argument instructions. */
    rotation = &g_world_map_projection_rotation;
    view = (world_map_projection_view_t*)((u8*)rotation - offsetof(world_map_projection_view_t, rotation));
    RotMatrix(rotation, &matrix);
    TransMatrix(&matrix, &view->origin);
    ScaleMatrix(&matrix, &view->scale);
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);

    i = 0;
    entry = g_world_map_projected_tiles;
    for (; i < WORLD_MAP_CORNER_COUNT; i++) {
        coordinate = g_world_map_projected_tiles[i].map.x;
        position.vx = coordinate;
        coordinate = g_world_map_projected_tiles[i].map.y;
        position.vz = 0;
        position.vy = coordinate;
        RotTrans(&position, &projected, &flag);
        g_world_map_projected_tiles[i].screen.x = projected.vx;
        g_world_map_projected_tiles[i].screen.y = projected.vy;
        flags = entry->flags;
        if (!(flags & WORLD_MAP_TILE_EMPTY)) {
            if (projected.vx < -0xa0 || projected.vx > 0x80 || projected.vy < -0x98 || projected.vy > 0x78) {
                updated = flags | WORLD_MAP_TILE_OFF_SCREEN;
            } else {
                updated = flags & ~WORLD_MAP_TILE_OFF_SCREEN;
            }
            entry->flags = updated;
        }
        entry++;
    }
}

/* Sorts the world map's projected tile grid into `ot` as textured quads.
 * Each tile spans from its own projected corner to the next column's x and
 * the next row's y; a tile flagged off screen or empty is skipped. All tiles
 * share the map CLUT and the current screen tint.
 *
 * The 32-bit texture value packs u, v, width and height from the top byte
 * down; a width or height that would run past texel 255 is shortened by one.
 *
 * Needs aspsx 2.21: four sites that reach the typed global array keep their
 * `addiu at,at,<offset>` under 2.21 and lose it under 2.34. Each tile reads
 * its own entry and the next one's .screen.x, so the accesses are written
 * against `index` and `index + 1`. */
void world_map_tiles_draw(GsOT* ot) {
    POLY_FT4 poly;
    CVECTOR color;
    s32 row;
    s32 column;
    s32 index;

    SetPolyFT4(&poly);
    color.r = 0x80;
    color.g = 0x80;
    color.b = 0x80;
    world_map_fade_tint_color(&color, &color);
    setRGB0(&poly, color.r, color.g, color.b);
    poly.clut = GetClut(0, WORLD_MAP_TILE_CLUT_Y);

    for (row = 0; row < WORLD_MAP_TILE_ROWS; row++) {
        index = row * WORLD_MAP_ROW_STRIDE;

        poly.y0 = poly.y1 = g_world_map_projected_tiles[index].screen.y;
        poly.y2 = poly.y3 = g_world_map_projected_tiles[index + WORLD_MAP_ROW_STRIDE].screen.y;

        for (column = 0; column < WORLD_MAP_TILE_COLUMNS; column++, index++) {
            s32 flags = g_world_map_projected_tiles[index].flags;
            u32 texture;
            s32 u;
            s32 v;
            s32 w;
            s32 h;

            if (flags & (WORLD_MAP_TILE_OFF_SCREEN | WORLD_MAP_TILE_EMPTY)) {
                continue;
            }
            poly.tpage = flags & WORLD_MAP_TILE_TPAGE_MASK;
            poly.x0 = poly.x2 = g_world_map_projected_tiles[index].screen.x;
            poly.x1 = poly.x3 = g_world_map_projected_tiles[index + 1].screen.x;

            texture = g_world_map_projected_tiles[index].texture;
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

/* Scroll `position` by `amount` on both axes within the scroll limits, leaving
 * the part that could not be applied in `amount`. */
void world_map_scroll_apply(point32_t* amount, VECTOR* position) {
    world_map_horizontal_scroll_apply(&amount->x, &position->vx);
    world_map_vertical_scroll_apply(amount, position);
}

/* Apply a (negated) scroll amount to a position clamped to [-116, 128] (the
 * same x range coordinates_clamp clamps to) and leave the unapplied
 * remainder in *amount. */
void world_map_horizontal_scroll_apply(s32* amount, s32* position) {
    s32 delta = -*amount;
    s32 old_position;
    s32 value;

    *amount = delta;
    if (delta < 0) {
        old_position = *position;
        value = old_position + delta;
        if (value < WORLD_MAP_SCROLL_MIN_X) {
            value = WORLD_MAP_SCROLL_MIN_X;
        }
        *amount = delta - (value - old_position);
        *position = value;
    }

    if (*amount > 0) {
        old_position = *position;
        value = old_position + *amount;
        if (value >= (WORLD_MAP_SCROLL_MAX_X + 1)) {
            value = WORLD_MAP_SCROLL_MAX_X;
        }
        *amount -= value - old_position;
        *position = value;
    }

    *amount = -*amount;
}

/* Vertical counterpart of world_map_horizontal_scroll_apply: the
 * same shape applied to the vertical amount and projection origin,
 * clamped to [-64, 80], the y range coordinates_clamp uses. */
void world_map_vertical_scroll_apply(point32_t* amount, VECTOR* position) {
    s32 delta = -amount->y;
    s32 old_position;
    s32 value;

    amount->y = delta;
    if (delta < 0) {
        old_position = position->vy;
        value = old_position + delta;
        if (value < WORLD_MAP_SCROLL_MIN_Y) {
            value = WORLD_MAP_SCROLL_MIN_Y;
        }
        amount->y = delta - (value - old_position);
        position->vy = value;
    }

    if (amount->y > 0) {
        old_position = position->vy;
        value = old_position + amount->y;
        if (value >= (WORLD_MAP_SCROLL_MAX_Y + 1)) {
            value = WORLD_MAP_SCROLL_MAX_Y;
        }
        amount->y -= value - old_position;
        position->vy = value;
    }

    amount->y = -amount->y;
}
