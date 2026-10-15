#include "fft/battle_gfx.h"
#include "fft/gfx.h"
#include "fft/menu.h"

/* Render the animated status groups of a menu panel.
 *
 * Each group's animation byte counts down: values above 0x40 step by 2, the
 * 0x0b-0x40 range steps by 1 and then jumps to 1, and 2-0x0a steps down to 0.
 * 0xff marks an inactive group. */
void menu_build_status_group_primitives_variant(POLY_FT4* poly) {
    s32 group;
    s32 index;
    s32 count;
    s32 first;
    s32 status;
    u8 group_id;

    for (group = 0; group < g_menu_panel_status_group_count; group++) {
        group_id = g_menu_panel_status_group_ids[group];
        count = g_menu_panel_status_group_bounds[group_id + 1] - g_menu_panel_status_group_bounds[group_id];
        first = g_menu_panel_status_group_bounds[group_id];
        if (g_menu_panel_status_animation[group] != 0xff) {
            if (g_menu_panel_status_animation[group] >= 0x41) {
                g_menu_panel_status_animation[group] -= 2;
            } else if (g_menu_panel_status_animation[group] >= 0x0b) {
                g_menu_panel_status_animation[group]--;
                if (g_menu_panel_status_animation[group] == 0x0b) {
                    g_menu_panel_status_animation[group] = 1;
                }
            } else if (g_menu_panel_status_animation[group] >= 2) {
                g_menu_panel_status_animation[group]--;
                if (g_menu_panel_status_animation[group] == 1) {
                    g_menu_panel_status_animation[group] = 0;
                }
            }
        }

        for (index = 0; index < count; index++) {
            menu_init_default_poly_ft4(poly);
            status = first + count - index;
            g_menu_draw_area_template.y += g_menu_panel_status_y_offsets[group * 2];
            gfx_set_scaled_poly_ft4_4bpp(poly, &g_menu_panel_status_texture, &g_menu_draw_area_template,
                &g_menu_panel_status_uv_rects[status], &g_menu_panel_status_scales[status],
                &g_menu_panel_status_offsets[group]);
            g_menu_draw_area_template.y -= g_menu_panel_status_y_offsets[group * 2];

            if (g_menu_panel_status_animation[group] == 0x40) {
                setRGB0(poly, 0x80, 0x80, 0x80);
                g_menu_panel_status_animation[group] = 0;
            }

            if (g_menu_panel_status_animation[group] == 0 || g_menu_panel_status_animation[group] >= 0x40) {
                poly->clut = 0x7d7c;
            } else if (g_menu_panel_status_animation[group] == 1) {
                poly->clut = 0x7dfc;
            } else if (g_menu_panel_status_animation[group] < 0x40) {
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
