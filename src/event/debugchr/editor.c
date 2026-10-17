/* The debug unit editor and the mounted unit's status screen: applying values to a battle unit, the unit thread and
 * the session. */

#include "fft/command.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/status_screen.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_editor.h"
#include "fft/unit_event.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static POLY_FT4 g_unit_editor_gfx_render_buffers[2][30] = { 0 };
static POLY_FT4 g_unit_editor_cursor_polys[2] = { 0 };
static POLY_FT4 g_unit_editor_cursor_shade_polys[2] = { 0 };
static help_navigation_record_t* g_unit_editor_field_table = 0;
static help_navigation_record_t* g_unit_editor_current_field = 0;
static s32 g_unit_editor_selected_unit_slot = 0;

/* Write the editor's values back to battle unit `unit_id`: HP, MP and CT
 * (each first capped at its maximum), level, experience, Brave, Faith, the
 * zodiac sign (the top nibble of the birthday 16-bit value), move, speed and
 * jump. */
static void debug_unit_editor_apply_values(s32 unit_id) {
    unit_stats_t* unit = unit_get_stats(unit_id);
    s32 zodiac;
    u32 unit_flags;

    if (g_unit_editor_hp_max < g_unit_editor_values[0])
        g_unit_editor_values[0] = g_unit_editor_hp_max;
    if (g_unit_editor_mp_max < g_unit_editor_values[3])
        g_unit_editor_values[3] = g_unit_editor_mp_max;
    if (g_unit_editor_ct_max < g_unit_editor_values[6])
        g_unit_editor_values[6] = g_unit_editor_ct_max;
    unit->hp = g_unit_editor_values[0];
    unit->mp = g_unit_editor_values[3];
    unit->ct = g_unit_editor_values[6];
    unit->level = g_unit_panel_selected_unit_status.level;
    unit->experience = g_unit_panel_selected_unit_status.experience;
    /* 16-bit objects (the loader stores sh); this function reads only the
     * low byte, and the lvalue form emits the target's bare lbu. */
    unit->brave = g_unit_panel_selected_unit.brave;
    unit->faith = g_unit_panel_selected_unit.faith;
    /* Named bitfield assignment narrows the signed 16-bit zodiac load
     * and reorders these stores. */
    zodiac = g_unit_panel_selected_unit.zodiac << 12;
    unit_flags = *(u16*)&unit->birthday & 0x0fff;
    *(u16*)&unit->birthday = unit_flags | zodiac;
    unit->move = g_unit_panel_selected_unit_fields.state.move;
    unit->attributes[UNIT_ATTRIBUTE_SPEED] = g_unit_panel_selected_unit_fields.state.speed;
    unit->jump = g_unit_panel_selected_unit_fields.state.jump;
}

/*
 * Thread task of the unit status screen opened on one battle unit: the
 * read-only use of the DEBUGCHR panels (g_debug_unit_editor_active stays 0).
 *
 * Loads `unit_id` and starts the status banner, numeric and character status
 * panel threads. L1/R1 switch between the unit and its mount partner, Circle
 * opens the status-effect list as the previous thread slot and waits for it
 * (refused for a generic monster with no status), and Cross closes the
 * panels and ends the loop, after which the first unit is loaded again. The
 * menu-building thread holds the status groups' animation bytes at 1 while
 * it runs, and the panels are dimmed only while a THREAD_TASK_RESUME
 * thread is running as well.
 *
 * `sound_latched` is read before anything assigns it when the menu-building
 * thread is already running on the first frame.
 */
