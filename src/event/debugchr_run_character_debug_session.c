/* Character debug session: pick a unit, then edit its fields with the pad.
 *
 * Matching notes (all barriers below are zero-instruction and each is
 * required):
 *  - The step loops' `cur_u`/`cur_d` are separate single-set locals. Combine
 *    only splits `lh` into a fresh register plus a copy (the target's
 *    `lh v0; move a0,v0`) when the destination has no tracked nonzero bits;
 *    reusing the function-wide `cur` there loads it with a plain `lh`, and
 *    writing `cur` in the up-arm clamp computes `hi - 1` in `cur`'s register.
 *  - The memory clobber after `g_debugchr_editor_fields[15].destination[0] = 0xE`, the tied `mark` and `uv_lo`
 *    operands, the empty volatile asm in `prev_unit` and the two
 *    `"r"(clearable)` uses each fix a scheduling or allocation difference
 *    described where they appear.
 *  - `PRIM_AT` keeps the target's `addu rd,index,base` operand order.
 */
#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/debugchr.h"
#include "fft/event.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Non-volatile view of debugchr.h's volatile array: volatile accesses change this function. */
#define EDITOR_UNIT_IDS ((u8*)g_debugchr_editor_unit_ids)

typedef struct {
    s32 unk_00;   /* 0x00 */
    s32 unk_04;   /* 0x04 */
    s32 unk_08;   /* 0x08 */
    s32 redraw;   /* 0x0C */
    s32 counter;  /* 0x10 */
} debug_thread_t; /* 0x14 */

extern debug_thread_t g_debugchr_status_display_thread_params;
extern debug_thread_t g_debugchr_numeric_editor_thread_params;
extern debug_thread_t g_debugchr_character_status_frame_config;

void battle_thread_start(s32 thread_id, void (*function)(void));

/* `index + base`, not `base + index`: GCC canonicalises a PLUS of two
   registers, but a MINUS over a NEGATE keeps the source operand order, which
   is the target's `addu rd,index,base` at the five primitive bases. */
#define PRIM_AT(base, index) ((POLY_FT4*)((s32)((index) * sizeof(POLY_FT4)) - -(s32)(base)))

