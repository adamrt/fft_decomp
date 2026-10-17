/* Deployment panel status icons and shaded background primitives. */

#include "fft/deploy.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Rebuild and submit the deployment panel's prompt icon groups, one scaled quad per
 * cell, after advancing each group's animation byte by one frame:
 *   0xff         hidden;
 *   0            lit (CLUT 0x7d7c);
 *   1            dimmed (CLUT 0x7dfc);
 *   2-0x0a       pressed (CLUT 0x7e7c, one pixel lower), counting down to lit;
 *   0x0b-0x40    pressed, counting down to dimmed;
 *   above 0x40   fading in, two steps a frame, at brightness 0xc0 minus the value until
 *                it reaches 0x40 and becomes lit.
 *
 * The panel uses the unsigned low half of each signed word offset. Taking its address
 * before the halfword read preserves the retail load scheduling. */
void unit_panel_build_status_groups(POLY_FT4* poly) {
    s32 group;
    s32 cell;
    s32 first;
    s32 count;
    s32 index;
    s32 sprite;
    s32 level;

    for (group = 0; group < g_unit_panel_status_group_count; group++) {
        sprite = g_unit_panel_status_group_ids[group];
        count = g_unit_panel_status_group_bounds[sprite + 1] - g_unit_panel_status_group_bounds[sprite];
        first = g_unit_panel_status_group_bounds[sprite];
        if (g_unit_panel_status_animation[group] != 0xff) {
            if (g_unit_panel_status_animation[group] > 0x40) {
                g_unit_panel_status_animation[group] -= 2;
            } else if (g_unit_panel_status_animation[group] > 10) {
                g_unit_panel_status_animation[group]--;
                if (g_unit_panel_status_animation[group] == 11) {
                    g_unit_panel_status_animation[group] = 1;
                }
            } else if (g_unit_panel_status_animation[group] > 1) {
                g_unit_panel_status_animation[group]--;
                if (g_unit_panel_status_animation[group] == 1) {
                    g_unit_panel_status_animation[group] = 0;
                }
            }
        }
        for (cell = 0; cell < count; cell++) {
            menu_init_default_poly_ft4(poly);
            index = first + count - cell;
            g_menu_draw_area_template.y += (u16)g_deploy_slot_offsets[group];
            gfx_set_scaled_poly_ft4_4bpp(poly, g_unit_panel_editor_numeric_geometry, &g_menu_draw_area_template,
                &g_unit_panel_status_frames[index - 1], &g_unit_panel_status_frame_scales[index - 1],
                &g_unit_panel_status_offsets[group]);
            g_menu_draw_area_template.y -= (u16)g_deploy_slot_offsets[group];
            if (g_unit_panel_status_animation[group] == 0x40) {
                setRGB0(poly, 0x80, 0x80, 0x80);
                g_unit_panel_status_animation[group] = 0;
            }
            if (g_unit_panel_status_animation[group] == 0 || g_unit_panel_status_animation[group] >= 0x40) {
                poly->clut = 0x7d7c;
            } else if (g_unit_panel_status_animation[group] == 1) {
                poly->clut = 0x7dfc;
            } else if (g_unit_panel_status_animation[group] < 0x40) {
                poly->clut = 0x7e7c;
                poly->y0++;
                poly->y1++;
                poly->y2++;
                poly->y3++;
            }
            SetSemiTrans(poly, 1);
            SetShadeTex(poly, 0);
            if (g_unit_panel_status_animation[group] != 0xff) {
                if (g_unit_panel_status_animation[group] > 0x40) {
                    level = 0xc0 - g_unit_panel_status_animation[group];
                    setRGB0(poly, level, level, level);
                }
                gfx_draw_or_append_gpu_primitive(poly);
            }
            poly++;
        }
    }
}

/* Build the 4x8 grid of shaded, textured quads behind the panel: each row is
 * one step brighter than the one above, and every cell shows the same 64x32
 * texture rectangle. */
void gfx_build_gradient_grid_primitives(POLY_GT4* poly) {
    s32 row;
    s32 column;
    s32 intensity;

    if (g_deploy_tiles_only_mode != 0) {
        return;
    }

    for (row = 0; row < 8; row++) {
        for (column = 0; column < 4; column++) {
            SetPolyGT4(poly);
            poly->clut = 0x7fbc;
            poly->tpage = GetTPage(0, 0, 0x3c0, 0x100);
            intensity = row * 8 + 0x40;
            setRGB0(poly, intensity, intensity, intensity);
            setRGB1(poly, intensity, intensity, intensity);
            intensity = (row + 1) * 8 + 0x40;
            setRGB2(poly, intensity, intensity, intensity);
            setRGB3(poly, intensity, intensity, intensity);
            setXY4(poly, column * 0x40 + 0x80, row * 0x20, column * 0x40 + 0xc0, row * 0x20, column * 0x40 + 0x80,
                row * 0x20 + 0x20, column * 0x40 + 0xc0, row * 0x20 + 0x20);
            setUV4(poly, 0x58, 0xd8, 0x98, 0xd8, 0x58, 0xf8, 0x98, 0xf8);
            SetSemiTrans(poly, 1);
            SetShadeTex(poly, 0);
            gfx_draw_or_append_gpu_primitive(poly);
            poly++;
        }
    }
}