void debugchr_mounted_unit_entrypoint(s32 unit_id) {
    s32 frame;
    s32 one;
    u8* animation;
    POLY_FT4* render_buffers;
    /* A record-base cursor changes the saved address and all three retail offsets. */
    s16* selected_unit;
    u8* animation_second;
    u8* animation_third;
    s32 sound_latched;
    s32 event_mode;
    unit_stats_t* unit;
    s32 input;
    /* Shared by the R1 and L1 blocks: set twice in the loop, the address is
     * not a loop invariant candidate and is rebuilt in each block. */
    s16* unit_type_ptr;
    /* The target stores the unit id and re-reads it three times through one
     * address register; only a volatile view keeps CSE from merging the
     * loads. */
    volatile u8* unit_ids;

    thread_set_current_task_id(THREAD_TASK_MOUNTED_STATUS_SCREEN);
    g_debug_unit_editor_active = 0;
    g_unit_panel_panel_status_group_count = 2;

    /* Slot 0 is this unit and slot 1 its mount partner, between which L1 and R1 toggle. */
    unit = unit_get_stats(unit_id);
    unit_ids = g_unit_editor_unit_ids;
    unit_ids[0] = unit_id;
    {
        s32 partner_id;
        s32 selected_id;
        s32 comparison_id;
        s32 type_id;

        partner_id = unit->mount_info;
        selected_id = unit_ids[0];
        comparison_id = unit_ids[0];
        type_id = unit_ids[0];
        unit_ids[1] = partner_id & UNIT_MOUNT_INFO_PARTNER_ID_MASK;
        g_unit_panel_selected_unit.battle_id = selected_id;
        g_unit_panel_selected_unit.name_id = comparison_id;
        g_unit_panel_selected_unit_status.unit_index = type_id;
    }
    unit_panel_load_selected();
    g_unit_editor_selected_unit_slot = 0;

    /* The template store is repeated in both arms: jump2 cross-jumps the
     * identical tails only after sched2, so the join keeps the store ahead of
     * the thread-setup constant loads. */
    if (g_unit_panel_selected_unit_fields.state.generic_monster != 0) {
        mem_copy_bytes_nonempty(g_unit_editor_template, g_unit_editor_template_special, 0xa8);
        g_unit_editor_fields[15].destination[0] = 0xe;
    } else {
        mem_copy_bytes_nonempty(g_unit_editor_template, g_unit_editor_template_normal, 0xa8);
        g_unit_editor_fields[15].destination[0] = 0xe;
    }

    {
        /* Whole-record locals fold these saved member addresses into absolute stores. */
        s32* refresh_a;
        s32* refresh_b;
        s32* refresh_c;

        refresh_a = &g_unit_editor_status_display_thread_params.style;
        refresh_b = &g_unit_panel_numbers_thread_params.style;
        refresh_c = &g_unit_panel_character_status_frame_config.style;
        *refresh_a = 0;
        *refresh_b = 0;
        *refresh_c = 0;
        thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, unit_panel_banner_thread);
        thread_set_parameters(
            THREAD_SCREEN_SLOT_UNIT_STATUS, (u8*)refresh_a - offsetof(unit_panel_thread_t, style), 0, 0);
        thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, unit_panel_numbers_thread);
        thread_set_parameters(
            THREAD_SCREEN_SLOT_NUMERIC_PANEL, (u8*)refresh_b - offsetof(unit_panel_thread_t, style), 0, 0);
        thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_ability_thread);
        thread_set_parameters(
            THREAD_SCREEN_SLOT_CHARACTER_STATUS, (u8*)refresh_c - offsetof(unit_panel_thread_t, style), 0, 0);
    }

    frame = 0;
    render_buffers = g_unit_editor_gfx_render_buffers[0];
    one = 1;
    animation = g_unit_panel_panel_status_animation;
    animation_second = animation + 1;
    animation_third = animation + 2;
    selected_unit = &g_unit_panel_selected_unit.battle_id;
    g_menu_modal_open = 0;

    for (;; frame++) {
        thread_yield();
        g_menu_input_controller = script_get_controller_input_pointer(0);
        {
            s32 render_offset;
            render_offset = (frame & 1) * sizeof(g_unit_editor_gfx_render_buffers[0]);
            /* Typed polygon indexing reverses the ADDU operands. */
            unit_panel_build_status_groups((POLY_FT4*)(render_offset + (s32)render_buffers));
        }

        if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
            /* `one` doubles as SOUND_SFX_CONFIRM, played once per menu-building run. */
            if (sound_latched == 0) {
                g_sound_pending_sfx_id = one;
                sound_latched = 1;
            }
            if (thread_find_running_by_task(THREAD_TASK_RESUME) != 0) {
                g_unit_editor_status_display_thread_params.style = one;
                g_unit_panel_numbers_thread_params.style = one;
                g_unit_panel_character_status_frame_config.style = one;
            } else {
                g_unit_editor_status_display_thread_params.style = 0;
                g_unit_panel_numbers_thread_params.style = 0;
                g_unit_panel_character_status_frame_config.style = 0;
            }
            animation[0] = one;
            animation[1] = one;
            animation[2] = one;
            continue;
        }

        event_mode = g_menu_input_lock_mode;
        sound_latched = 0;
        animation[0] = 0;
        animation[1] = 0;
        animation[2] = 0;

        if (event_mode == 0 && g_help_open == 0 && thread_is_previous_running() == 0) {
            input = *g_menu_input_controller;
            if ((input & PSX_PAD_CROSS) != 0) {
                thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
                thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, 0, 0, 1);
                thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
                g_sound_pending_sfx_id = SOUND_SFX_CANCEL;
                break;
            }

            if ((input & PSX_PAD_CIRCLE) != 0 && thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) == 0
                && (unit_has_any_current_status(g_unit_panel_selected_unit.battle_id) != 0
                    || g_unit_panel_selected_unit_fields.state.generic_monster == 0)) {
                g_unit_editor_status_display_thread_params.style = one;
                g_unit_panel_numbers_thread_params.style = one;
                g_unit_panel_character_status_frame_config.style = one;
                thread_start(g_thread_current_id - 1, status_screen_build_status_list);
                thread_set_parameters(
                    g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_STATUS_LIST], 0, 0);
                animation[0] = one;
                animation_second[0] = one;
                animation_third[0] = one;

                /* Keep the status groups drawn until the status list thread in the previous slot exits. */
                for (;;) {
                    thread_yield();
                    {
                        s32 render_offset;
                        render_offset = (frame & 1) * sizeof(g_unit_editor_gfx_render_buffers[0]);
                        /* Typed polygon indexing reverses the ADDU operands. */
                        unit_panel_build_status_groups((POLY_FT4*)(render_offset + (s32)render_buffers));
                    }
                    if (thread_is_running_asm(g_thread_current_id - 1) == 0) {
                        break;
                    }
                    frame++;
                }

                animation[0] = 0;
                animation[1] = 0;
                animation[2] = 0;
                g_unit_editor_status_display_thread_params.style = 0;
                g_unit_panel_numbers_thread_params.style = 0;
                g_unit_panel_character_status_frame_config.style = 0;
            }
        }

        if ((*g_menu_input_controller & PSX_PAD_R1) != 0) {
            s32 slot;
            u8* chosen_unit;
            unit_panel_unit_t* unit_data;

            slot = g_unit_editor_selected_unit_slot;
            g_sound_pending_sfx_id = SOUND_SFX_PAGE_SWITCH;
            g_unit_panel_panel_status_animation[1] = 7;
            slot ^= 1;
            g_unit_editor_selected_unit_slot = slot;

            chosen_unit = &g_unit_editor_unit_ids[slot];
            *selected_unit = *chosen_unit;
            unit_data = (unit_panel_unit_t*)((u8*)selected_unit - offsetof(unit_panel_unit_t, battle_id));
            unit_data->name_id = *chosen_unit;
            unit_type_ptr = &g_unit_panel_selected_unit_status.unit_index;
            *unit_type_ptr = *chosen_unit;
            unit_panel_load_selected();
            unit_panel_copy_billboard_in(
                &g_unit_panel_selected_unit_status, unit_data, &g_unit_panel_selected_unit_fields);

            /* The status display takes its redraw request as the portrait wipe direction; L1 passes 2. */
            g_unit_editor_status_display_thread_params.redraw_request = one;
            g_unit_panel_numbers_thread_params.redraw_request = one;
            g_unit_panel_character_status_frame_config.redraw_request = one;
        }

        if ((*g_menu_input_controller & PSX_PAD_L1) != 0) {
            s32 slot;
            u8* chosen_unit;
            unit_panel_unit_t* unit_data;

            slot = g_unit_editor_selected_unit_slot;
            g_sound_pending_sfx_id = SOUND_SFX_PAGE_SWITCH;
            g_unit_panel_panel_status_animation[0] = 7;
            slot ^= 1;
            g_unit_editor_selected_unit_slot = slot;

            chosen_unit = &g_unit_editor_unit_ids[slot];
            *selected_unit = *chosen_unit;
            unit_data = (unit_panel_unit_t*)((u8*)selected_unit - offsetof(unit_panel_unit_t, battle_id));
            unit_data->name_id = *chosen_unit;
            unit_type_ptr = &g_unit_panel_selected_unit_status.unit_index;
            *unit_type_ptr = *chosen_unit;
            unit_panel_load_selected();
            unit_panel_copy_billboard_in(
                &g_unit_panel_selected_unit_status, unit_data, &g_unit_panel_selected_unit_fields);

            /* 2 runs the portrait wipe in the opposite direction from R1's. */
            g_unit_editor_status_display_thread_params.redraw_request = 2;
            g_unit_panel_numbers_thread_params.redraw_request = one;
            g_unit_panel_character_status_frame_config.redraw_request = one;
        }
    }

    /* Leave the panels on the unit the thread was started with. */
    {
        s16* selected_unit_ptr;
        s16* comparison_ptr;
        s16* unit_type_id_ptr;

        selected_unit_ptr = &g_unit_panel_selected_unit.battle_id;
        unit_type_id_ptr = &g_unit_panel_selected_unit_status.unit_index;
        selected_unit_ptr[0] = g_unit_editor_unit_ids[0];
        /* A register address keeps cse from folding this store to a direct
         * global address. */
        comparison_ptr = selected_unit_ptr
            - (offsetof(unit_panel_unit_t, battle_id) - offsetof(unit_panel_unit_t, name_id))
                / sizeof(*selected_unit_ptr);
        *comparison_ptr = g_unit_editor_unit_ids[0];
        *unit_type_id_ptr = g_unit_editor_unit_ids[0];
        unit_panel_load_selected();
        unit_panel_copy_billboard_in(
            &g_unit_panel_selected_unit_status, &g_unit_panel_selected_unit, &g_unit_panel_selected_unit_fields);
    }
}

