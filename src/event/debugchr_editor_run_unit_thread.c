#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/event_debugchr.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Thread task of the unit status screen opened on one battle unit: the
 * read-only use of the DEBUGCHR panels (g_debugchr_editor_active stays 0).
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
void debugchr_editor_run_unit_thread(s32 unit_id) {
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

    thread_set_current_task_id(THREAD_TASK_DEBUG_UNIT_EDITOR);
    g_debugchr_editor_active = 0;
    g_menu_panel_status_group_count = 2;
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    unit_ids = g_debugchr_editor_unit_ids;
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
        unit_ids[1] = partner_id & BATTLE_MOUNT_INFO_PARTNER_ID_MASK;
        g_debugchr_panel_selected_unit_data.battle_id = selected_id;
        g_debugchr_panel_selected_unit_data.name_id = comparison_id;
        g_debugchr_panel_selected_billboard.unit_index = type_id;
    }
    debugchr_editor_load_selected_unit();
    g_debugchr_editor_selected_unit_slot = 0;

    /* The template store is repeated in both arms: jump2 cross-jumps the
     * identical tails only after sched2, so the join keeps the store ahead of
     * the thread-setup constant loads. */
    if (g_menu_status_panel_fields.state.generic_monster != 0) {
        mem_copy_bytes_nonempty(g_debugchr_editor_template, g_debugchr_editor_template_special, 0xa8);
        g_debugchr_editor_fields[15].destination[0] = 0xe;
    } else {
        mem_copy_bytes_nonempty(g_debugchr_editor_template, g_debugchr_editor_template_normal, 0xa8);
        g_debugchr_editor_fields[15].destination[0] = 0xe;
    }

    {
        /* Whole-record locals fold these saved member addresses into absolute stores. */
        s32* refresh_a;
        s32* refresh_b;
        s32* refresh_c;

        refresh_a = &g_debugchr_status_display_thread_params.style;
        refresh_b = &g_debugchr_numeric_editor_thread_params.style;
        refresh_c = &g_debugchr_character_status_frame_config.style;
        *refresh_a = 0;
        *refresh_b = 0;
        *refresh_c = 0;
        thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, debugchr_panel_run_unit_status_thread);
        thread_set_parameters(
            THREAD_SCREEN_SLOT_UNIT_STATUS, (u8*)refresh_a - offsetof(menu_status_thread_t, style), 0, 0);
        thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, debugchr_editor_run_numeric_thread);
        thread_set_parameters(
            THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (u8*)refresh_b - offsetof(menu_status_thread_t, style), 0, 0);
        thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_status_panel_thread);
        thread_set_parameters(
            THREAD_SCREEN_SLOT_CHARACTER_STATUS, (u8*)refresh_c - offsetof(menu_status_thread_t, style), 0, 0);
    }

    frame = 0;
    render_buffers = g_debugchr_gfx_render_buffers[0];
    one = 1;
    animation = g_menu_panel_status_animation;
    animation_second = animation + 1;
    animation_third = animation + 2;
    selected_unit = &g_debugchr_panel_selected_unit_data.battle_id;
    g_menu_modal_open = 0;

    for (;; frame++) {
        thread_yield();
        g_menu_input_controller = script_get_controller_input_pointer(0);
        {
            s32 render_offset;
            render_offset = (frame & 1) * sizeof(g_debugchr_gfx_render_buffers[0]);
            /* Typed polygon indexing reverses the ADDU operands. */
            menu_build_status_group_primitives((POLY_FT4*)(render_offset + (s32)render_buffers));
        }

        if (battle_thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
            if (sound_latched == 0) {
                g_sound_pending_sfx_id = one;
                sound_latched = 1;
            }
            if (battle_thread_find_running_by_task(THREAD_TASK_RESUME) != 0) {
                g_debugchr_status_display_thread_params.style = one;
                g_debugchr_numeric_editor_thread_params.style = one;
                g_debugchr_character_status_frame_config.style = one;
            } else {
                g_debugchr_status_display_thread_params.style = 0;
                g_debugchr_numeric_editor_thread_params.style = 0;
                g_debugchr_character_status_frame_config.style = 0;
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

        if (event_mode == 0 && g_battle_menu_help_open == 0 && battle_thread_is_previous_running() == 0) {
            input = *g_menu_input_controller;
            if ((input & PSX_PAD_CROSS) != 0) {
                thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
                thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, 0, 0, 1);
                thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
                g_sound_pending_sfx_id = MAIN_SFX_CANCEL;
                break;
            }
            if ((input & PSX_PAD_CIRCLE) != 0 && battle_thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) == 0
                && (unit_has_any_current_status(g_debugchr_panel_selected_unit_data.battle_id) != 0
                    || g_menu_status_panel_fields.state.generic_monster == 0)) {
                g_debugchr_status_display_thread_params.style = one;
                g_debugchr_numeric_editor_thread_params.style = one;
                g_debugchr_character_status_frame_config.style = one;
                thread_start(g_thread_current_id - 1, battle_menu_build_unit_status_list);
                thread_set_parameters(
                    g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_STATUS_LIST], 0, 0);
                animation[0] = one;
                animation_second[0] = one;
                animation_third[0] = one;
                for (;;) {
                    thread_yield();
                    {
                        s32 render_offset;
                        render_offset = (frame & 1) * sizeof(g_debugchr_gfx_render_buffers[0]);
                        /* Typed polygon indexing reverses the ADDU operands. */
                        menu_build_status_group_primitives((POLY_FT4*)(render_offset + (s32)render_buffers));
                    }
                    if (battle_thread_is_running_asm(g_thread_current_id - 1) == 0) {
                        break;
                    }
                    frame++;
                }
                animation[0] = 0;
                animation[1] = 0;
                animation[2] = 0;
                g_debugchr_status_display_thread_params.style = 0;
                g_debugchr_numeric_editor_thread_params.style = 0;
                g_debugchr_character_status_frame_config.style = 0;
            }
        }

        if ((*g_menu_input_controller & PSX_PAD_R1) != 0) {
            s32 slot;
            u8* chosen_unit;

            slot = g_debugchr_editor_selected_unit_slot;
            g_sound_pending_sfx_id = MAIN_SFX_PAGE_SWITCH;
            g_menu_panel_status_animation[1] = 7;
            slot ^= 1;
            g_debugchr_editor_selected_unit_slot = slot;
            chosen_unit = &g_debugchr_editor_unit_ids[slot];
            *selected_unit = *chosen_unit;
            ((menu_active_unit_t*)((u8*)selected_unit - offsetof(menu_active_unit_t, battle_id)))->name_id
                = *chosen_unit;
            unit_type_ptr = &g_debugchr_panel_selected_billboard.unit_index;
            *unit_type_ptr = *chosen_unit;
            debugchr_editor_load_selected_unit();
            battle_menu_copy_billboard_data_in(&g_debugchr_panel_selected_billboard,
                ((menu_active_unit_t*)((u8*)selected_unit - offsetof(menu_active_unit_t, battle_id))),
                &g_menu_status_panel_fields);
            g_debugchr_status_display_thread_params.redraw_request = one;
            g_debugchr_numeric_editor_thread_params.redraw_request = one;
            g_debugchr_character_status_frame_config.redraw_request = one;
        }
        if ((*g_menu_input_controller & PSX_PAD_L1) != 0) {
            s32 slot;
            u8* chosen_unit;

            slot = g_debugchr_editor_selected_unit_slot;
            g_sound_pending_sfx_id = MAIN_SFX_PAGE_SWITCH;
            g_menu_panel_status_animation[0] = 7;
            slot ^= 1;
            g_debugchr_editor_selected_unit_slot = slot;
            chosen_unit = &g_debugchr_editor_unit_ids[slot];
            *selected_unit = *chosen_unit;
            ((menu_active_unit_t*)((u8*)selected_unit - offsetof(menu_active_unit_t, battle_id)))->name_id
                = *chosen_unit;
            unit_type_ptr = &g_debugchr_panel_selected_billboard.unit_index;
            *unit_type_ptr = *chosen_unit;
            debugchr_editor_load_selected_unit();
            battle_menu_copy_billboard_data_in(&g_debugchr_panel_selected_billboard,
                ((menu_active_unit_t*)((u8*)selected_unit - offsetof(menu_active_unit_t, battle_id))),
                &g_menu_status_panel_fields);
            g_debugchr_status_display_thread_params.redraw_request = 2;
            g_debugchr_numeric_editor_thread_params.redraw_request = one;
            g_debugchr_character_status_frame_config.redraw_request = one;
        }
    }

    {
        s16* selected_unit_ptr;
        s16* comparison_ptr;
        s16* unit_type_id_ptr;

        selected_unit_ptr = &g_debugchr_panel_selected_unit_data.battle_id;
        unit_type_id_ptr = &g_debugchr_panel_selected_billboard.unit_index;
        selected_unit_ptr[0] = g_debugchr_editor_unit_ids[0];
        /* A register address keeps cse from folding this store to a direct
         * global address. */
        comparison_ptr = selected_unit_ptr
            - (offsetof(menu_active_unit_t, battle_id) - offsetof(menu_active_unit_t, name_id))
                / sizeof(*selected_unit_ptr);
        *comparison_ptr = g_debugchr_editor_unit_ids[0];
        *unit_type_id_ptr = g_debugchr_editor_unit_ids[0];
        debugchr_editor_load_selected_unit();
        battle_menu_copy_billboard_data_in(
            &g_debugchr_panel_selected_billboard, &g_debugchr_panel_selected_unit_data, &g_menu_status_panel_fields);
    }
}
