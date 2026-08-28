#include "fft/attack.h"
#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/menu_types.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/types.h"

void battle_gfx_init_image_loading(POLY_FT4* primitive, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters);

/* The v/y adjustments and the tpage/clut stores address the current
 * primitive through a byte offset recomputed at the top of each iteration:
 * GCC then strength-reduces that offset last, after the g_attack_deploy_text_graphic_params[i]
 * parameter pointer, which is the target's induction-register order. The
 * equivalent g_attack_deploy_text_polys[parity][i] (or a flat index) form reduces a second
 * copy of the offset after the join below and changes the loop increments. */
void attack_deploy_update_screen_graphics(void) {
    s32 i;
    s32 parity;
    s32 squad;
    s32* value;
    s32 off;

    parity = 0;
    for (i = 0; i < 9; i++) {
        g_attack_deploy_text_reveal_steps[i] = 1;
        g_attack_deploy_text_reveal_heights[i] = 0;
    }
    while (1) {
        squad = g_attack_deploy_current_squad;
        parity ^= 1;
        if (g_attack_deploy_current_squad >= battle_script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT)) {
            squad--;
        }
        if (squad == 0) {
            g_attack_deploy_text_graphic_params[1].x_load = 0xc0;
            g_attack_deploy_text_graphic_params[1].y_load = 0x18;
        } else {
            g_attack_deploy_text_graphic_params[1].x_load = 0xe8;
            g_attack_deploy_text_graphic_params[1].y_load = 0xc;
        }
        g_attack_deploy_text_graphic_params[4].x_load = g_attack_deploy_current_squad_data->unit_limit * 8 + 0xc8;
        g_attack_deploy_text_graphic_params[7].x_load
            = (g_attack_deploy_current_squad_data->unit_limit - g_attack_deploy_deployed_unit_count) * 8 + 0xc8;
        for (i = 0; i < 9; i++) {
            off = parity * sizeof(g_attack_deploy_text_polys[0]) + i * sizeof(POLY_FT4);
            if ((g_attack_deploy_current_squad_data->unit_limit - g_attack_deploy_deployed_unit_count - 1 < 0 && i >= 6)
                || battle_thread_get_current_parameter_3() != 0) {
                if (g_attack_deploy_current_squad_data->unit_limit - g_attack_deploy_deployed_unit_count - 1 < 0) {
                    g_attack_deploy_text_graphic_params[7].x_load = 0xd0;
                }
                g_attack_deploy_text_reveal_steps[6] = -1;
                g_attack_deploy_text_reveal_steps[7] = -1;
                g_attack_deploy_text_reveal_steps[8] = -1;
            } else {
                g_attack_deploy_text_reveal_steps[6] = 1;
                g_attack_deploy_text_reveal_steps[7] = 1;
                g_attack_deploy_text_reveal_steps[8] = 1;
            }
            battle_gfx_init_default_poly_ft4(&g_attack_deploy_text_polys[parity][i]);
            ((POLY_FT4*)((u8*)g_attack_deploy_text_polys + off))->tpage = GetTPage(0, 0, 0x100, 0);
            ((POLY_FT4*)((u8*)g_attack_deploy_text_polys + off))->clut = 0x7d7c;
            battle_gfx_init_image_loading(&g_attack_deploy_text_polys[parity][i],
                (battle_image_location_t*)g_attack_deploy_texture_location, &g_attack_deploy_text_screen_origin,
                &g_attack_deploy_text_graphic_params[i]);
            if (g_attack_deploy_text_reveal_steps[i] != 0) {
                if (g_attack_deploy_text_reveal_steps[i] > 0) {
                    value = &g_attack_deploy_text_reveal_heights[i];
                    if (*value != 12) {
                        *value += g_attack_deploy_text_reveal_steps[i];
                    }
                } else {
                    value = &g_attack_deploy_text_reveal_heights[i];
                    if (*value != 0) {
                        *value += g_attack_deploy_text_reveal_steps[i];
                    }
                }
                ((POLY_FT4*)((u8*)g_attack_deploy_text_polys + off))->v0 -= *value - 12;
                ((POLY_FT4*)((u8*)g_attack_deploy_text_polys + off))->v1 -= *value - 12;
                ((POLY_FT4*)((u8*)g_attack_deploy_text_polys + off))->y0 -= *value - 12;
                ((POLY_FT4*)((u8*)g_attack_deploy_text_polys + off))->y1 -= *value - 12;
            }
            battle_gfx_draw_or_append_gpu_primitive((s32*)&g_attack_deploy_text_polys[parity][i]);
        }
        battle_thread_yield();
        if (battle_thread_get_current_parameter_3() == 0) {
            continue;
        }
        for (i = 8; i >= 0; i--) {
            g_attack_deploy_text_reveal_steps[i] = -1;
        }
        if (g_attack_deploy_text_reveal_heights[0] == 0 && g_attack_deploy_text_reveal_heights[3] == 0
            && g_attack_deploy_text_reveal_heights[6] == 0) {
            break;
        }
    }
    battle_thread_exit_current();
}