/* Ordinary indexing changes the ADDU operand order at the primitive bases. */
#define PRIM_AT(base, index) ((POLY_FT4*)((s32)((index) * sizeof(POLY_FT4)) - -(s32)(base)))

/* Run the debug unit editor on the battle units present on the field.
 *
 * It opens the status, numeric and character panels and alternates between two
 * modes. Unit selection: R1 and L1 page through the selectable units, CIRCLE enters the
 * field editor and CROSS closes the panels and returns. Field editor: the D-pad follows
 * each field's links to its neighbors, CIRCLE starts or commits an edit and CROSS commits
 * one or goes back to unit selection. While editing, RIGHT/UP and LEFT/DOWN step the value
 * (by 10 while L1 is held) within the field template's range, TRIANGLE zeroes rows 9 to
 * 13, and every change is written back to the battle unit. Each level step (row 0) also
 * applies one level's stat growth or its reversal. */
void debugchr_editor_entrypoint(void) {
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
    s32 cursor_x;

    g_debug_unit_editor_active = 1;
    g_unit_panel_panel_status_group_count = 2;

    /* A slot is selectable when the lookup of its ENTD unit id returns that same slot: a present unit,
     * the first one with that id. */
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit_entd_find_stats(unit_get_stats(i)->unit_id, &battle_id);
        if ((battle_id >= 0) && (i == battle_id)) {
            g_unit_editor_unit_ids[i] = 1;
        } else {
            g_unit_editor_unit_ids[i] = 0;
        }
    }

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        if (g_unit_editor_unit_ids[i] != 0) {
            g_unit_panel_selected_unit.battle_id = i;
            g_unit_panel_selected_unit.name_id = i;
            g_unit_panel_selected_unit_status.unit_index = i;
            unit_panel_load_selected();
            g_unit_editor_selected_unit_slot = i;
            break;
        }
    }

    /* Retail reloads the same unit and resets the slot to 0, even when the first selectable unit is a
     * later slot. */
    unit_panel_load_selected();
    g_unit_editor_selected_unit_slot = 0;
    if (g_unit_panel_selected_unit_fields.state.generic_monster != 0) {
        mem_copy_bytes_nonempty(g_unit_editor_template, g_unit_editor_template_special, 0xA8);
        g_unit_editor_fields[15].destination[0] = 0xE;
    } else {
        mem_copy_bytes_nonempty(g_unit_editor_template, g_unit_editor_template_normal, 0xA8);
        g_unit_editor_fields[15].destination[0] = 0xE;
    }

    g_unit_editor_status_display_thread_params.style = 0;
    g_unit_panel_numbers_thread_params.style = 0;
    g_unit_panel_character_status_frame_config.style = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, unit_panel_banner_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&g_unit_editor_status_display_thread_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, unit_panel_numbers_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, (s32)&g_unit_panel_numbers_thread_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_ability_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_unit_panel_character_status_frame_config, 0, 0);

    row = 0;
    /* The unit menu and the field editor re-enter each other by label: an
       enclosing loop's notes reweight the allocation of the whole body. */
