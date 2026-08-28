#include "fft/battle_gfx.h"
#include "fft/battle_text.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "fft/require.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

#define DIGITS g_require_reward_money_digit_polys
#define NEXT   g_require_reward_money_next_digit_polys
/* Texture page (384, 0): both GPU-type encodings select page 6, but the
 * GetGraphType() calls of the type test remain in the target. */
#define TPAGE ((GetGraphType() == 1 || GetGraphType() == 2) ? 6 : 6)
#define X0    (0x108 - j * 13 - (5 - digits) * 13 / 2)
#define CLUT  ((g_require_reward_bonus_palette_rect.y << 6) | ((g_require_reward_bonus_palette_rect.x >> 4) & 0x3f))

/*
 * Post-battle gil ticker: adds the war-result money to script variable 0x2c,
 * starts the display-condition thread, splits the amount into up to six
 * digits and rolls each digit's odometer strip (13-pixel cells, counter << 13
 * fixed point, slowing by 0x44 per step once the target digit comes round)
 * before holding for 60 frames and fading out.
 *
 * The polygon pointers are formed as `(s32)array + offset` byte offsets, which
 * keeps the target's separate base and offset; `&DIGITS[bank][j]` is reduced
 * to a single pointer induction variable. `pass` doubles as the fade level,
 * which the target's register allocation reflects.
 */
