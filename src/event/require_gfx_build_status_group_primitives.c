#include "fft/battle_gfx.h"
#include "fft/require.h"
#include "psx/types.h"

/* Render the animated status groups used by the post-battle transition.
 *
 * Each group's animation byte counts down: values above 0x40 step by 2, the
 * 0x0b-0x40 range steps by 1 and then jumps to 1, and 2-0x0a steps down to 0.
 * 0xff marks an inactive group. */
void require_gfx_build_status_group_primitives(POLY_FT4* poly) {
    s32 group;
    s32 index;
    s32 count;
    s32 first;
    s32 status;
    u8 group_id;
    s16* position_y;

    for (group = 0; group < g_require_panel_status_group_count; group++) {
        /* Assigned inside the loop so the invariant lands in the preheader. */
        position_y = &g_require_gfx_draw_area_template.y;
        group_id = g_require_panel_status_group_ids[group];
        count = g_require_panel_status_group_bounds[group_id + 1] - g_require_panel_status_group_bounds[group_id];
        first = g_require_panel_status_group_bounds[group_id];
        if (g_require_panel_status_animation[group] != 0xff) {
            if (g_require_panel_status_animation[group] >= 0x41) {
                g_require_panel_status_animation[group] -= 2;
            } else if (g_require_panel_status_animation[group] >= 0x0b) {
                g_require_panel_status_animation[group]--;
                if (g_require_panel_status_animation[group] == 0x0b) {
                    g_require_panel_status_animation[group] = 1;
                }
            } else if (g_require_panel_status_animation[group] >= 2) {
                g_require_panel_status_animation[group]--;
                if (g_require_panel_status_animation[group] == 1) {
                    g_require_panel_status_animation[group] = 0;
                }
            }
        }

        for (index = 0; index < count; index++) {
            battle_gfx_init_default_poly_ft4(poly);
            status = first + count - index;
            *position_y += g_require_panel_status_y_offsets[group * 2];
            require_gfx_set_scaled_poly_ft4_geometry(poly,
                (require_gfx_texture_page_position_t*)&g_require_panel_status_texture,
                (require_gfx_point_t*)(position_y - 1),
                (require_gfx_sprite_rect_t*)g_require_panel_status_uv_rects[status],
                (require_gfx_point_t*)g_require_panel_status_scales[status],
                (POLY_FT4*)g_require_panel_status_offsets[group]);
            *position_y -= g_require_panel_status_y_offsets[group * 2];

            if (g_require_panel_status_animation[group] == 0x40) {
                poly->r0 = 0x80;
                poly->g0 = 0x80;
                poly->b0 = 0x80;
                g_require_panel_status_animation[group] = 0;
            }

            if (g_require_panel_status_animation[group] == 0 || g_require_panel_status_animation[group] >= 0x40) {
                poly->clut = 0x7d7c;
            } else if (g_require_panel_status_animation[group] == 1) {
                poly->clut = 0x7dfc;
            } else if (g_require_panel_status_animation[group] < 0x40) {
                poly->clut = 0x7e7c;
                poly->y0++;
                poly->y1++;
                poly->y2++;
                poly->y3++;
            }

            SetSemiTrans(poly, 1);
            SetShadeTex(poly, 0);
            battle_gfx_draw_or_append_gpu_primitive(poly);
            poly++;
        }
    }
}