loop_outer:
    i = 0;
    /* name_id is five 16-bit values before battle_id; record access changes the store displacements. */
    selected_battle_id = &g_unit_panel_selected_unit.battle_id;
    for (;;) {
        mark = 0xE;
        /* Removing the tied use changes the marker and menu-pointer allocation. */
        __asm__("" : "=r"(mark) : "0"(mark));
        thread_yield();
        unit_panel_build_status_groups(g_unit_editor_gfx_render_buffers[i & 1]);

        input = script_get_controller_input_pointer(0);
        g_menu_input_controller = input;
        buttons = *input;
        if (buttons & PSX_PAD_CIRCLE) {
            goto editor;
        }
        if (buttons & PSX_PAD_CROSS) {
            thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
            return;
        }

        if (buttons & PSX_PAD_R1) {
            g_unit_panel_panel_status_animation[1] = 7;
            /* Label loops: a do-while's loop notes change the allocation. */
        next_unit:
            g_unit_editor_selected_unit_slot += 1;
            if (g_unit_editor_selected_unit_slot == 0x15) {
                g_unit_editor_selected_unit_slot = 0;
            }
            if (g_unit_editor_unit_ids[g_unit_editor_selected_unit_slot] == 0) {
                goto next_unit;
            }

            unit_index = g_unit_editor_selected_unit_slot;
            selected_battle_id[0] = unit_index;
            selected_battle_id[-5] = unit_index;
            g_unit_panel_selected_unit_status.unit_index = unit_index;
            unit_panel_load_selected();
            if (g_unit_panel_selected_unit_fields.state.generic_monster != 0) {
                mem_copy_bytes_nonempty(g_unit_editor_template, g_unit_editor_template_special, 0xA8);
            } else {
                mem_copy_bytes_nonempty(g_unit_editor_template, g_unit_editor_template_normal, 0xA8);
            }
            g_unit_editor_fields[15].destination[0] = mark;

            /* The status display takes its redraw request as the portrait wipe direction; L1 passes 2. */
            g_unit_editor_status_display_thread_params.redraw_request = 1;
            g_unit_panel_numbers_thread_params.redraw_request = 1;
            g_unit_panel_character_status_frame_config.redraw_request = 1;
        }

        if (*g_menu_input_controller & PSX_PAD_L1) {
            g_unit_panel_panel_status_animation[0] = 7;
        prev_unit:
            g_unit_editor_selected_unit_slot -= 1;
            if (g_unit_editor_selected_unit_slot == -1) {
                g_unit_editor_selected_unit_slot = 0x14;
            }
            if (g_unit_editor_unit_ids[g_unit_editor_selected_unit_slot] == 0) {
                goto prev_unit;
            }

            unit_index = g_unit_editor_selected_unit_slot;
            selected_battle_id[0] = unit_index;
            selected_battle_id[-5] = unit_index;
            g_unit_panel_selected_unit_status.unit_index = unit_index;
            unit_panel_load_selected();
            if (g_unit_panel_selected_unit_fields.state.generic_monster != 0) {
                mem_copy_bytes_nonempty(g_unit_editor_template, g_unit_editor_template_special, 0xA8);
            } else {
                mem_copy_bytes_nonempty(g_unit_editor_template, g_unit_editor_template_normal, 0xA8);
            }
            g_unit_editor_fields[15].destination[0] = mark;
            /* Keeps the `li v0,2` for the redraw store below the byte store. */
            __asm__("");
            /* 2 runs the portrait wipe in the opposite direction from R1's. */
            g_unit_editor_status_display_thread_params.redraw_request = 2;
            g_unit_panel_numbers_thread_params.redraw_request = 1;
            g_unit_panel_character_status_frame_config.redraw_request = 1;
        }

        i++;
    }

