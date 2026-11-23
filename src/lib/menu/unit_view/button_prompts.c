/* The animated button prompt icons (L1, R1 and the like) drawn beside a unit panel. */

#include "fft/gfx/gfx_primitive.h"
#include "fft/menu/menu.h"
#include "fft/menu/menu_gfx.h"
#include "fft/party/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern u8 g_unit_panel_button_prompt_ids[];
extern u8 g_unit_panel_button_prompt_cell_bounds[];
extern sprite_draw_offset_t g_unit_panel_button_prompt_offsets[];
extern s32 g_unit_panel_button_prompt_y_offsets[5];

/* Advance and draw a unit panel's button prompt icons, one scaled quad per cell.
 *
 * Each prompt's animation byte counts down: values above 0x40 step by 2, the
 * 0x0b-0x40 range steps by 1 and then jumps to 1, and 2-0x0a steps down to 0.
 * A press sets it to 7; 0xff hides the prompt. */
void unit_panel_build_button_prompts(POLY_FT4* poly) {
    s32 group;
    s32 cell;
    s32 count;
    s32 first;
    s32 index;
    u8 group_id;

    for (group = 0; group < g_unit_panel_button_prompt_count; group++) {
        group_id = g_unit_panel_button_prompt_ids[group];
        count = g_unit_panel_button_prompt_cell_bounds[group_id + 1] - g_unit_panel_button_prompt_cell_bounds[group_id];
        first = g_unit_panel_button_prompt_cell_bounds[group_id];
        if (g_unit_panel_button_prompt_animation[group] != 0xff) {
            if (g_unit_panel_button_prompt_animation[group] >= 0x41) {
                g_unit_panel_button_prompt_animation[group] -= 2;
            } else if (g_unit_panel_button_prompt_animation[group] >= 0x0b) {
                g_unit_panel_button_prompt_animation[group]--;
                if (g_unit_panel_button_prompt_animation[group] == 0x0b) {
                    g_unit_panel_button_prompt_animation[group] = 1;
                }
            } else if (g_unit_panel_button_prompt_animation[group] >= 2) {
                g_unit_panel_button_prompt_animation[group]--;
                if (g_unit_panel_button_prompt_animation[group] == 1) {
                    g_unit_panel_button_prompt_animation[group] = 0;
                }
            }
        }

        for (cell = 0; cell < count; cell++) {
            menu_init_default_poly_ft4(poly);
            index = first + count - cell;
            g_menu_draw_area_template.y += (u16)g_unit_panel_button_prompt_y_offsets[group];
            gfx_set_scaled_poly_ft4_4bpp(poly, &g_unit_panel_texture_origin, &g_menu_draw_area_template,
                &g_unit_panel_button_prompt_frames[index - 1], &g_unit_panel_button_prompt_frame_scales[index - 1],
                &g_unit_panel_button_prompt_offsets[group]);
            g_menu_draw_area_template.y -= (u16)g_unit_panel_button_prompt_y_offsets[group];

            if (g_unit_panel_button_prompt_animation[group] == 0x40) {
                setRGB0(poly, 0x80, 0x80, 0x80);
                g_unit_panel_button_prompt_animation[group] = 0;
            }

            if (g_unit_panel_button_prompt_animation[group] == 0
                || g_unit_panel_button_prompt_animation[group] >= 0x40) {
                poly->clut = 0x7d7c;
            } else if (g_unit_panel_button_prompt_animation[group] == 1) {
                poly->clut = 0x7dfc;
            } else if (g_unit_panel_button_prompt_animation[group] < 0x40) {
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
