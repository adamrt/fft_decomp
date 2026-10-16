/* The animated status groups of a menu panel. */

#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Render the animated status groups of a menu panel.
 *
 * Each group's animation byte counts down: values above 0x40 step by 2, the
 * 0x0b-0x40 range steps by 1 and then jumps to 1, and 2-0x0a steps down to 0.
 * 0xff marks an inactive group. */
void unit_panel_build_status_groups(POLY_FT4* poly) {
    s32 group;
    s32 cell;
    s32 count;
    s32 first;
    s32 index;
    u8 group_id;

    for (group = 0; group < g_unit_panel_panel_status_group_count; group++) {
        group_id = g_unit_panel_panel_status_group_ids[group];
        count = g_unit_panel_panel_status_group_bounds[group_id + 1] - g_unit_panel_panel_status_group_bounds[group_id];
        first = g_unit_panel_panel_status_group_bounds[group_id];
        if (g_unit_panel_panel_status_animation[group] != 0xff) {
            if (g_unit_panel_panel_status_animation[group] >= 0x41) {
                g_unit_panel_panel_status_animation[group] -= 2;
            } else if (g_unit_panel_panel_status_animation[group] >= 0x0b) {
                g_unit_panel_panel_status_animation[group]--;
                if (g_unit_panel_panel_status_animation[group] == 0x0b) {
                    g_unit_panel_panel_status_animation[group] = 1;
                }
            } else if (g_unit_panel_panel_status_animation[group] >= 2) {
                g_unit_panel_panel_status_animation[group]--;
                if (g_unit_panel_panel_status_animation[group] == 1) {
                    g_unit_panel_panel_status_animation[group] = 0;
                }
            }
        }

        for (cell = 0; cell < count; cell++) {
            menu_init_default_poly_ft4(poly);
            index = first + count - cell;
            g_menu_draw_area_template.y += g_unit_panel_panel_status_y_offsets[group * 2];
            gfx_set_scaled_poly_ft4_4bpp(poly, &g_unit_panel_panel_status_texture, &g_menu_draw_area_template,
                &g_unit_panel_panel_status_uv_rects[index], &g_unit_panel_panel_status_scales[index],
                &g_unit_panel_panel_status_offsets[group]);
            g_menu_draw_area_template.y -= g_unit_panel_panel_status_y_offsets[group * 2];

            if (g_unit_panel_panel_status_animation[group] == 0x40) {
                setRGB0(poly, 0x80, 0x80, 0x80);
                g_unit_panel_panel_status_animation[group] = 0;
            }

            if (g_unit_panel_panel_status_animation[group] == 0 || g_unit_panel_panel_status_animation[group] >= 0x40) {
                poly->clut = 0x7d7c;
            } else if (g_unit_panel_panel_status_animation[group] == 1) {
                poly->clut = 0x7dfc;
            } else if (g_unit_panel_panel_status_animation[group] < 0x40) {
                poly->clut = 0x7e7c;
                poly->y0++;
                poly->y1++;
                poly->y2++;
                poly->y3++;
            }

            SetSemiTrans(poly, 1);
            SetShadeTex(poly, 0);
            gfx_draw_or_append_gpu_primitive(poly);
            poly++;
        }
    }
}
