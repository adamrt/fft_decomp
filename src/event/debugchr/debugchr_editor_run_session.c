#include "fft/gfx.h"
#include "fft/memory.h"
/* Select a unit and edit its fields with the controller. */
#include "fft/battle_script.h"
#include "fft/event_debugchr.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Ordinary indexing changes the ADDU operand order at the primitive bases. */
#define PRIM_AT(base, index) ((POLY_FT4*)((s32)((index) * sizeof(POLY_FT4)) - -(s32)(base)))

void debugchr_editor_run_session(void) {
    s32 battle_id;
    help_navigation_record_t* field;
    u32* input;
    POLY_FT4* cursor_v;
    POLY_FT4* shadow_v;
    POLY_FT4* cursor_prims;
    POLY_FT4* cursor_v_edit;
    POLY_FT4* shadow_v_edit;
    POLY_FT4* cursor_h;
    POLY_FT4* shadow_h;
    POLY_FT4* cursor_h_edit;
    POLY_FT4* shadow_h_edit;
    u32 buttons;
    u32 keys;
    u32 navkeys;
    s32 i;
    s32 row;
    s32 steps;
    s32 direction;
    s32 bob_y;
    s32 bob_x;
    u8 uv_lo;
    u8 uv_v_hi;
    u8 uv_v_far;
    u8 uv_h_hi;
    u8 uv_h_far;
    u8 uv_h_mid;
    u8 uv_s_hi;
    u8 uv_s_far;
    u8 uv_s_mid;
    u8 mark;
    s16* selected_battle_id;
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
    g_menu_panel_status_group_count = 2;
    for (i = 0; i < 0x15; i++) {
        battle_unit_find_stats_by_entd_unit_id(battle_unit_get_stats_from_battle_id(i)->unit_id, &battle_id);
        if ((battle_id >= 0) && (i == battle_id)) {
            g_debugchr_editor_unit_ids[i] = 1;
        } else {
            g_debugchr_editor_unit_ids[i] = 0;
        }
    }
    for (i = 0; i < 0x15; i++) {
        if (g_debugchr_editor_unit_ids[i] != 0) {
            g_debugchr_panel_selected_unit_data.battle_id = i;
            g_debugchr_panel_selected_unit_data.name_id = i;
            g_debugchr_panel_selected_billboard.unit_index = i;
            debugchr_editor_load_selected_unit();
            g_debugchr_editor_selected_unit_slot = i;
            break;
        }
    }
    debugchr_editor_load_selected_unit();
    g_debugchr_editor_selected_unit_slot = 0;
    if (g_menu_status_panel_fields.state.generic_monster != 0) {
        mem_copy_bytes_nonempty(g_debugchr_editor_template, g_debugchr_editor_template_special, 0xA8);
        g_debugchr_editor_fields[15].destination[0] = 0xE;
    } else {
        mem_copy_bytes_nonempty(g_debugchr_editor_template, g_debugchr_editor_template_normal, 0xA8);
        g_debugchr_editor_fields[15].destination[0] = 0xE;
    }
    g_debugchr_status_display_thread_params.style = 0;
    g_debugchr_numeric_editor_thread_params.style = 0;
    g_debugchr_character_status_frame_config.style = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, debugchr_panel_run_unit_status_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&g_debugchr_status_display_thread_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, debugchr_editor_run_numeric_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (s32)&g_debugchr_numeric_editor_thread_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_status_panel_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_debugchr_character_status_frame_config, 0, 0);
    row = 0;
    /* The unit menu and the field editor re-enter each other by label: an
       enclosing loop's notes reweight the allocation of the whole body. */
