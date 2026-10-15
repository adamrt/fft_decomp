#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

#define X0 (0x108 - j * 13 - (5 - digits) * 13 / 2)

/*
 * Post-battle gil ticker: adds the war-result money to script variable 0x2c,
 * starts the display-condition thread, splits the amount into up to six
 * digits and rolls each digit's odometer strip (13-pixel cells, counter << 13
 * fixed point, slowing by 0x44 per step once the target digit comes round)
 * before holding for 60 frames and fading out.
 *
 * The polygon pointers are formed as `(s32)array + offset` byte offsets, which
 * keeps the target's separate base and offset; `&g_require_reward_money_digit_polys[buffer][j]` is reduced
 * to a single pointer induction variable. `pass` doubles as the fade level,
 * which the target's register allocation reflects.
 */
void require_reward_run_gil_ticker(void) {
    DR_AREA areas[4];
    s32 thread_id;
    s32 money;
    s32 digits;
    s32 i;
    s32 j;
    s32 buffer;
    s32 tick;
    s32 pass;
    s32 x;
    s32 off;
    POLY_FT4* commas;
    POLY_FT4* comma;
    POLY_FT4* sign;

    g_require_reward_war_trophy_entry_count = 0;
    battle_unit_find_war_trophies_and_bonus_money(&g_require_reward_war_trophy_search_result);
    script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS,
        script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + g_require_reward_war_trophy_search_result.level_sum_money);
    money = g_require_reward_war_trophy_search_result.level_sum_money;
    thread_id = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_id, require_condition_run_special_thread);
    thread_set_parameters(thread_id, 2, 0, 0);
    g_menu_input_controller = script_get_controller_input_pointer(0);
    /* `i = 0` here (not before the script calls) lets sched1 keep
     * `li a0,0x2c` above the money load. */
    for (i = 0; i < 6; i++) {
        g_require_reward_money_digit_speeds[i] = g_require_reward_money_digit_initial_speeds[i];
        g_require_reward_money_digit_settling[i] = 0;
    }
    digits = text_count_decimal_digits(money);
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
    g_sound_pending_sfx_id = MAIN_SFX_GIL_TICKER;
    for (buffer = 0; buffer < 2; buffer++) {
        for (j = 0; j < digits; j++) {
            POLY_FT4* pd;
            POLY_FT4* pn;

            menu_init_default_poly_ft4(&g_require_reward_money_digit_polys[buffer][j]);
            menu_init_default_poly_ft4(&g_require_reward_money_next_digit_polys[buffer][j]);
            g_require_reward_money_digit_polys[buffer][j].tpage = getTPage(0, 0, 384, 0);
            g_require_reward_money_digit_polys[buffer][j].clut
                = getClut(g_require_reward_bonus_palette_rect.x, g_require_reward_bonus_palette_rect.y);
            g_require_reward_money_next_digit_polys[buffer][j].tpage = getTPage(0, 0, 384, 0);
            g_require_reward_money_next_digit_polys[buffer][j].clut
                = getClut(g_require_reward_bonus_palette_rect.x, g_require_reward_bonus_palette_rect.y);
            pd = (POLY_FT4*)((s32)g_require_reward_money_digit_polys + (j * 0x28 + buffer * 0xf0));
            setXY4(pd, X0, 0x80, X0 + 13, 0x80, X0, 0x97, X0 + 13, 0x97);
            pn = (POLY_FT4*)((s32)g_require_reward_money_next_digit_polys + (j * 0x28 + buffer * 0xf0));
            setXY4(pn, X0, 0x69, X0 + 13, 0x69, X0, 0x80, X0 + 13, 0x80);
            if (j >= 3) {
                g_require_reward_money_digit_polys[buffer][j].x0 -= 8;
                g_require_reward_money_next_digit_polys[buffer][j].x0 -= 8;
            }
        }
    }
    for (i = 0; i < 2; i++) {
        /* Block-local pointers: local-alloc then builds each address in the
         * pointer's own register, as the target does. */
        POLY_FT4* comma_poly;
        POLY_FT4* sign_poly;

        comma_poly = &g_require_reward_money_comma_polys[i];
        menu_init_default_poly_ft4(comma_poly);
        setXY4(comma_poly, g_require_reward_money_digit_polys[0][2].x0 - 10, 0x8f,
            g_require_reward_money_digit_polys[0][2].x0 - 4, 0x8f, g_require_reward_money_digit_polys[0][2].x0 - 10,
            0x99, g_require_reward_money_digit_polys[0][2].x0 - 4, 0x99);
        setUV4(comma_poly, 0x82, 0x40, 0x88, 0x40, 0x82, 0x4a, 0x88, 0x4a);
        g_require_reward_money_comma_polys[i].tpage = getTPage(0, 0, 384, 0);
        g_require_reward_money_comma_polys[i].clut
            = getClut(g_require_reward_bonus_palette_rect.x, g_require_reward_bonus_palette_rect.y);
        sign_poly = &g_require_reward_money_sign_polys[i];
        menu_init_default_poly_ft4(sign_poly);
        setXY4(sign_poly, g_require_reward_money_digit_polys[0][0].x0 + 0x11, 0x80,
            g_require_reward_money_digit_polys[0][0].x0 + 0x31, 0x80,
            g_require_reward_money_digit_polys[0][0].x0 + 0x11, 0x97,
            g_require_reward_money_digit_polys[0][0].x0 + 0x31, 0x97);
        setUV4(sign_poly, 0x88, 0x32, 0xa8, 0x32, 0x88, 0x49, 0xa8, 0x49);
        g_require_reward_money_sign_polys[i].tpage = getTPage(0, 0, 384, 0);
        g_require_reward_money_sign_polys[i].clut
            = getClut(g_require_reward_bonus_palette_rect.x, g_require_reward_bonus_palette_rect.y);
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
        buffer = i & 1;
        thread_yield();
        gfx_draw_or_append_gpu_primitive(&areas[buffer + 2]);
        for (pass = 0; pass < 2; pass++, tick++) {
            for (j = 0; j < digits; j++) {
                POLY_FT4* pd;
                POLY_FT4* pn;

                g_require_reward_money_digit_positions[j] += g_require_reward_money_digit_speeds[j];
                off = j * 0x28 + buffer * 0xf0;
                x = g_require_reward_money_digit_polys[buffer][j].x0;
                pd = (POLY_FT4*)((s32)g_require_reward_money_digit_polys + off);
                setXY4(pd, x, ((g_require_reward_money_digit_positions[j] >> 8) & 0x1f) + 0x80, x + 13,
                    ((g_require_reward_money_digit_positions[j] >> 8) & 0x1f) + 0x80, x,
                    ((g_require_reward_money_digit_positions[j] >> 8) & 0x1f) + 0x97, x + 13,
                    ((g_require_reward_money_digit_positions[j] >> 8) & 0x1f) + 0x97);
                setUV4(pd, (g_require_reward_money_digit_positions[j] >> 13) % 10 * 13, 0x32,
                    (g_require_reward_money_digit_positions[j] >> 13) % 10 * 13 + 13, 0x32,
                    (g_require_reward_money_digit_positions[j] >> 13) % 10 * 13, 0x49,
                    (g_require_reward_money_digit_positions[j] >> 13) % 10 * 13 + 13, 0x49);
                x = g_require_reward_money_next_digit_polys[buffer][j].x0;
                off = j * 0x28 + buffer * 0xf0;
                pn = (POLY_FT4*)((s32)g_require_reward_money_next_digit_polys + off);
                setXY4(pn, x, g_require_reward_money_digit_polys[buffer][j].y0 - 0x20, x + 13,
                    g_require_reward_money_digit_polys[buffer][j].y0 - 0x20, x,
                    g_require_reward_money_digit_polys[buffer][j].y0 - 9, x + 13,
                    g_require_reward_money_digit_polys[buffer][j].y0 - 9);
                setUV4(pn, ((g_require_reward_money_digit_positions[j] >> 13) + 1) % 10 * 13, 0x32,
                    ((g_require_reward_money_digit_positions[j] >> 13) + 1) % 10 * 13 + 13, 0x32,
                    ((g_require_reward_money_digit_positions[j] >> 13) + 1) % 10 * 13, 0x49,
                    ((g_require_reward_money_digit_positions[j] >> 13) + 1) % 10 * 13 + 13, 0x49);
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
                gfx_draw_or_append_gpu_primitive(&g_require_reward_money_digit_polys[buffer][j]);
                gfx_draw_or_append_gpu_primitive(&g_require_reward_money_next_digit_polys[buffer][j]);
            }
        }
        gfx_draw_or_append_gpu_primitive(&areas[buffer]);
        if (digits >= 4 && i >= 0x31) {
            gfx_draw_or_append_gpu_primitive(&g_require_reward_money_comma_polys[buffer]);
        }
        gfx_draw_or_append_gpu_primitive(&g_require_reward_money_sign_polys[buffer]);
        if (*g_menu_input_controller & PSX_PAD_CIRCLE) {
            /* Confirm skips the settled-digit frame drawn at `finished`. */
            goto skipped;
        }
        i++;
    }