editor:
    for (i = 0; i < 2; i++) {
        menu_init_default_poly_ft4(&g_unit_editor_cursor_polys[i]);
        SetSemiTrans(&g_unit_editor_cursor_polys[i], 0);
        g_unit_editor_cursor_polys[i].tpage = GetTPage(0, 0, 0x3C0, 0x100);
        g_unit_editor_cursor_polys[i].clut = 0x7D7C;
        menu_init_default_poly_ft4(&g_unit_editor_cursor_shade_polys[i]);
        SetSemiTrans(&g_unit_editor_cursor_shade_polys[i], 1);
        g_unit_editor_cursor_shade_polys[i].tpage = GetTPage(0, 2, 0x3C0, 0x100);
        g_unit_editor_cursor_shade_polys[i].clut = 0x7DBC;
    }

    g_unit_editor_field_table = g_unit_editor_fields;
    cursor_prims = g_unit_editor_cursor_polys;
    i = 0;
    thread_yield();
    for (;;) {
        field = &g_unit_editor_field_table[row];
        x = field->x;
        y = field->y + 8;
        g_unit_editor_current_field = field;

        /* Outside edit mode Cross returns to unit selection; while editing, Cross and Circle both commit the
         * value. */
        keys = *g_menu_input_controller;
        if (keys & PSX_PAD_CROSS) {
            if (g_unit_editor_field_edit_active != 1) {
                goto loop_outer;
            }
            g_unit_editor_field_edit_active = 0;
            debug_unit_editor_apply_values(g_unit_editor_selected_unit_slot);
            unit_panel_load_selected();
        } else if (keys & PSX_PAD_CIRCLE) {
            if (g_unit_editor_field_edit_active == 1) {
                g_unit_editor_field_edit_active = 0;
                debug_unit_editor_apply_values(g_unit_editor_selected_unit_slot);
                unit_panel_load_selected();
            } else {
                g_unit_editor_field_edit_active = 1;
            }
        }

        /* Triangle clears the value of rows 9 to 13 while editing. */
        if ((g_unit_editor_field_edit_active != 0) && ((row - 9) < 5U)
            && (*g_menu_input_controller & PSX_PAD_TRIANGLE)) {
            *g_unit_editor_current_field->value = 0;
            debug_unit_editor_apply_values(g_unit_editor_selected_unit_slot);
            unit_panel_load_selected();
            g_unit_editor_status_display_thread_params.redraw_request = 1;
            g_unit_panel_numbers_thread_params.redraw_request = 1;
            g_unit_panel_character_status_frame_config.redraw_request = 1;
        }

        /* Outside edit mode the D-pad follows the field's up, down, right and left links; while editing it
         * steps the value. */
        navkeys = *g_menu_input_controller;
        if (navkeys & PSX_PAD_DPAD_MASK) {
            if (g_unit_editor_field_edit_active == 0) {
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
                row = g_unit_editor_current_field->destination[direction];
            } else {
                if ((navkeys & PSX_PAD_RIGHT) || (navkeys & PSX_PAD_UP)) {
                    /* A value outside its range first snaps to one below the maximum (even from below the minimum),
                     * so stepping up lands on the maximum. */
                    value = g_unit_editor_current_field->value;
                    cur = *value;
                    /* Shared pointers or a combined range test change address materialization. */
                    up_lo = &g_unit_editor_template[row].min;
                    if (cur < *up_lo) {
                        goto clamp_hi;
                    }
                    up_hi = &g_unit_editor_template[row].max;
                    if (*up_hi < cur) {
                    clamp_hi:
                        up_clamp = &g_unit_editor_template[row].max;
                        *value = *up_clamp - 1;
                    }

                    /* PadRead gives the held buttons: holding L1 steps the value by 10. */
                    if (PadRead(1) & PSX_PAD_L1) {
                        steps = 0xA;
                    } else {
                        steps = 1;
                    }

                    /* `i` is also the frame counter, so each step resets the cursor's buffer index and bob phase. */
                    for (i = 0; i < steps; i++) {
                        hi_limit = &g_unit_editor_template[row].max;
                        span = row - 9;
                        clearable = span < 5U;
                        /* The native loop computes this range test without reading its result. */
                        __asm__("" : : "r"(clearable));
                        value = g_unit_editor_current_field->value;
                        cur_u = *value;
                        if (cur_u < *hi_limit) {
                            *value = cur_u + 1;
                            /* Row 0 is the level: growth depends on it, so it is applied with the old
                             * level written back before the new value is restored. */
                            if (row == 0) {
                                *g_unit_editor_current_field->value -= 1;
                                debug_unit_editor_apply_values(g_unit_editor_selected_unit_slot);
                                unit_apply_level_growth(unit_get_stats(g_unit_editor_selected_unit_slot), 0);
                                *g_unit_editor_current_field->value += 1;
                            }
                            debug_unit_editor_apply_values(g_unit_editor_selected_unit_slot);
                            unit_panel_load_selected();
                        }
                    }
                } else {
                    /* Likewise a value outside its range snaps to one above the minimum. */
                    down_value = g_unit_editor_current_field->value;
                    cur = *down_value;
                    dn_lo = &g_unit_editor_template[row].min;
                    if (cur < *dn_lo) {
                        goto clamp_lo;
                    }
                    dn_hi = &g_unit_editor_template[row].max;
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
                        lo_limit = &g_unit_editor_template[row].min;
                        span = row - 9;
                        clearable = span < 5U;
                        /* The native loop computes this range test without reading its result. */
                        __asm__("" : : "r"(clearable));
                        value = g_unit_editor_current_field->value;
                        cur_d = *value;
                        if (*lo_limit < cur_d) {
                            *value = cur_d - 1;
                            if (row == 0) {
                                *g_unit_editor_current_field->value += 1;
                                debug_unit_editor_apply_values(g_unit_editor_selected_unit_slot);
                                unit_apply_level_growth(unit_get_stats(g_unit_editor_selected_unit_slot), 1);
                                *g_unit_editor_current_field->value -= 1;
                            }
                            debug_unit_editor_apply_values(g_unit_editor_selected_unit_slot);
                            unit_panel_load_selected();
                        }
                    }
                }

                g_unit_editor_status_display_thread_params.redraw_request = 1;
                g_unit_panel_numbers_thread_params.redraw_request = 1;
                g_unit_panel_character_status_frame_config.redraw_request = 1;
            }
        }

        /* The cursor bobs with the frame count unless a value is being edited, which also switches it to
         * the 0x7E7C palette. */
        buffer = i & 1;
        if (g_unit_editor_current_field->vertical_cursor != 0) {
            /* The upright cursor is the same texture cell transposed. */
            uv_lo = 0xA8;
            g_unit_editor_cursor_polys[buffer].u0 = uv_lo;
            g_unit_editor_cursor_polys[buffer].u1 = uv_lo;
            uv_v_hi = 0xB8;
            g_unit_editor_cursor_polys[buffer].u2 = uv_v_hi;
            g_unit_editor_cursor_polys[buffer].u3 = uv_v_hi;
            g_unit_editor_cursor_polys[buffer].v0 = 0;
            g_unit_editor_cursor_polys[buffer].v1 = 0x10;
            g_unit_editor_cursor_polys[buffer].v2 = 0;
            g_unit_editor_cursor_polys[buffer].v3 = 0x10;

            uv_lo = 0xB8;
            g_unit_editor_cursor_shade_polys[buffer].u0 = uv_lo;
            g_unit_editor_cursor_shade_polys[buffer].u1 = uv_lo;
            uv_v_far = 0xC8;
            g_unit_editor_cursor_shade_polys[buffer].u2 = uv_v_far;
            g_unit_editor_cursor_shade_polys[buffer].u3 = uv_v_far;
            g_unit_editor_cursor_shade_polys[buffer].v0 = 0;
            g_unit_editor_cursor_shade_polys[buffer].v1 = 0x10;
            g_unit_editor_cursor_shade_polys[buffer].v2 = 0;
            g_unit_editor_cursor_shade_polys[buffer].v3 = 0x10;

            if (g_unit_editor_field_edit_active == 0) {
                cursor_v = PRIM_AT(cursor_prims, buffer);
                shadow_v = &g_unit_editor_cursor_shade_polys[buffer];
                bob_y = y - (((i * g_script_event_speed) >> 4) & 3);
                setXY4(cursor_v, x, bob_y, x + 0x10, bob_y, x, bob_y + 0x10, x + 0x10, bob_y + 0x10);
                setXY4(shadow_v, x + 2, bob_y + 2, x + 0x12, bob_y + 2, x + 2, bob_y + 0x12, x + 0x12, bob_y + 0x12);
                g_unit_editor_cursor_polys[buffer].clut = 0x7D7C;
            } else {
                cursor_v_edit = PRIM_AT(cursor_prims, buffer);
                shadow_v_edit = &g_unit_editor_cursor_shade_polys[buffer];
                setXY4(cursor_v_edit, x, y, x + 0x10, y, x, y + 0x10, x + 0x10, y + 0x10);
                setXY4(shadow_v_edit, x + 2, y + 2, x + 0x12, y + 2, x + 2, y + 0x12, x + 0x12, y + 0x12);
                g_unit_editor_cursor_polys[buffer].clut = 0x7E7C;
            }
        } else if (g_unit_editor_field_edit_active == 0) {
            cursor_h = PRIM_AT(cursor_prims, buffer);
            uv_lo = 0xA8;
            cursor_h->u0 = uv_lo;
            uv_lo = 0xB8;
            /* Removing the tied use eliminates two native instructions. */
            __asm__("" : "=r"(uv_lo) : "0"(uv_lo));
            cursor_h->v0 = 0;
            uv_h_hi = 0xB8;
            cursor_h->u1 = uv_h_hi;
            shadow_h = &g_unit_editor_cursor_shade_polys[buffer];
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
            cursor_x = x - bob_x;
            setXY4(cursor_h, cursor_x, y, cursor_x + 0x10, y, cursor_x, y + 0x10, cursor_x + 0x10, y + 0x10);
            /* A separate temporary schedules the decrement after the
             * cursor's `cursor_x + 0x10` sums; `bob_x -= 2` would move it ahead. */
            shadow_bob_x = bob_x - 2;
            setXY4(shadow_h, x - shadow_bob_x, y + 2, (x - shadow_bob_x) + 0x10, y + 2, x - shadow_bob_x, y + 0x12,
                (x - shadow_bob_x) + 0x10, y + 0x12);
            g_unit_editor_cursor_polys[buffer].clut = 0x7D7C;
        } else {
            cursor_h_edit = PRIM_AT(cursor_prims, buffer);
            shadow_h_edit = &g_unit_editor_cursor_shade_polys[buffer];
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
            g_unit_editor_cursor_polys[buffer].clut = 0x7E7C;
        }

        gfx_draw_or_append_gpu_primitive(PRIM_AT(cursor_prims, buffer));
        gfx_draw_or_append_gpu_primitive(&g_unit_editor_cursor_shade_polys[buffer]);
        thread_wait_frames(1);
        i++;
    }
}