void require_render_gil_reward_ticker(void) {
    DR_AREA areas[4];
    s32 thread_id;
    s32 money;
    s32 digits;
    s32 i;
    s32 j;
    s32 bank;
    s32 tick;
    s32 pass;
    s32 x;
    s32 off;
    POLY_FT4* commas;
    POLY_FT4* comma;
    POLY_FT4* sign;

    g_require_reward_war_trophy_entry_count = 0;
    battle_unit_find_war_trophies_and_bonus_money(&g_require_reward_war_trophy_search_result);
    battle_script_set_variable(0x2c,
        battle_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS)
            + g_require_reward_war_trophy_search_result.level_sum_money);
    money = g_require_reward_war_trophy_search_result.level_sum_money;
    thread_id = battle_thread_resolve_id(0x10);
    battle_thread_start(thread_id, require_render_display_condition_special_cases_thread);
    battle_thread_set_parameters(thread_id, 2, 0, 0);
    g_require_input_controller = battle_script_get_controller_input_pointer(0);
    /* `i = 0` here (not before the script calls) lets sched1 keep
     * `li a0,0x2c` above the money load. */
    for (i = 0; i < 6; i++) {
        g_require_reward_money_digit_speeds[i] = g_require_reward_money_digit_initial_speeds[i];
        g_require_reward_money_digit_settling[i] = 0;
    }
    digits = battle_text_count_decimal_digits(money);
    if (digits >= 7) {
        main_system_handle_malloc_exception(7, 0);
        money = 0;
        digits = 1;
    }
    j = money;
    for (i = 0; i < digits; i++) {
        g_require_reward_money_digit_targets[i] = j % 10;
        j /= 10;
        g_require_reward_money_digit_positions[i] = g_require_reward_money_digit_targets[i] << 13;
    }
    g_sound_effect_id_to_play = 4;
    for (bank = 0; bank < 2; bank++) {
        for (j = 0; j < digits; j++) {
            POLY_FT4* pd;
            POLY_FT4* pn;

            battle_gfx_init_default_poly_ft4(&DIGITS[bank][j]);
            battle_gfx_init_default_poly_ft4(&NEXT[bank][j]);
            DIGITS[bank][j].tpage = TPAGE;
            DIGITS[bank][j].clut = CLUT;
            NEXT[bank][j].tpage = TPAGE;
            NEXT[bank][j].clut = CLUT;
            pd = (POLY_FT4*)((s32)DIGITS + (j * 0x28 + bank * 0xf0));
            pd->x0 = X0;
            pd->y0 = 0x80;
            pd->x1 = X0 + 13;
            pd->y1 = 0x80;
            pd->x2 = X0;
            pd->y2 = 0x97;
            pd->x3 = X0 + 13;
            pd->y3 = 0x97;
            pn = (POLY_FT4*)((s32)NEXT + (j * 0x28 + bank * 0xf0));
            pn->x0 = X0;
            pn->y0 = 0x69;
            pn->x1 = X0 + 13;
            pn->y1 = 0x69;
            pn->x2 = X0;
            pn->y2 = 0x80;
            pn->x3 = X0 + 13;
            pn->y3 = 0x80;
            if (j >= 3) {
                DIGITS[bank][j].x0 -= 8;
                NEXT[bank][j].x0 -= 8;
            }
        }
    }
    for (i = 0; i < 2; i++) {
        /* Block-local pointers: local-alloc then builds each address in the
         * pointer's own register, as the target does. */
        POLY_FT4* comma_poly;
        POLY_FT4* sign_poly;

        comma_poly = &g_require_reward_money_comma_polys[i];
        battle_gfx_init_default_poly_ft4(comma_poly);
        comma_poly->x0 = DIGITS[0][2].x0 - 10;
        comma_poly->y0 = 0x8f;
        comma_poly->x1 = DIGITS[0][2].x0 - 4;
        comma_poly->y1 = 0x8f;
        comma_poly->x2 = DIGITS[0][2].x0 - 10;
        comma_poly->y2 = 0x99;
        comma_poly->x3 = DIGITS[0][2].x0 - 4;
        comma_poly->y3 = 0x99;
        comma_poly->u0 = 0x82;
        comma_poly->v0 = 0x40;
        comma_poly->u1 = 0x88;
        comma_poly->v1 = 0x40;
        comma_poly->u2 = 0x82;
        comma_poly->v2 = 0x4a;
        comma_poly->u3 = 0x88;
        comma_poly->v3 = 0x4a;
        g_require_reward_money_comma_polys[i].tpage = TPAGE;
        g_require_reward_money_comma_polys[i].clut = CLUT;
        sign_poly = &g_require_reward_money_sign_polys[i];
        battle_gfx_init_default_poly_ft4(sign_poly);
        sign_poly->x0 = DIGITS[0][0].x0 + 0x11;
        sign_poly->y0 = 0x80;
        sign_poly->x1 = DIGITS[0][0].x0 + 0x31;
        sign_poly->y1 = 0x80;
        sign_poly->x2 = DIGITS[0][0].x0 + 0x11;
        sign_poly->y2 = 0x97;
        sign_poly->x3 = DIGITS[0][0].x0 + 0x31;
        sign_poly->y3 = 0x97;
        sign_poly->u0 = 0x88;
        sign_poly->v0 = 0x32;
        sign_poly->u1 = 0xa8;
        sign_poly->v1 = 0x32;
        sign_poly->u2 = 0x88;
        sign_poly->v2 = 0x49;
        sign_poly->u3 = 0xa8;
        sign_poly->v3 = 0x49;
        g_require_reward_money_sign_polys[i].tpage = TPAGE;
        g_require_reward_money_sign_polys[i].clut = CLUT;
    }
    if (g_main_gfx_screen_polarity == 0) {
        SetDrawArea(&areas[0], &g_require_reward_money_clip_rect_0);
        SetDrawArea(&areas[1], &g_require_reward_money_clip_rect_1);
        SetDrawArea(&areas[2], &g_require_reward_screen_clip_rect_0);
        SetDrawArea(&areas[3], &g_require_reward_screen_clip_rect_1);
    } else {
        SetDrawArea(&areas[0], &g_require_reward_money_clip_rect_1);
        SetDrawArea(&areas[1], &g_require_reward_money_clip_rect_0);
        SetDrawArea(&areas[2], &g_require_reward_screen_clip_rect_1);
        SetDrawArea(&areas[3], &g_require_reward_screen_clip_rect_0);
    }
    tick = 0;
    i = 0;
    for (;;) {
        bank = i & 1;
        battle_thread_yield();
        battle_gfx_draw_or_append_gpu_primitive((s32*)&areas[bank + 2]);
        for (pass = 0; pass < 2; pass++, tick++) {
            for (j = 0; j < digits; j++) {
                POLY_FT4* pd;
                POLY_FT4* pn;

                g_require_reward_money_digit_positions[j] += g_require_reward_money_digit_speeds[j];
                off = j * 0x28 + bank * 0xf0;
                x = DIGITS[bank][j].x0;
                pd = (POLY_FT4*)((s32)DIGITS + off);
                pd->x0 = x;
                pd->y0 = ((g_require_reward_money_digit_positions[j] >> 8) & 0x1f) + 0x80;
                pd->x1 = x + 13;
                pd->y1 = ((g_require_reward_money_digit_positions[j] >> 8) & 0x1f) + 0x80;
                pd->x2 = x;
                pd->y2 = ((g_require_reward_money_digit_positions[j] >> 8) & 0x1f) + 0x97;
                pd->x3 = x + 13;
                pd->y3 = ((g_require_reward_money_digit_positions[j] >> 8) & 0x1f) + 0x97;
                pd->u0 = (g_require_reward_money_digit_positions[j] >> 13) % 10 * 13;
                pd->v0 = 0x32;
                pd->u1 = (g_require_reward_money_digit_positions[j] >> 13) % 10 * 13 + 13;
                pd->v1 = 0x32;
                pd->u2 = (g_require_reward_money_digit_positions[j] >> 13) % 10 * 13;
                pd->v2 = 0x49;
                pd->u3 = (g_require_reward_money_digit_positions[j] >> 13) % 10 * 13 + 13;
                pd->v3 = 0x49;
                x = NEXT[bank][j].x0;
                off = j * 0x28 + bank * 0xf0;
                pn = (POLY_FT4*)((s32)NEXT + off);
                pn->x0 = x;
                pn->y0 = DIGITS[bank][j].y0 - 0x20;
                pn->x1 = x + 13;
                pn->y1 = DIGITS[bank][j].y0 - 0x20;
                pn->x2 = x;
                pn->y2 = DIGITS[bank][j].y0 - 9;
                pn->x3 = x + 13;
                pn->y3 = DIGITS[bank][j].y0 - 9;
                pn->u0 = ((g_require_reward_money_digit_positions[j] >> 13) + 1) % 10 * 13;
                pn->v0 = 0x32;
                pn->u1 = ((g_require_reward_money_digit_positions[j] >> 13) + 1) % 10 * 13 + 13;
                pn->v1 = 0x32;
                pn->u2 = ((g_require_reward_money_digit_positions[j] >> 13) + 1) % 10 * 13;
                pn->v2 = 0x49;
                pn->u3 = ((g_require_reward_money_digit_positions[j] >> 13) + 1) % 10 * 13 + 13;
                pn->v3 = 0x49;
                if (j * 32 + (digits - 1) * 16 < tick) {
                    if (((g_require_reward_money_digit_positions[j] >> 13) + 6) % 10
                        == g_require_reward_money_digit_targets[j]) {
                        g_require_reward_money_digit_settling[j] = 1;
                    }
                    if (g_require_reward_money_digit_settling[j] == 1) {
                        if (g_require_reward_money_digit_speeds[j] > 0x100) {
                            g_require_reward_money_digit_speeds[j] -= 0x44;
                        } else {
                            g_require_reward_money_digit_speeds[j] = 0x100;
                            if ((g_require_reward_money_digit_positions[j] >> 13) % 10
                                    == g_require_reward_money_digit_targets[j]
                                && ((g_require_reward_money_digit_positions[j] >> 8) & 0x1f) == 0) {
                                g_require_reward_money_digit_speeds[j] = 0;
                                if (j == digits - 1) {
                                    /* The last digit settled: leave all three loops. */
                                    goto finished;
                                }
                            }
                        }
                    }
                }
            }
        }
        for (j = 0; j < digits; j++) {
            if (j * 16 < i) {
                battle_gfx_draw_or_append_gpu_primitive((s32*)&DIGITS[bank][j]);
                battle_gfx_draw_or_append_gpu_primitive((s32*)&NEXT[bank][j]);
            }
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&areas[bank]);
        if (digits >= 4 && i >= 0x31) {
            battle_gfx_draw_or_append_gpu_primitive((s32*)&g_require_reward_money_comma_polys[bank]);
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&g_require_reward_money_sign_polys[bank]);
        if (*g_require_input_controller & PSX_PAD_CIRCLE) {
            /* Confirm skips the settled-digit frame drawn at `finished`. */
            goto skipped;
        }
        i++;
    }
