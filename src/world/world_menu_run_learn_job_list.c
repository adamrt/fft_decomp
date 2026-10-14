#include "fft/main.h"
#include "fft/world.h"
#include "psx/pad.h"

/* Runs the learn menu's job list for the selected formation unit.
 *
 * The first call builds the unlocked-job list and installs the job
 * level/JP menu-script callbacks. Returns -1 when canceled, 1 when a job
 * with learnable abilities is confirmed (caching its id and generic job
 * index), and 0 while the list is still open. */
s32 world_menu_run_learn_job_list(void) {
    s16 x;
    s16 y;

    if (g_world_learn_job_list_initialized == 0) {
        world_script_set_vsync_mode_and_event_speed(2);
        world_job_build_unit_list(g_world_formation_selected_unit_index, g_world_menu_learn_unlocked_jobs, 0);
        g_world_menu_script_callbacks[0].value = world_job_calculate_current_level;
        g_world_menu_script_callbacks[2].value = world_menu_get_learn_job_jp;
        g_world_menu_script_callbacks[5].value = world_job_calculate_next_level_jp_requirement;
        g_world_menu_script_callbacks[6].value = world_job_calculate_current_total_jp;
        g_world_menu_script_callbacks[13].value = world_job_calculate_current_mastered;
        world_menu_get_selection_record(9, &x, &y, g_world_menu_learn_unlocked_jobs);
        world_menu_init_scrollable_list(
            g_world_menu_learn_unlocked_jobs, x, y, g_text_section_pointers[TEXT_SECTION_JOB_NAMES]);
        g_world_learn_job_list_initialized = 1;
    }
    g_world_menu_description_text_id = g_world_menu_learn_unlocked_jobs[g_world_menu_cursor_position] + 0x6000;
    if (g_world_input_primary_repeat & PSX_PAD_CROSS) {
        world_script_set_vsync_mode_and_event_speed(0);
        g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
        g_world_learn_job_list_initialized = 0;
        return MENU_JOB_LIST_STEP_CLOSE;
    }
    if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (world_job_calculate_current_mastered(g_world_menu_cursor_position) == 0
            || g_world_formation_current_menu == WORLD_FORMATION_MENU_VIEW_ABILITIES) {
            g_world_learn_selected_job = g_world_menu_learn_unlocked_jobs[g_world_menu_cursor_position];
            g_world_learn_selected_job_index = job_get_generic_index(g_world_learn_selected_job);
            world_job_calculate_current_level(g_world_menu_cursor_position);
            world_menu_get_learn_job_jp(g_world_menu_cursor_position);
            world_job_calculate_current_total_jp(g_world_menu_cursor_position);
            world_job_calculate_next_level_jp_requirement(g_world_menu_cursor_position);
            world_menu_set_selection_record(
                9, g_world_menu_cursor_position, g_world_menu_scroll_offset, g_world_menu_learn_unlocked_jobs);
            g_world_learn_job_list_initialized = 0;
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            return MENU_JOB_LIST_STEP_ABILITIES;
        }
        world_text_show_message_and_play_sound(0xC007, MAIN_SFX_ERROR_MESSAGE);
    }
    world_menu_run_script_with_palette_mode(
        &g_world_learn_job_list_script, g_world_input_primary_repeat, g_menu_input_lock_mode);
    return MENU_JOB_LIST_STEP_JOBS;
}