loop_outer:
    i = 0;
    /* name_id is five 16-bit values before battle_id; record access changes the store displacements. */
    selected_battle_id = &g_debugchr_panel_selected_unit_data.battle_id;
    for (;;) {
        mark = 0xE;
        /* Removing the tied use changes the marker and menu-pointer allocation. */
        __asm__("" : "=r"(mark) : "0"(mark));
        thread_yield();
        menu_build_status_group_primitives(g_debugchr_gfx_render_buffers[i & 1]);
        input = script_get_controller_input_pointer(0);
        g_menu_input_controller = input;
        buttons = *input;
        if (buttons & PSX_PAD_CIRCLE) {
            goto editor;
        }
        if (buttons & PSX_PAD_CROSS) {
            thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
            return;
        }
        if (buttons & PSX_PAD_R1) {
            g_menu_panel_status_animation[1] = 7;
            /* Label loops: a do-while's loop notes change the allocation. */
        next_unit:
            g_debugchr_editor_selected_unit_slot += 1;
            if (g_debugchr_editor_selected_unit_slot == 0x15) {
                g_debugchr_editor_selected_unit_slot = 0;
            }
            if (g_debugchr_editor_unit_ids[g_debugchr_editor_selected_unit_slot] == 0) {
                goto next_unit;
            }
            unit_index = g_debugchr_editor_selected_unit_slot;
            selected_battle_id[0] = unit_index;
            selected_battle_id[-5] = unit_index;
            g_debugchr_panel_selected_billboard.unit_index = unit_index;
            debugchr_editor_load_selected_unit();
            if (g_menu_status_panel_fields.state.generic_monster != 0) {
                mem_copy_bytes_nonempty(g_debugchr_editor_template, g_debugchr_editor_template_special, 0xA8);
            } else {
                mem_copy_bytes_nonempty(g_debugchr_editor_template, g_debugchr_editor_template_normal, 0xA8);
            }
            g_debugchr_editor_fields[15].destination[0] = mark;
            g_debugchr_status_display_thread_params.redraw_request = 1;
            g_debugchr_numeric_editor_thread_params.redraw_request = 1;
            g_debugchr_character_status_frame_config.redraw_request = 1;
        }
        if (*g_menu_input_controller & PSX_PAD_L1) {
            g_menu_panel_status_animation[0] = 7;
        prev_unit:
            g_debugchr_editor_selected_unit_slot -= 1;
            if (g_debugchr_editor_selected_unit_slot == -1) {
                g_debugchr_editor_selected_unit_slot = 0x14;
            }
            if (g_debugchr_editor_unit_ids[g_debugchr_editor_selected_unit_slot] == 0) {
                goto prev_unit;
            }
            unit_index = g_debugchr_editor_selected_unit_slot;
            selected_battle_id[0] = unit_index;
            selected_battle_id[-5] = unit_index;
            g_debugchr_panel_selected_billboard.unit_index = unit_index;
            debugchr_editor_load_selected_unit();
            if (g_menu_status_panel_fields.state.generic_monster != 0) {
                mem_copy_bytes_nonempty(g_debugchr_editor_template, g_debugchr_editor_template_special, 0xA8);
            } else {
                mem_copy_bytes_nonempty(g_debugchr_editor_template, g_debugchr_editor_template_normal, 0xA8);
            }
            g_debugchr_editor_fields[15].destination[0] = mark;
            /* Keeps the `li v0,2` for the redraw store below the byte store. */
            __asm__ volatile("");
            g_debugchr_status_display_thread_params.redraw_request = 2;
            g_debugchr_numeric_editor_thread_params.redraw_request = 1;
            g_debugchr_character_status_frame_config.redraw_request = 1;
        }
        i++;
    }
