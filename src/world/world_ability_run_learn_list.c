#include "fft/battle_ability.h"
#include "fft/data.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Run the learn-abilities list for the job chosen in the learn-menu job list.
 *
 * Learn-menu twin of world_ability_run_view_list. Confirming an unlearned
 * ability opens the confirmation menu thread; when it closes with choice 0
 * the ability is learned and its JP cost deducted from the unit's JP in the
 * job. Cancel returns 0 (back to the job list), otherwise 1.
 */
s32 world_ability_run_learn_list(void) {
    s32 unused[160];
    RECT rect;
    s16 x;
    s16 y;
    s32 category;
    u8 running;
    s32 input;
    s32 mode;
    s16 index;

    if (*(s8*)&g_world_learn_ability_list_open == 0) {
        g_world_menu_script_callbacks[0] = (s32 (*)(s32))world_job_get_cached_level;
        g_world_menu_script_callbacks[1] = (s32 (*)(s32))world_job_get_cached_total_jp;
        g_world_menu_script_callbacks[2] = (s32 (*)(s32))world_job_get_cached_level_jp_requirement;
        g_world_menu_script_callbacks[3] = (s32 (*)(s32))world_job_get_selected_unit_job_points_entry;
        g_world_menu_script_callbacks[4] = world_ability_cache_entry_pointers;
        g_world_menu_script_callbacks[5] = (s32 (*)(s32))world_job_get_cached_skillset_empty_flag;
        g_world_menu_script_callbacks[6] = world_menu_is_learn_ability_learned;
        g_world_menu_script_callbacks[7] = world_ability_get_mp_cost_display_value;
        g_world_menu_script_callbacks[8] = world_ability_get_ct_display_value;
        g_world_menu_script_callbacks[9] = world_ability_get_jp_cost_display_value;
        g_world_menu_script_callbacks[10] = (s32 (*)(s32))world_ability_is_category_movement;
        g_world_menu_script_callbacks[11] = (s32 (*)(s32))world_ability_is_category_support;
        g_world_menu_script_callbacks[12] = (s32 (*)(s32))world_ability_is_category_reaction;
        g_world_menu_script_callbacks[13] = (s32 (*)(s32))world_ability_is_category_action;
        g_world_menu_script_callbacks[14] = world_ability_is_non_action_unlearned;
        g_world_learn_confirm_prompt_state = 0;
        g_world_menu_script_callbacks[15] = (s32 (*)(s32))world_ability_is_selected_action_type;
        world_gfx_clear_sprite_slot(10);
        world_gfx_clear_sprite_slot(11);
        world_gfx_clear_sprite_slot(12);
        world_gfx_clear_sprite_slot(13);
        g_world_ability_category = world_move_menu_cursor_horizontal(4, 8, 0);
        world_ability_find_unit_abilities(g_world_formation_selected_unit_index, g_world_learn_selected_job,
            g_world_ability_category, g_world_ability_entries, WORLD_ABILITY_LIST_MODE_LEARN_MENU);
        world_menu_init_scrollable_list_core(g_world_ability_entries, 0, (u32)g_world_text_ability_names);
        rect.x = 0x240;
        rect.y = 0x1c8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_world_learn_job_name_list[0] = g_world_learn_selected_job;
        world_text_render_id_list_to_image_rows(g_world_text_job_names, (s16*)g_world_learn_job_name_list, &rect, 0);
        g_world_learn_ability_list_open = 1;
        g_world_formation_cursor_ot_override = 0x3c;
    }
    if (g_world_menu_option_count == 0) {
        g_world_menu_description_text_id = -1;
    } else {
        g_world_menu_description_text_id
            = ((u16)g_world_ability_entries[g_world_menu_cursor_position] & 0x3ff) + 0x7800;
    }
    if (g_world_learn_confirm_prompt_state == 0) {
        if ((g_world_input_primary_repeat & PSX_PAD_CIRCLE) && g_world_menu_option_count != 0) {
            index = g_world_menu_cursor_position;
            if ((u16)g_world_ability_entries[index] >> 14) {
                world_text_show_message_and_play_sound(0xc009, 0x30);
            } else if (world_menu_is_learn_ability_learned(index) != 0) {
                g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
            } else {
                g_world_learn_confirm_prompt_state = world_menu_run_thread(6, g_world_learn_confirm_menu);
                g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
                g_world_learn_confirm_submenu.cursor = 0;
                g_world_menu_sound_muted = 1;
            }
        } else if (g_world_input_primary_repeat & PSX_PAD_CROSS) {
            g_world_learn_ability_list_open = 0;
            g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_cursor_ot_override = 0;
            return 0;
        }
        category = world_menu_step_cursor_with_sound_2(4, 8, g_world_input_newly_pressed, 6);
        if (category != g_world_ability_category) {
            world_gfx_set_sprite_slot(g_world_ability_category + 10, g_world_menu_cursor_position,
                g_world_menu_scroll_offset, (u16*)g_world_ability_entries);
            g_world_ability_category = category;
            /* The target passes the category narrowed to a halfword. */
            world_ability_find_unit_abilities(g_world_formation_selected_unit_index, g_world_learn_selected_job,
                (s16)category, g_world_ability_entries, WORLD_ABILITY_LIST_MODE_LEARN_MENU);
            world_gfx_get_sprite_slot(g_world_ability_category + 10, &x, &y, g_world_ability_entries);
            world_menu_init_scrollable_list(g_world_ability_entries, x, y, (u32)g_world_text_ability_names);
            world_menu_set_window_scale_step(10);
        }
    } else {
        g_world_learn_confirm_prompt_state = world_menu_run_thread(6, g_world_learn_confirm_menu);
        if (g_world_learn_confirm_prompt_state == 0) {
            if (g_world_menu_selection_results[5] == 0) {
                world_ability_learn(g_world_formation_selected_unit_index, g_world_learn_selected_job,
                    g_world_ability_entries[g_world_menu_cursor_position]);
                category = g_main_ability_data[g_world_ability_entries[g_world_menu_cursor_position]].jp_cost;
                g_world_formation_unit_pointers[g_world_formation_selected_unit_index]
                    ->job_points[g_world_learn_selected_job_index] -= category;
                main_sound_play_sfx_find_channel(0x98);
            } else if (g_world_menu_selection_results[5] == 1) {
                g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            }
            g_world_menu_sound_muted = 0;
            world_ability_find_unit_abilities(g_world_formation_selected_unit_index, g_world_learn_selected_job,
                g_world_ability_category, g_world_ability_entries, WORLD_ABILITY_LIST_MODE_LEARN_MENU);
            world_menu_init_scrollable_list_core(
                g_world_ability_entries, g_world_menu_cursor_position, (u32)g_world_text_ability_names);
            world_menu_set_window_scale_step(10);
            world_menu_reset_selection_results();
        }
    }
    running = g_world_learn_confirm_prompt_state;
    input = 0;
    if (running == 0) {
        input = g_world_input_primary_repeat;
    }
    mode = 0;
    if (g_world_thread_task_active != 0 || running != 0) {
        mode = 1;
    }
    world_menu_run_script_with_palette_mode(g_world_ability_list_script, input, mode);
    rect.x = 0x26;
    rect.y = 0x2d;
    rect.w = 0x50;
    rect.h = 0x10;
    world_gfx_enqueue_textured_quad(&rect, 0, 0xc8, 0, 0, g_world_menu_window_tpage, g_world_menu_window_clut, 0xb);
    return 1;
}
