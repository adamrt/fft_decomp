#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* World-map tile atlas setup, run once from wldcore_init_subsystems between
 * wldcore_window_init_record_pools() and
 * wldcore_load_formation_unit_name_and_face(). Because the pools were just
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
 *    wpos2/rec2). One shared pair is long-lived and materialises both
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
                        = ((clut_x & 0xff) << 24) | ((clut_y & 0xff) << 16) | ((width & 0xff) << 8) | (height & 0xff);
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

    map_window_index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_map_window_index = map_window_index;
    g_wldcore_window_records[map_window_index].sequence = WLDCORE_ANIM_SEQUENCE_MAP_FRAME;
    g_wldcore_window_records[map_window_index].priority = 13;
    wpos = &g_wldcore_window_records[map_window_index].position;
    rec = &g_wldcore_window_records[map_window_index];
    wpos->x = 0;
    wpos->y = 8;
    rec->anim_counter = 0;
    g_wldcore_window_records[map_window_index].frame_index = 0;

    menu_window_index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
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