void debugchr_run_character_debug_session(void) {
    s32 battle_id;
    help_navigation_record_t* field;
    s32* input;
    POLY_FT4* cursor_v;
    POLY_FT4* shadow_v;
    POLY_FT4* cursor_prims;
    POLY_FT4* cursor_v_edit;
    POLY_FT4* shadow_v_edit;
    POLY_FT4* cursor_h;
    POLY_FT4* shadow_h;
    POLY_FT4* cursor_h_edit;
    POLY_FT4* shadow_h_edit;
    s32 buttons;
    s32 keys;
    s32 navkeys;
    s32 i;
    s32 row;
    s32 steps;
    s32 direction;
    s32 bob_y;
    s32 bob_x;
    s32 uv_lo;
    s32 uv_v_hi;
    s32 uv_v_far;
    s32 uv_h_hi;
    s32 uv_h_far;
    s32 uv_h_mid;
    s32 uv_s_hi;
    s32 uv_s_far;
    s32 uv_s_mid;
    s32 mark;
    s16* menu_state;
    u16 unit_index;
    s32 clearable;
    s32 span;
    s32 buffer;
    s32* hi_limit;
    s32* lo_limit;
    s32* up_lo;
    s32* up_hi;
    s32* up_clamp;
    s32* dn_lo;
    s32* dn_hi;
    s16* value;
    s16* down_value;
    s16 cur;
    s16 cur_u;
    s16 cur_d;
    s32 x;
    s32 y;
    s32 shadow_bob_x;

    g_debugchr_editor_active = 1;
    g_debugchr_panel_status_group_count = 2;
    for (i = 0; i < 0x15; i++) {
        battle_find_unit_data_pointer_for_entd_unit_id(battle_unit_get_stats_from_battle_id(i)->unit_id, &battle_id);
        if ((battle_id >= 0) && (i == battle_id)) {
            EDITOR_UNIT_IDS[i] = 1;
        } else {
            EDITOR_UNIT_IDS[i] = 0;
        }
    }
    for (i = 0; i < 0x15; i++) {
        if (EDITOR_UNIT_IDS[i] != 0) {
            g_debugchr_panel_selected_unit_data[5] = i;
            g_debugchr_panel_selected_unit_data[0] = i;
            g_debugchr_panel_selected_billboard.unit = i;
            debugchr_editor_load_selected_unit();
            g_debugchr_editor_selected_unit_slot = i;
            break;
        }
    }
    debugchr_editor_load_selected_unit();
    g_debugchr_editor_selected_unit_slot = 0;
    if (g_debugchr_editor_display_mode != 0) {
        battle_copy_bytes(g_debugchr_editor_template, g_debugchr_editor_template_special, 0xA8);
    } else {
        battle_copy_bytes(g_debugchr_editor_template, g_debugchr_editor_template_normal, 0xA8);
    }
    g_debugchr_editor_fields[15].destination[0] = 0xE;
    /* Same ordering constraint as debugchr_editor_run_unit_thread: without
     * the barrier GCC sinks this byte store past the thread-record address
     * setup. */
    __asm__ volatile("" : : : "memory");
    g_debugchr_status_display_thread_params.counter = 0;
    g_debugchr_numeric_editor_thread_params.counter = 0;
    g_debugchr_character_status_frame_config.counter = 0;
    battle_thread_start(8, debugchr_render_unit_status_panel_thread);
    battle_thread_set_parameters(8, (s32)&g_debugchr_status_display_thread_params, 0, 0);
    battle_thread_start(0xC, debugchr_editor_run_numeric_thread);
    battle_thread_set_parameters(0xC, (s32)&g_debugchr_numeric_editor_thread_params, 0, 0);
    battle_thread_start(9, debugchr_panel_run_character_status_thread);
    battle_thread_set_parameters(9, (s32)&g_debugchr_character_status_frame_config, 0, 0);
    row = 0;
    /* The unit menu and the field editor re-enter each other by label: an
       enclosing loop's notes reweight the allocation of the whole body. */
loop_outer:
    i = 0;
    /* Assigned here, in the preheader, rather than inside the loop: as a loop
     * body statement it becomes a loop.c movable, and moving it consumes
     * threshold budget that then makes a fourth invariant (&g_debugchr_editor_selected_unit_slot)
     * profitable, which the target does not hoist. Assigned outside the loop
     * it is not a movable at all, so loop.c still hoists exactly mark and the
     * constant 1, and this address is emitted into the preheader ahead of
     * them -- the target's emission order. */
    menu_state = &g_debugchr_panel_selected_unit_data[5];
    for (;;) {
        mark = 0xE;
        /* Tied launder: it invalidates cse's constant equivalence for
         * `mark`, which puts `mark` in $s1 and the hoisted `&g_debugchr_panel_selected_unit_data[5]`
         * in $s3, as the target has them. */
        __asm__("" : "=r"(mark) : "0"(mark));
        battle_thread_yield();
        debugchr_gfx_build_status_group_primitives((POLY_FT4*)g_debugchr_gfx_render_buffers[i & 1]);
        input = battle_script_get_controller_input_pointer(0);
        g_debugchr_input_controller = input;
        buttons = *input;
        if (buttons & PSX_PAD_CIRCLE) {
            goto editor;
        }
        if (buttons & PSX_PAD_CROSS) {
            battle_thread_set_parameters(8, 0, 0, 1);
            battle_thread_set_parameters(0xC, 0, 0, 1);
            battle_thread_set_parameters(9, 0, 0, 1);
            return;
        }
        if (buttons & PSX_PAD_R1) {
            g_debugchr_panel_status_animation[1] = 7;
            /* Label loops: a do-while's loop notes change the allocation. */
        next_unit:
            g_debugchr_editor_selected_unit_slot += 1;
            if (g_debugchr_editor_selected_unit_slot == 0x15) {
                g_debugchr_editor_selected_unit_slot = 0;
            }
            if (EDITOR_UNIT_IDS[g_debugchr_editor_selected_unit_slot] == 0) {
                goto next_unit;
            }
            unit_index = g_debugchr_editor_selected_unit_slot;
            menu_state[0] = unit_index;
            menu_state[-5] = unit_index;
            g_debugchr_panel_selected_billboard.unit = unit_index;
            debugchr_editor_load_selected_unit();
            if (g_debugchr_editor_display_mode != 0) {
                battle_copy_bytes(g_debugchr_editor_template, g_debugchr_editor_template_special, 0xA8);
            } else {
                battle_copy_bytes(g_debugchr_editor_template, g_debugchr_editor_template_normal, 0xA8);
            }
            g_debugchr_editor_fields[15].destination[0] = mark;
            g_debugchr_status_display_thread_params.redraw = 1;
            g_debugchr_numeric_editor_thread_params.redraw = 1;
            g_debugchr_character_status_frame_config.redraw = 1;
        }
        if (*g_debugchr_input_controller & PSX_PAD_L1) {
            g_debugchr_panel_status_animation[0] = 7;
        prev_unit:
            g_debugchr_editor_selected_unit_slot -= 1;
            if (g_debugchr_editor_selected_unit_slot == -1) {
                g_debugchr_editor_selected_unit_slot = 0x14;
            }
            if (EDITOR_UNIT_IDS[g_debugchr_editor_selected_unit_slot] == 0) {
                goto prev_unit;
            }
            unit_index = g_debugchr_editor_selected_unit_slot;
            menu_state[0] = unit_index;
            menu_state[-5] = unit_index;
            g_debugchr_panel_selected_billboard.unit = unit_index;
            debugchr_editor_load_selected_unit();
            if (g_debugchr_editor_display_mode != 0) {
                battle_copy_bytes(g_debugchr_editor_template, g_debugchr_editor_template_special, 0xA8);
            } else {
                battle_copy_bytes(g_debugchr_editor_template, g_debugchr_editor_template_normal, 0xA8);
            }
            g_debugchr_editor_fields[15].destination[0] = mark;
            /* Keeps the `li v0,2` for the redraw store below the byte store. */
            __asm__ volatile("");
            g_debugchr_status_display_thread_params.redraw = 2;
            g_debugchr_numeric_editor_thread_params.redraw = 1;
            g_debugchr_character_status_frame_config.redraw = 1;
        }
        i++;
    }
editor:
    for (i = 0; i < 2; i++) {
        battle_gfx_init_default_poly_ft4(&g_debugchr_editor_cursor_polys[i]);
        SetSemiTrans(&g_debugchr_editor_cursor_polys[i], 0);
        g_debugchr_editor_cursor_polys[i].tpage = GetTPage(0, 0, 0x3C0, 0x100);
        g_debugchr_editor_cursor_polys[i].clut = 0x7D7C;
        battle_gfx_init_default_poly_ft4(&g_debugchr_editor_cursor_shade_polys[i]);
        SetSemiTrans(&g_debugchr_editor_cursor_shade_polys[i], 1);
        g_debugchr_editor_cursor_shade_polys[i].tpage = GetTPage(0, 2, 0x3C0, 0x100);
        g_debugchr_editor_cursor_shade_polys[i].clut = 0x7DBC;
    }
    g_debugchr_editor_field_table = g_debugchr_editor_fields;
    cursor_prims = g_debugchr_editor_cursor_polys;
    i = 0;
    battle_thread_yield();
    for (;;) {
        field = &g_debugchr_editor_field_table[row];
        x = field->x;
        y = field->y + 8;
        g_debugchr_editor_current_field = field;
        keys = *g_debugchr_input_controller;
        if (keys & PSX_PAD_CROSS) {
            if (g_debugchr_editor_field_edit_active != 1) {
                goto loop_outer;
            }
            g_debugchr_editor_field_edit_active = 0;
            debugchr_editor_apply_values_to_battle_unit(g_debugchr_editor_selected_unit_slot);
            debugchr_editor_load_selected_unit();
        } else if (keys & PSX_PAD_CIRCLE) {
            if (g_debugchr_editor_field_edit_active == 1) {
                g_debugchr_editor_field_edit_active = 0;
                debugchr_editor_apply_values_to_battle_unit(g_debugchr_editor_selected_unit_slot);
                debugchr_editor_load_selected_unit();
            } else {
                g_debugchr_editor_field_edit_active = 1;
            }
        }
        if ((g_debugchr_editor_field_edit_active != 0) && ((u32)(row - 9) < 5U)
            && (*g_debugchr_input_controller & PSX_PAD_TRIANGLE)) {
            *g_debugchr_editor_current_field->value = 0;
            debugchr_editor_apply_values_to_battle_unit(g_debugchr_editor_selected_unit_slot);
            debugchr_editor_load_selected_unit();
            g_debugchr_status_display_thread_params.redraw = 1;
            g_debugchr_numeric_editor_thread_params.redraw = 1;
            g_debugchr_character_status_frame_config.redraw = 1;
        }
        navkeys = *g_debugchr_input_controller;
        if (navkeys & PSX_PAD_DPAD_MASK) {
            if (g_debugchr_editor_field_edit_active == 0) {
                /* The `do { } while (0)` is a register-allocation lever, not
                 * control flow: flow.c adds the loop depth to reg_n_refs for
                 * every pseudo referenced inside a loop note, which raises
                 * `direction` above `navkeys` in allocno_compare's order so that
                 * `direction` is allocated first and takes $v1, leaving `navkeys`
                 * in $a0 as the target has it. */
                do {
                    direction = 0;
                } while (0);
                if (!(navkeys & PSX_PAD_UP)) {
                    if (navkeys & PSX_PAD_DOWN) {
                        direction = 1;
                    } else {
                        direction = 3;
                        if (navkeys & PSX_PAD_RIGHT) {
                            direction = 2;
                        }
                    }
                }
                row = g_debugchr_editor_current_field->destination[direction];
            } else {
                if ((navkeys & PSX_PAD_RIGHT) || (navkeys & PSX_PAD_UP)) {
                    value = g_debugchr_editor_current_field->value;
                    cur = *value;
                    /* One pointer local per basic block: a shared one loses the
                     * target's per-block address materialisation. */
                    /* `||` instead of the goto moves the upper-bound address
                       out of its own block. */
                    up_lo = &g_debugchr_editor_template[row].lo;
                    if (cur < *up_lo) {
                        goto clamp_hi;
                    }
                    up_hi = &g_debugchr_editor_template[row].hi;
                    if (*up_hi < cur) {
                    clamp_hi:
                        up_clamp = &g_debugchr_editor_template[row].hi;
                        *value = *up_clamp - 1;
                    }
                    if (PadRead(1) & 4) {
                        steps = 0xA;
                    } else {
                        steps = 1;
                    }
                    for (i = 0; i < steps; i++) {
                        hi_limit = &g_debugchr_editor_template[row].hi;
                        span = row - 9;
                        clearable = (u32)span < 5U;
                        /* The target hoists this clear-to-zero range test into
                         * the preheader and never reads the result; the empty
                         * "r" operand keeps the use alive without emitting an
                         * instruction. */
                        __asm__("" : : "r"(clearable));
                        value = g_debugchr_editor_current_field->value;
                        cur_u = *value;
                        if (cur_u < *hi_limit) {
                            *value = cur_u + 1;
                            if (row == 0) {
                                *g_debugchr_editor_current_field->value -= 1;
                                debugchr_editor_apply_values_to_battle_unit(g_debugchr_editor_selected_unit_slot);
                                main_unit_apply_level_growth(
                                    battle_unit_get_stats_from_battle_id(g_debugchr_editor_selected_unit_slot), 0);
                                *g_debugchr_editor_current_field->value += 1;
                            }
                            debugchr_editor_apply_values_to_battle_unit(g_debugchr_editor_selected_unit_slot);
                            debugchr_editor_load_selected_unit();
                        }
                    }
                } else {
                    down_value = g_debugchr_editor_current_field->value;
                    cur = *down_value;
                    dn_lo = &g_debugchr_editor_template[row].lo;
                    if (cur < *dn_lo) {
                        goto clamp_lo;
                    }
                    dn_hi = &g_debugchr_editor_template[row].hi;
                    if (*dn_hi < cur) {
                    clamp_lo:
                        *down_value = *dn_lo + 1;
                    }
                    if (PadRead(1) & 4) {
                        steps = 0xA;
                    } else {
                        steps = 1;
                    }
                    for (i = 0; i < steps; i++) {
                        lo_limit = &g_debugchr_editor_template[row].lo;
                        span = row - 9;
                        clearable = (u32)span < 5U;
                        /* The target hoists this clear-to-zero range test into
                         * the preheader and never reads the result; the empty
                         * "r" operand keeps the use alive without emitting an
                         * instruction. */
                        __asm__("" : : "r"(clearable));
                        value = g_debugchr_editor_current_field->value;
                        cur_d = *value;
                        if (*lo_limit < cur_d) {
                            *value = cur_d - 1;
                            if (row == 0) {
                                *g_debugchr_editor_current_field->value += 1;
                                debugchr_editor_apply_values_to_battle_unit(g_debugchr_editor_selected_unit_slot);
                                main_unit_apply_level_growth(
                                    battle_unit_get_stats_from_battle_id(g_debugchr_editor_selected_unit_slot), 1);
                                *g_debugchr_editor_current_field->value -= 1;
                            }
                            debugchr_editor_apply_values_to_battle_unit(g_debugchr_editor_selected_unit_slot);
                            debugchr_editor_load_selected_unit();
                        }
                    }
                }
                g_debugchr_status_display_thread_params.redraw = 1;
                g_debugchr_numeric_editor_thread_params.redraw = 1;
                g_debugchr_character_status_frame_config.redraw = 1;
            }
        }
        buffer = i & 1;
        if (g_debugchr_editor_current_field->vertical_cursor != 0) {
            uv_lo = 0xA8;
            g_debugchr_editor_cursor_polys[buffer].u0 = uv_lo;
            g_debugchr_editor_cursor_polys[buffer].u1 = uv_lo;
            uv_v_hi = 0xB8;
            g_debugchr_editor_cursor_polys[buffer].u2 = uv_v_hi;
            g_debugchr_editor_cursor_polys[buffer].u3 = uv_v_hi;
            g_debugchr_editor_cursor_polys[buffer].v0 = 0;
            g_debugchr_editor_cursor_polys[buffer].v1 = 0x10;
            g_debugchr_editor_cursor_polys[buffer].v2 = 0;
            g_debugchr_editor_cursor_polys[buffer].v3 = 0x10;
            uv_lo = 0xB8;
            g_debugchr_editor_cursor_shade_polys[buffer].u0 = uv_lo;
            g_debugchr_editor_cursor_shade_polys[buffer].u1 = uv_lo;
            uv_v_far = 0xC8;
            g_debugchr_editor_cursor_shade_polys[buffer].u2 = uv_v_far;
            g_debugchr_editor_cursor_shade_polys[buffer].u3 = uv_v_far;
            g_debugchr_editor_cursor_shade_polys[buffer].v0 = 0;
            g_debugchr_editor_cursor_shade_polys[buffer].v1 = 0x10;
            g_debugchr_editor_cursor_shade_polys[buffer].v2 = 0;
            g_debugchr_editor_cursor_shade_polys[buffer].v3 = 0x10;
            if (g_debugchr_editor_field_edit_active == 0) {
                cursor_v = PRIM_AT(cursor_prims, buffer);
                shadow_v = &g_debugchr_editor_cursor_shade_polys[buffer];
                bob_y = y - (((i * g_battle_event_speed) >> 4) & 3);
                cursor_v->x0 = x;
                cursor_v->y0 = bob_y;
                cursor_v->x1 = x + 0x10;
                cursor_v->y1 = bob_y;
                cursor_v->x2 = x;
                cursor_v->y2 = bob_y + 0x10;
                cursor_v->x3 = x + 0x10;
                cursor_v->y3 = bob_y + 0x10;
                shadow_v->x0 = x + 2;
                shadow_v->y0 = bob_y + 2;
                shadow_v->x1 = x + 0x12;
                shadow_v->y1 = bob_y + 2;
                shadow_v->x2 = x + 2;
                shadow_v->y2 = bob_y + 0x12;
                shadow_v->x3 = x + 0x12;
                shadow_v->y3 = bob_y + 0x12;
                g_debugchr_editor_cursor_polys[buffer].clut = 0x7D7C;
            } else {
                cursor_v_edit = PRIM_AT(cursor_prims, buffer);
                shadow_v_edit = &g_debugchr_editor_cursor_shade_polys[buffer];
                cursor_v_edit->x0 = x;
                cursor_v_edit->y0 = y;
                cursor_v_edit->x1 = x + 0x10;
                cursor_v_edit->y1 = y;
                cursor_v_edit->x2 = x;
                cursor_v_edit->y2 = y + 0x10;
                cursor_v_edit->x3 = x + 0x10;
                cursor_v_edit->y3 = y + 0x10;
                shadow_v_edit->x0 = x + 2;
                shadow_v_edit->y0 = y + 2;
                shadow_v_edit->x1 = x + 0x12;
                shadow_v_edit->y1 = y + 2;
                shadow_v_edit->x2 = x + 2;
                shadow_v_edit->y2 = y + 0x12;
                shadow_v_edit->x3 = x + 0x12;
                shadow_v_edit->y3 = y + 0x12;
                g_debugchr_editor_cursor_polys[buffer].clut = 0x7E7C;
            }
        } else if (g_debugchr_editor_field_edit_active == 0) {
            cursor_h = PRIM_AT(cursor_prims, buffer);
            uv_lo = 0xA8;
            cursor_h->u0 = uv_lo;
            uv_lo = 0xB8;
            /* Hides the constant from cse; without it this block loses two of
             * the target's instructions. */
            __asm__("" : "=r"(uv_lo) : "0"(uv_lo));
            cursor_h->v0 = 0;
            uv_h_hi = 0xB8;
            cursor_h->u1 = uv_h_hi;
            shadow_h = &g_debugchr_editor_cursor_shade_polys[buffer];
            cursor_h->v1 = 0;
            uv_h_mid = 0xA8;
            cursor_h->u2 = uv_h_mid;
            cursor_h->v2 = 0x10;
            cursor_h->u3 = uv_h_hi;
            cursor_h->v3 = 0x10;
            shadow_h->u0 = uv_lo;
            shadow_h->v0 = 0;
            uv_h_far = 0xC8;
            shadow_h->u1 = uv_h_far;
            shadow_h->v1 = 0;
            shadow_h->u2 = uv_h_hi;
            shadow_h->v2 = 0x10;
            shadow_h->u3 = uv_h_far;
            shadow_h->v3 = 0x10;
            bob_x = ((i * g_battle_event_speed) >> 4) & 3;
            cursor_h->x0 = x - bob_x;
            cursor_h->y0 = y;
            cursor_h->x1 = (x - bob_x) + 0x10;
            cursor_h->y1 = y;
            cursor_h->x2 = x - bob_x;
            cursor_h->y2 = y + 0x10;
            cursor_h->x3 = (x - bob_x) + 0x10;
            cursor_h->y3 = y + 0x10;
            /* A separate temporary schedules the decrement after the
             * `(x - bob_x) + 0x10` sum; `bob_x -= 2` would move it ahead. */
            shadow_bob_x = bob_x - 2;
            shadow_h->x0 = x - shadow_bob_x;
            shadow_h->y0 = y + 2;
            shadow_h->x1 = (x - shadow_bob_x) + 0x10;
            shadow_h->y1 = y + 2;
            shadow_h->x2 = x - shadow_bob_x;
            shadow_h->y2 = y + 0x12;
            shadow_h->x3 = (x - shadow_bob_x) + 0x10;
            shadow_h->y3 = y + 0x12;
            g_debugchr_editor_cursor_polys[buffer].clut = 0x7D7C;
        } else {
            cursor_h_edit = PRIM_AT(cursor_prims, buffer);
            shadow_h_edit = &g_debugchr_editor_cursor_shade_polys[buffer];
            uv_lo = 0xA8;
            cursor_h_edit->u0 = uv_lo;
            uv_lo = 0xB8;
            /* Hides the constant from cse; without it this block loses two of
             * the target's instructions. */
            __asm__("" : "=r"(uv_lo) : "0"(uv_lo));
            cursor_h_edit->v0 = 0;
            uv_s_hi = 0xB8;
            cursor_h_edit->u1 = uv_s_hi;
            cursor_h_edit->v1 = 0;
            uv_s_mid = 0xA8;
            cursor_h_edit->u2 = uv_s_mid;
            cursor_h_edit->v2 = 0x10;
            cursor_h_edit->u3 = uv_s_hi;
            cursor_h_edit->v3 = 0x10;
            shadow_h_edit->u0 = uv_lo;
            shadow_h_edit->v0 = 0;
            uv_s_far = 0xC8;
            shadow_h_edit->u1 = uv_s_far;
            shadow_h_edit->v1 = 0;
            shadow_h_edit->u2 = uv_s_hi;
            shadow_h_edit->v2 = 0x10;
            shadow_h_edit->u3 = uv_s_far;
            shadow_h_edit->v3 = 0x10;
            cursor_h_edit->x0 = x;
            cursor_h_edit->y0 = y;
            cursor_h_edit->x1 = x + 0x10;
            cursor_h_edit->y1 = y;
            cursor_h_edit->x2 = x;
            cursor_h_edit->y2 = y + 0x10;
            cursor_h_edit->x3 = x + 0x10;
            cursor_h_edit->y3 = y + 0x10;
            shadow_h_edit->x0 = x + 2;
            shadow_h_edit->y0 = y + 2;
            shadow_h_edit->x1 = x + 0x12;
            shadow_h_edit->y1 = y + 2;
            shadow_h_edit->x2 = x + 2;
            shadow_h_edit->y2 = y + 0x12;
            shadow_h_edit->x3 = x + 0x12;
            shadow_h_edit->y3 = y + 0x12;
            g_debugchr_editor_cursor_polys[buffer].clut = 0x7E7C;
        }
        battle_gfx_draw_or_append_gpu_primitive(PRIM_AT(cursor_prims, buffer));
        battle_gfx_draw_or_append_gpu_primitive(&g_debugchr_editor_cursor_shade_polys[buffer]);
        battle_thread_wait_frames(1);
        i++;
    }
}