finished:
    for (j = 0; j < digits; j++) {
        gfx_draw_or_append_gpu_primitive(&g_require_reward_money_digit_polys[buffer][j]);
        gfx_draw_or_append_gpu_primitive(&g_require_reward_money_next_digit_polys[buffer][j]);
    }
    gfx_draw_or_append_gpu_primitive(&areas[buffer]);
    if (digits >= 4) {
        gfx_draw_or_append_gpu_primitive(&g_require_reward_money_comma_polys[buffer]);
    }
    gfx_draw_or_append_gpu_primitive(&g_require_reward_money_sign_polys[buffer]);
skipped:
    g_sound_pending_sfx_id = MAIN_SFX_NONE;
    i++;
    tick = 0;
    for (;;) {
        buffer = i & 1;
        thread_yield();
        gfx_draw_or_append_gpu_primitive(&areas[buffer + 2]);
        for (j = 0; j < digits; j++) {
            POLY_FT4* digit_quad;

            x = g_require_reward_money_digit_polys[buffer][j].x0;
            digit_quad = (POLY_FT4*)((s32)g_require_reward_money_digit_polys + (j * 0x28 + buffer * 0xf0));
            setXY4(digit_quad, x, 0x80, x + 13, 0x80, x, 0x97, x + 13, 0x97);
            setUV4(digit_quad, g_require_reward_money_digit_targets[j] * 13, 0x32,
                g_require_reward_money_digit_targets[j] * 13 + 13, 0x32, g_require_reward_money_digit_targets[j] * 13,
                0x49, g_require_reward_money_digit_targets[j] * 13 + 13, 0x49);
            gfx_draw_or_append_gpu_primitive(&g_require_reward_money_digit_polys[buffer][j]);
        }
        gfx_draw_or_append_gpu_primitive(&areas[buffer]);
        tick++;
        if (digits >= 4) {
            gfx_draw_or_append_gpu_primitive(&g_require_reward_money_comma_polys[buffer]);
        }
        gfx_draw_or_append_gpu_primitive(&g_require_reward_money_sign_polys[buffer]);
        if (tick == 60 || (*g_menu_input_controller & PSX_PAD_CIRCLE)) {
            break;
        }
        i++;
    }
    thread_set_parameters(thread_id, 0, 0, 1);
    i++;
    pass = 0x80;
    do {
        buffer = i & 1;
        thread_yield();
        gfx_draw_or_append_gpu_primitive(&areas[buffer + 2]);
        for (j = 0; j < digits; j++) {
            POLY_FT4* digit_quad;

            x = g_require_reward_money_digit_polys[buffer][j].x0;
            digit_quad = (POLY_FT4*)((s32)g_require_reward_money_digit_polys + (j * 0x28 + buffer * 0xf0));
            setXY4(digit_quad, x, 0x80, x + 13, 0x80, x, 0x97, x + 13, 0x97);
            setUV4(digit_quad, g_require_reward_money_digit_targets[j] * 13, 0x32,
                g_require_reward_money_digit_targets[j] * 13 + 13, 0x32, g_require_reward_money_digit_targets[j] * 13,
                0x49, g_require_reward_money_digit_targets[j] * 13 + 13, 0x49);
            setRGB0(digit_quad, pass, pass, pass);
            gfx_draw_or_append_gpu_primitive(&g_require_reward_money_digit_polys[buffer][j]);
        }
        gfx_draw_or_append_gpu_primitive(&areas[buffer]);
        /* Loading the base first keeps `la g_require_reward_money_comma_polys` above buffer * 40. */
        commas = g_require_reward_money_comma_polys;
        comma = commas + buffer;
        sign = &g_require_reward_money_sign_polys[buffer];
        setRGB0(comma, pass, pass, pass);
        setRGB0(sign, pass, pass, pass);
        if (digits >= 4) {
            gfx_draw_or_append_gpu_primitive(comma);
        }
        gfx_draw_or_append_gpu_primitive(sign);
        pass -= 12;
        i++;
    } while (pass >= 0x30);
    thread_wait_until_inactive(thread_id);
    thread_exit_current();
}
