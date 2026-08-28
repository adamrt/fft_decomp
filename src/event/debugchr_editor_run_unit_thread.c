#include "fft/battle.h"
#include "fft/debugchr.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

void debugchr_editor_run_unit_thread(s32 unit_id) {
    s32 frame;
    s32 one;
    u8* animation;
    u8* render_buffers;
    s16* selected_unit;
    u8* animation_second;
    u8* animation_third;
    s32 sound_latched;
    s32 event_mode;
    battle_stats_t* unit;
    s32 input;
    /* Shared by the R1 and L1 blocks: set twice in the loop, the address is
     * not a loop invariant candidate and is rebuilt in each block. */
    s16* unit_type_ptr;
    /* The target stores the unit id and re-reads it three times through one
     * address register; only a volatile view keeps CSE from merging the
     * loads. */
    volatile u8* unit_ids;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_DEBUG_UNIT_EDITOR);
    g_debugchr_editor_active = 0;
    g_debugchr_panel_status_group_count = 2;
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
        g_debugchr_editor_selected_unit_id = selected_id;
        g_debugchr_panel_selected_unit_data[0] = comparison_id;
        g_debugchr_panel_selected_billboard.unit = type_id;
    }
    debugchr_editor_load_selected_unit();
    g_debugchr_editor_selected_unit_slot = 0;

    /* The template store is repeated in both arms: jump2 cross-jumps the
     * identical tails only after sched2, so the join keeps the store ahead of
     * the thread-setup constant loads. */
    if (g_debugchr_editor_display_mode != 0) {
        battle_copy_bytes(g_debugchr_editor_template, g_debugchr_editor_template_special, 0xa8);
        g_debugchr_editor_fields[15].destination[0] = 0xe;
    } else {
        battle_copy_bytes(g_debugchr_editor_template, g_debugchr_editor_template_normal, 0xa8);
        g_debugchr_editor_fields[15].destination[0] = 0xe;
    }

    {
        s32* refresh_a;
        s32* refresh_b;
        s32* refresh_c;

        refresh_a = &g_debugchr_thread_refresh_a;
        refresh_b = &g_debugchr_thread_refresh_b;
        refresh_c = &g_debugchr_thread_refresh_c;
        *refresh_a = 0;
        *refresh_b = 0;
        *refresh_c = 0;
        battle_thread_start(8, debugchr_render_unit_status_panel_thread);
        battle_thread_set_parameters(8, (u8*)refresh_a - 0x10, 0, 0);
        battle_thread_start(0xc, debugchr_editor_run_numeric_thread);
        battle_thread_set_parameters(0xc, (u8*)refresh_b - 0x10, 0, 0);
        battle_thread_start(9, debugchr_panel_run_character_status_thread);
        battle_thread_set_parameters(9, (u8*)refresh_c - 0x10, 0, 0);
    }

    frame = 0;
    render_buffers = g_debugchr_gfx_render_buffers[0];
    one = 1;
    animation = g_debugchr_panel_status_animation;
    animation_second = animation + 1;
    animation_third = animation + 2;
    selected_unit = &g_debugchr_editor_selected_unit_id;
    g_option_menu_open = 0;

    for (;; frame++) {
        battle_thread_yield();
        g_debugchr_input_controller = battle_script_get_controller_input_pointer(0);
        {
            s32 render_offset;
            render_offset = (frame & 1) * 0x4b0;
            debugchr_gfx_build_status_group_primitives((void*)(render_offset + (s32)render_buffers));
        }

        if (battle_thread_is_running_8014cc94(3) != 0) {
            if (sound_latched == 0) {
                g_sound_effect_id_to_play = one;
                sound_latched = 1;
            }
            if (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_RESUME) != 0) {
                g_debugchr_thread_refresh_a = one;
                g_debugchr_thread_refresh_b = one;
                g_debugchr_thread_refresh_c = one;
            } else {
                g_debugchr_thread_refresh_a = 0;
                g_debugchr_thread_refresh_b = 0;
                g_debugchr_thread_refresh_c = 0;
            }
            animation[0] = one;
            animation[1] = one;
            animation[2] = one;
            continue;
        }

        event_mode = g_event_mode;
        sound_latched = 0;
        animation[0] = 0;
        animation[1] = 0;
        animation[2] = 0;

        if (event_mode == 0 && g_battle_menu_help_open == 0 && battle_thread_is_previous_running() == 0) {
            input = *g_debugchr_input_controller;
            if ((input & PSX_PAD_CROSS) != 0) {
                battle_thread_set_parameters(8, 0, 0, 1);
                battle_thread_set_parameters(0xc, 0, 0, 1);
                battle_thread_set_parameters(9, 0, 0, 1);
                g_sound_effect_id_to_play = MAIN_SFX_CANCEL;
                break;
            }
            if ((input & PSX_PAD_CIRCLE) != 0 && battle_thread_is_running_8014cc94(3) == 0
                && (battle_menu_has_status_effect_for_status_window(g_debugchr_editor_selected_unit_id) != 0
                    || g_debugchr_editor_display_mode == 0)) {
                g_debugchr_thread_refresh_a = one;
                g_debugchr_thread_refresh_b = one;
                g_debugchr_thread_refresh_c = one;
                battle_thread_start(g_battle_current_thread_id - 1, battle_menu_build_unit_status_list);
                battle_thread_set_parameters(
                    g_battle_current_thread_id - 1, (s32)&g_battle_menu_thread_menu_data[8], 0, 0);
                animation[0] = one;
                animation_second[0] = one;
                animation_third[0] = one;
                for (;;) {
                    battle_thread_yield();
                    {
                        s32 render_offset;
                        render_offset = (frame & 1) * 0x4b0;
                        debugchr_gfx_build_status_group_primitives((void*)(render_offset + (s32)render_buffers));
                    }
                    if (battle_thread_is_running_8014cc94(g_battle_current_thread_id - 1) == 0) {
                        break;
                    }
                    frame++;
                }
                animation[0] = 0;
                animation[1] = 0;
                animation[2] = 0;
                g_debugchr_thread_refresh_a = 0;
                g_debugchr_thread_refresh_b = 0;
                g_debugchr_thread_refresh_c = 0;
            }
        }

        if ((*g_debugchr_input_controller & PSX_PAD_R1) != 0) {
            s32 slot;
            u8* chosen_unit;

            slot = g_debugchr_editor_selected_unit_slot;
            g_sound_effect_id_to_play = MAIN_SFX_PAGE_SWITCH;
            g_debugchr_panel_status_animation[1] = 7;
            slot ^= 1;
            g_debugchr_editor_selected_unit_slot = slot;
            chosen_unit = &g_debugchr_editor_unit_ids[slot];
            *selected_unit = *chosen_unit;
            selected_unit[-5] = *chosen_unit;
            unit_type_ptr = &g_debugchr_panel_selected_billboard.unit;
            *unit_type_ptr = *chosen_unit;
            debugchr_editor_load_selected_unit();
            battle_action_copy_active_turn_data_from(
                unit_type_ptr - 5, selected_unit - 5, g_debugchr_editor_unit_fields);
            g_debugchr_panel_refresh_a = one;
            g_debugchr_panel_refresh_b = one;
            g_debugchr_panel_refresh_c = one;
        }
        if ((*g_debugchr_input_controller & PSX_PAD_L1) != 0) {
            s32 slot;
            u8* chosen_unit;

            slot = g_debugchr_editor_selected_unit_slot;
            g_sound_effect_id_to_play = MAIN_SFX_PAGE_SWITCH;
            g_debugchr_panel_status_animation[0] = 7;
            slot ^= 1;
            g_debugchr_editor_selected_unit_slot = slot;
            chosen_unit = &g_debugchr_editor_unit_ids[slot];
            *selected_unit = *chosen_unit;
            selected_unit[-5] = *chosen_unit;
            unit_type_ptr = &g_debugchr_panel_selected_billboard.unit;
            *unit_type_ptr = *chosen_unit;
            debugchr_editor_load_selected_unit();
            battle_action_copy_active_turn_data_from(
                unit_type_ptr - 5, selected_unit - 5, g_debugchr_editor_unit_fields);
            g_debugchr_panel_refresh_a = 2;
            g_debugchr_panel_refresh_b = one;
            g_debugchr_panel_refresh_c = one;
        }
    }

    {
        s16* selected_unit_ptr;
        s16* comparison_ptr;
        s16* unit_type_id_ptr;

        selected_unit_ptr = &g_debugchr_editor_selected_unit_id;
        unit_type_id_ptr = &g_debugchr_panel_selected_billboard.unit;
        selected_unit_ptr[0] = g_debugchr_editor_unit_ids[0];
        /* A register address keeps cse from folding this store to a direct
         * global address. */
        comparison_ptr = selected_unit_ptr - 5;
        *comparison_ptr = g_debugchr_editor_unit_ids[0];
        *unit_type_id_ptr = g_debugchr_editor_unit_ids[0];
        debugchr_editor_load_selected_unit();
        battle_action_copy_active_turn_data_from(
            unit_type_id_ptr - 5, selected_unit_ptr - 5, g_debugchr_editor_unit_fields);
    }
}
