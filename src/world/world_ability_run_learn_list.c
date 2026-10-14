#include "fft/menu.h"
#include "fft/sound.h"
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

    if (g_world_learn_ability_list_open == 0) {
        g_world_menu_script_callbacks[0].value = world_job_get_cached_level;
        g_world_menu_script_callbacks[1].value = world_job_get_cached_total_jp;
        g_world_menu_script_callbacks[2].value = world_job_get_cached_level_jp_requirement;
        g_world_menu_script_callbacks[3].value = world_job_get_selected_unit_job_points_entry;
        g_world_menu_script_callbacks[4].value = world_ability_is_non_action_learned;
        g_world_menu_script_callbacks[5].value = world_job_get_cached_skillset_empty_flag;
        g_world_menu_script_callbacks[6].value = world_menu_is_learn_ability_learned;
        g_world_menu_script_callbacks[7].value = world_ability_get_mp_cost_display_value;
        g_world_menu_script_callbacks[8].value = world_ability_get_ct_display_value;
        g_world_menu_script_callbacks[9].value = menu_ability_get_jp_cost_display_value;
        g_world_menu_script_callbacks[10].value = world_ability_is_category_movement;
        g_world_menu_script_callbacks[11].value = world_ability_is_category_support;
        g_world_menu_script_callbacks[12].value = world_ability_is_category_reaction;
        g_world_menu_script_callbacks[13].value = world_ability_is_category_action;
        g_world_menu_script_callbacks[14].value = world_ability_is_non_action_unlearned;
        g_world_learn_confirm_prompt_state = 0;
        g_world_menu_script_callbacks[15].value = world_ability_is_selected_action_type;
        world_menu_clear_selection_record(10);
        world_menu_clear_selection_record(11);
        world_menu_clear_selection_record(12);
        world_menu_clear_selection_record(13);
        g_world_ability_category = world_menu_update_wrapped_horizontal_selection(4, WORLD_MENU_CURSOR_ABILITY_TAB, 0);
        world_ability_find_unit_abilities(g_world_formation_selected_unit_index, g_world_learn_selected_job,
            g_world_ability_category, g_menu_ability_entries, WORLD_ABILITY_LIST_MODE_LEARN_MENU);
        world_menu_init_scrollable_list_core(
            g_menu_ability_entries, 0, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
        rect.x = 0x240;
        rect.y = 0x1c8;
        rect.w = 0x14;
        rect.h = 0x10;
        g_world_learn_job_name_list[0] = g_world_learn_selected_job;
        world_text_render_id_list_to_image_rows(
            g_text_section_pointers[TEXT_SECTION_JOB_NAMES], g_world_learn_job_name_list, &rect, 0);
        g_world_learn_ability_list_open = 1;
        g_world_formation_cursor_ot_override = 0x3c;
    }
    if (g_world_menu_option_count == 0) {
        g_world_menu_description_text_id = -1;
    } else {
        g_world_menu_description_text_id
            = (g_menu_ability_entries[g_world_menu_cursor_position] & ABILITY_LIST_ENTRY_ID_MASK) + 0x7800;
    }
    if (g_world_learn_confirm_prompt_state == 0) {
        if ((g_world_input_primary_repeat & PSX_PAD_CIRCLE) && g_world_menu_option_count != 0) {
            index = g_world_menu_cursor_position;
            if (g_menu_ability_entries[index] >> 14) {
                world_text_show_message_and_play_sound(0xc009, MAIN_SFX_ERROR_MESSAGE);
            } else if (world_menu_is_learn_ability_learned(index) != 0) {
                g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
            } else {
                g_world_learn_confirm_prompt_state
                    = world_menu_run_thread(THREAD_SCREEN_SLOT_MENU, g_world_learn_confirm_menu);
                g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
                g_world_learn_confirm_submenu.selected_index = 0;
                g_sound_suppressed = 1;
            }
        } else if (g_world_input_primary_repeat & PSX_PAD_CROSS) {
            g_world_learn_ability_list_open = 0;
            g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_cursor_ot_override = 0;
            return MENU_JOB_LIST_STEP_JOBS;
        }
        category = world_menu_update_horizontal_selection_and_mark_change(
            4, WORLD_MENU_CURSOR_ABILITY_TAB, g_world_input_newly_pressed, MAIN_SFX_PAGE_SWITCH);
        if (category != g_world_ability_category) {
            menu_set_selection_record(g_world_ability_category + 10, g_world_menu_cursor_position,
                g_world_menu_scroll_offset, g_menu_ability_entries);
            g_world_ability_category = category;
            world_ability_find_unit_abilities(g_world_formation_selected_unit_index, g_world_learn_selected_job,
                g_world_ability_category, g_menu_ability_entries, WORLD_ABILITY_LIST_MODE_LEARN_MENU);
            world_menu_get_selection_record(g_world_ability_category + 10, &x, &y, g_menu_ability_entries);
            world_menu_init_scrollable_list(
                g_menu_ability_entries, x, y, g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
            world_menu_set_window_scale_step(10);
        }
    } else {
        g_world_learn_confirm_prompt_state = world_menu_run_thread(THREAD_SCREEN_SLOT_MENU, g_world_learn_confirm_menu);
        if (g_world_learn_confirm_prompt_state == 0) {
            if (g_world_menu_selection_results[5] == 0) {
                world_ability_learn(g_world_formation_selected_unit_index, g_world_learn_selected_job,
                    g_menu_ability_entries[g_world_menu_cursor_position]);
                category = g_main_ability_data[g_menu_ability_entries[g_world_menu_cursor_position]].jp_cost;
                g_world_unit_pointers[g_world_formation_selected_unit_index]
                    ->job_points.current.values[g_world_learn_selected_job_index] -= category;
                main_sound_play_sfx_find_channel(MAIN_SFX_ABILITY_LEARNED);
            } else if (g_world_menu_selection_results[5] == 1) {
                g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            }
            g_sound_suppressed = 0;
            world_ability_find_unit_abilities(g_world_formation_selected_unit_index, g_world_learn_selected_job,
                g_world_ability_category, g_menu_ability_entries, WORLD_ABILITY_LIST_MODE_LEARN_MENU);
            world_menu_init_scrollable_list_core(g_menu_ability_entries, g_world_menu_cursor_position,
                g_text_section_pointers[TEXT_SECTION_ABILITY_NAMES]);
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
    if (g_menu_input_lock_mode != 0 || running != 0) {
        mode = 1;
    }
    menu_run_script_with_palette_mode(&g_world_ability_list_script, input, mode);
    rect.x = 0x26;
    rect.y = 0x2d;
    rect.w = 0x50;
    rect.h = 0x10;
    world_gfx_enqueue_textured_quad(&rect, 0, 0xc8, 0, 0, g_world_menu_window_tpage, g_world_menu_window_clut, 0xb);
    return MENU_JOB_LIST_STEP_ABILITIES;
}