editor:
    for (i = 0; i < 2; i++) {
        menu_init_default_poly_ft4(&g_debugchr_editor_cursor_polys[i]);
        SetSemiTrans(&g_debugchr_editor_cursor_polys[i], 0);
        g_debugchr_editor_cursor_polys[i].tpage = GetTPage(0, 0, 0x3C0, 0x100);
        g_debugchr_editor_cursor_polys[i].clut = 0x7D7C;
        menu_init_default_poly_ft4(&g_debugchr_editor_cursor_shade_polys[i]);
        SetSemiTrans(&g_debugchr_editor_cursor_shade_polys[i], 1);
        g_debugchr_editor_cursor_shade_polys[i].tpage = GetTPage(0, 2, 0x3C0, 0x100);
        g_debugchr_editor_cursor_shade_polys[i].clut = 0x7DBC;
    }
    g_debugchr_editor_field_table = g_debugchr_editor_fields;
    cursor_prims = g_debugchr_editor_cursor_polys;
    i = 0;
    thread_yield();
    for (;;) {
        field = &g_debugchr_editor_field_table[row];
        x = field->x;
        y = field->y + 8;
        g_debugchr_editor_current_field = field;
        keys = *g_menu_input_controller;
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
        if ((g_debugchr_editor_field_edit_active != 0) && ((row - 9) < 5U)
            && (*g_menu_input_controller & PSX_PAD_TRIANGLE)) {
            *g_debugchr_editor_current_field->value = 0;
            debugchr_editor_apply_values_to_battle_unit(g_debugchr_editor_selected_unit_slot);
            debugchr_editor_load_selected_unit();
            g_debugchr_status_display_thread_params.redraw_request = 1;
            g_debugchr_numeric_editor_thread_params.redraw_request = 1;
            g_debugchr_character_status_frame_config.redraw_request = 1;
        }
        navkeys = *g_menu_input_controller;
        if (navkeys & PSX_PAD_DPAD_MASK) {
            if (g_debugchr_editor_field_edit_active == 0) {
                /* Removing the wrapper changes direction and input-mask allocation. */
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
                    /* Shared pointers or a combined range test change address materialization. */
                    up_lo = &g_debugchr_editor_template[row].min;
                    if (cur < *up_lo) {
                        goto clamp_hi;
                    }
                    up_hi = &g_debugchr_editor_template[row].max;
                    if (*up_hi < cur) {
                    clamp_hi:
                        up_clamp = &g_debugchr_editor_template[row].max;
                        *value = *up_clamp - 1;
                    }
                    if (PadRead(1) & PSX_PAD_L1) {
                        steps = 0xA;
                    } else {
                        steps = 1;
                    }
                    for (i = 0; i < steps; i++) {
                        hi_limit = &g_debugchr_editor_template[row].max;
                        span = row - 9;
                        clearable = span < 5U;
                        /* The native loop computes this range test without reading its result. */
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
                    dn_lo = &g_debugchr_editor_template[row].min;
                    if (cur < *dn_lo) {
                        goto clamp_lo;
                    }
                    dn_hi = &g_debugchr_editor_template[row].max;
                    if (*dn_hi < cur) {
                    clamp_lo:
                        *down_value = *dn_lo + 1;
                    }
                    if (PadRead(1) & PSX_PAD_L1) {
                        steps = 0xA;
                    } else {
                        steps = 1;
                    }
                    for (i = 0; i < steps; i++) {
                        lo_limit = &g_debugchr_editor_template[row].min;
                        span = row - 9;
                        clearable = span < 5U;
                        /* The native loop computes this range test without reading its result. */
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
                g_debugchr_status_display_thread_params.redraw_request = 1;
                g_debugchr_numeric_editor_thread_params.redraw_request = 1;
                g_debugchr_character_status_frame_config.redraw_request = 1;
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
                bob_y = y - (((i * g_script_event_speed) >> 4) & 3);
                setXY4(cursor_v, x, bob_y, x + 0x10, bob_y, x, bob_y + 0x10, x + 0x10, bob_y + 0x10);
                setXY4(shadow_v, x + 2, bob_y + 2, x + 0x12, bob_y + 2, x + 2, bob_y + 0x12, x + 0x12, bob_y + 0x12);
                g_debugchr_editor_cursor_polys[buffer].clut = 0x7D7C;
            } else {
                cursor_v_edit = PRIM_AT(cursor_prims, buffer);
                shadow_v_edit = &g_debugchr_editor_cursor_shade_polys[buffer];
                setXY4(cursor_v_edit, x, y, x + 0x10, y, x, y + 0x10, x + 0x10, y + 0x10);
                setXY4(shadow_v_edit, x + 2, y + 2, x + 0x12, y + 2, x + 2, y + 0x12, x + 0x12, y + 0x12);
                g_debugchr_editor_cursor_polys[buffer].clut = 0x7E7C;
            }
        } else if (g_debugchr_editor_field_edit_active == 0) {
            cursor_h = PRIM_AT(cursor_prims, buffer);
            uv_lo = 0xA8;
            cursor_h->u0 = uv_lo;
            uv_lo = 0xB8;
            /* Removing the tied use eliminates two native instructions. */
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
            bob_x = ((i * g_script_event_speed) >> 4) & 3;
            setXY4(cursor_h, x - bob_x, y, (x - bob_x) + 0x10, y, x - bob_x, y + 0x10, (x - bob_x) + 0x10, y + 0x10);
            /* A separate temporary schedules the decrement after the
             * `(x - bob_x) + 0x10` sum; `bob_x -= 2` would move it ahead. */
            shadow_bob_x = bob_x - 2;
            setXY4(shadow_h, x - shadow_bob_x, y + 2, (x - shadow_bob_x) + 0x10, y + 2, x - shadow_bob_x, y + 0x12,
                (x - shadow_bob_x) + 0x10, y + 0x12);
            g_debugchr_editor_cursor_polys[buffer].clut = 0x7D7C;
        } else {
            cursor_h_edit = PRIM_AT(cursor_prims, buffer);
            shadow_h_edit = &g_debugchr_editor_cursor_shade_polys[buffer];
            uv_lo = 0xA8;
            cursor_h_edit->u0 = uv_lo;
            uv_lo = 0xB8;
            /* Removing the tied use eliminates two native instructions. */
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
            setXY4(cursor_h_edit, x, y, x + 0x10, y, x, y + 0x10, x + 0x10, y + 0x10);
            setXY4(shadow_h_edit, x + 2, y + 2, x + 0x12, y + 2, x + 2, y + 0x12, x + 0x12, y + 0x12);
            g_debugchr_editor_cursor_polys[buffer].clut = 0x7E7C;
        }
        gfx_draw_or_append_gpu_primitive(PRIM_AT(cursor_prims, buffer));
        gfx_draw_or_append_gpu_primitive(&g_debugchr_editor_cursor_shade_polys[buffer]);
        thread_wait_frames(1);
        i++;
    }
}
