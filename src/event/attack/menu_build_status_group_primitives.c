/* The animated status groups of a menu panel, fading in as they appear. */

#include "fft/event_attack.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Rebuild and submit the scaled sprite quads of every visible editor entry,
 * advancing each entry's fade/blink state by one frame.
 *
 * The y bias is read as `*g_attack_panel_status_y_offsets[entry]`: forming the row address first
 * makes the outer loop keep it as a spilled pointer induction variable
 * (0x30(sp), reloaded into $s4 per entry) as the target does; a plain
 * `g_attack_panel_status_y_offsets[entry][0]` folds the table into each load instead. */
void menu_build_status_group_primitives_variant(POLY_FT4* poly) {
    s32 group;
    s32 cell;
    s32 first;
    s32 count;
    s32 index;
    s32 sprite;
    s32 level;

    for (group = 0; group < g_attack_panel_status_group_count; group++) {
        sprite = g_attack_panel_status_group_ids[group];
        count = g_attack_panel_status_group_bounds[sprite + 1] - g_attack_panel_status_group_bounds[sprite];
        first = g_attack_panel_status_group_bounds[sprite];
        if (g_attack_panel_status_animation[group] != 0xff) {
            if (g_attack_panel_status_animation[group] > 0x40) {
                g_attack_panel_status_animation[group] -= 2;
            } else if (g_attack_panel_status_animation[group] > 10) {
                g_attack_panel_status_animation[group]--;
                if (g_attack_panel_status_animation[group] == 11) {
                    g_attack_panel_status_animation[group] = 1;
                }
            } else if (g_attack_panel_status_animation[group] > 1) {
                g_attack_panel_status_animation[group]--;
                if (g_attack_panel_status_animation[group] == 1) {
                    g_attack_panel_status_animation[group] = 0;
                }
            }
        }
        for (cell = 0; cell < count; cell++) {
            menu_init_default_poly_ft4(poly);
            index = first + count - cell;
            g_menu_draw_area_template.y += *g_attack_panel_status_y_offsets[group];
            gfx_set_scaled_poly_ft4_4bpp(poly, g_editor_numeric_geometry, &g_menu_draw_area_template,
                &g_attack_panel_status_uv_rects[index], &g_attack_panel_status_scales[index],
                &g_attack_panel_status_offsets[group]);
            g_menu_draw_area_template.y -= *g_attack_panel_status_y_offsets[group];
            if (g_attack_panel_status_animation[group] == 0x40) {
                setRGB0(poly, 0x80, 0x80, 0x80);
                g_attack_panel_status_animation[group] = 0;
            }
            if (g_attack_panel_status_animation[group] == 0 || g_attack_panel_status_animation[group] >= 0x40) {
                poly->clut = 0x7d7c;
            } else if (g_attack_panel_status_animation[group] == 1) {
                poly->clut = 0x7dfc;
            } else if (g_attack_panel_status_animation[group] < 0x40) {
                poly->clut = 0x7e7c;
                poly->y0++;
                poly->y1++;
                poly->y2++;
                poly->y3++;
            }
            SetSemiTrans(poly, 1);
            SetShadeTex(poly, 0);
            if (g_attack_panel_status_animation[group] != 0xff) {
                if (g_attack_panel_status_animation[group] > 0x40) {
                    level = 0xc0 - g_attack_panel_status_animation[group];
                    setRGB0(poly, level, level, level);
                }
                gfx_draw_or_append_gpu_primitive(poly);
            }
            poly++;
        }
    }
}