finished:
    for (j = 0; j < digits; j++) {
        battle_gfx_draw_or_append_gpu_primitive((s32*)&DIGITS[bank][j]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&NEXT[bank][j]);
    }
    battle_gfx_draw_or_append_gpu_primitive((s32*)&areas[bank]);
    if (digits >= 4) {
        battle_gfx_draw_or_append_gpu_primitive((s32*)&g_require_reward_money_comma_polys[bank]);
    }
    battle_gfx_draw_or_append_gpu_primitive((s32*)&g_require_reward_money_sign_polys[bank]);
skipped:
    g_sound_effect_id_to_play = 0;
    i++;
    tick = 0;
    for (;;) {
        bank = i & 1;
        battle_thread_yield();
        battle_gfx_draw_or_append_gpu_primitive((s32*)&areas[bank + 2]);
        for (j = 0; j < digits; j++) {
            POLY_FT4* digit_quad;

            x = DIGITS[bank][j].x0;
            digit_quad = (POLY_FT4*)((s32)DIGITS + (j * 0x28 + bank * 0xf0));
            digit_quad->x0 = x;
            digit_quad->y0 = 0x80;
            digit_quad->x1 = x + 13;
            digit_quad->y1 = 0x80;
            digit_quad->x2 = x;
            digit_quad->y2 = 0x97;
            digit_quad->x3 = x + 13;
            digit_quad->y3 = 0x97;
            digit_quad->u0 = g_require_reward_money_digit_targets[j] * 13;
            digit_quad->v0 = 0x32;
            digit_quad->u1 = g_require_reward_money_digit_targets[j] * 13 + 13;
            digit_quad->v1 = 0x32;
            digit_quad->u2 = g_require_reward_money_digit_targets[j] * 13;
            digit_quad->v2 = 0x49;
            digit_quad->u3 = g_require_reward_money_digit_targets[j] * 13 + 13;
            digit_quad->v3 = 0x49;
            battle_gfx_draw_or_append_gpu_primitive((s32*)&DIGITS[bank][j]);
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&areas[bank]);
        tick++;
        if (digits >= 4) {
            battle_gfx_draw_or_append_gpu_primitive((s32*)&g_require_reward_money_comma_polys[bank]);
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&g_require_reward_money_sign_polys[bank]);
        if (tick == 60 || (*g_require_input_controller & PSX_PAD_CIRCLE)) {
            break;
        }
        i++;
    }
    battle_thread_set_parameters(thread_id, 0, 0, 1);
    i++;
    pass = 0x80;
    do {
        bank = i & 1;
        battle_thread_yield();
        battle_gfx_draw_or_append_gpu_primitive((s32*)&areas[bank + 2]);
        for (j = 0; j < digits; j++) {
            POLY_FT4* digit_quad;

            x = DIGITS[bank][j].x0;
            digit_quad = (POLY_FT4*)((s32)DIGITS + (j * 0x28 + bank * 0xf0));
            digit_quad->x0 = x;
            digit_quad->y0 = 0x80;
            digit_quad->x1 = x + 13;
            digit_quad->y1 = 0x80;
            digit_quad->x2 = x;
            digit_quad->y2 = 0x97;
            digit_quad->x3 = x + 13;
            digit_quad->y3 = 0x97;
            digit_quad->u0 = g_require_reward_money_digit_targets[j] * 13;
            digit_quad->v0 = 0x32;
            digit_quad->u1 = g_require_reward_money_digit_targets[j] * 13 + 13;
            digit_quad->v1 = 0x32;
            digit_quad->u2 = g_require_reward_money_digit_targets[j] * 13;
            digit_quad->v2 = 0x49;
            digit_quad->u3 = g_require_reward_money_digit_targets[j] * 13 + 13;
            digit_quad->v3 = 0x49;
            digit_quad->r0 = pass;
            digit_quad->g0 = pass;
            digit_quad->b0 = pass;
            battle_gfx_draw_or_append_gpu_primitive((s32*)&DIGITS[bank][j]);
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&areas[bank]);
        /* Loading the base first keeps `la g_require_reward_money_comma_polys` above bank * 40. */
        commas = g_require_reward_money_comma_polys;
        comma = commas + bank;
        sign = &g_require_reward_money_sign_polys[bank];
        comma->r0 = pass;
        comma->g0 = pass;
        comma->b0 = pass;
        sign->r0 = pass;
        sign->g0 = pass;
        sign->b0 = pass;
        if (digits >= 4) {
            battle_gfx_draw_or_append_gpu_primitive((s32*)comma);
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)sign);
        pass -= 12;
        i++;
    } while (pass >= 0x30);
    battle_thread_wait_until_inactive(thread_id);
    battle_thread_exit_current();
}
