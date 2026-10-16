/* The learn job list, selected ability prompts and cached job level and JP lookups. */

#include "fft/job.h"
#include "fft/main.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_help.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/menu_text.h"
#include "fft/sound.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "fft/world_formation.h"
#include "fft/world_menu.h"
#include "fft/world_text.h"
#include "psx/pad.h"
#include "psx/types.h"

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
        menu_set_vsync_mode_and_event_speed(2);
        world_job_build_unit_list(g_unit_selected_index, g_menu_job_ids, 0);
        g_world_menu_script_callbacks[0].value = world_job_calculate_current_level;
        g_world_menu_script_callbacks[2].value = world_menu_get_learn_job_jp;
        g_world_menu_script_callbacks[5].value = menu_job_calculate_next_level_jp_requirement;
        g_world_menu_script_callbacks[6].value = world_job_calculate_current_total_jp;
        g_world_menu_script_callbacks[13].value = job_calculate_current_mastered;
        world_menu_get_selection_record(9, &x, &y, g_menu_job_ids);
        menu_init_scrollable_list(g_menu_job_ids, x, y, g_text_section_pointers[TEXT_SECTION_JOB_NAMES]);
        g_world_learn_job_list_initialized = 1;
    }
    g_menu_help_message_text_id = g_menu_job_ids[g_menu_list_selected_index] + 0x6000;
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        menu_set_vsync_mode_and_event_speed(0);
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        g_world_learn_job_list_initialized = 0;
        return MENU_JOB_LIST_STEP_CLOSE;
    }
    /* A mastered job has nothing left to learn, so only View Abilities may open it. */
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (job_calculate_current_mastered(g_menu_list_selected_index) == 0
            || g_world_formation_current_menu == WORLD_FORMATION_MENU_VIEW_ABILITIES) {
            g_world_learn_selected_job = g_menu_job_ids[g_menu_list_selected_index];
            g_world_learn_selected_job_index = job_get_generic_index(g_world_learn_selected_job);
            world_job_calculate_current_level(g_menu_list_selected_index);
            world_menu_get_learn_job_jp(g_menu_list_selected_index);
            world_job_calculate_current_total_jp(g_menu_list_selected_index);
            menu_job_calculate_next_level_jp_requirement(g_menu_list_selected_index);
            menu_set_selection_record(9, g_menu_list_selected_index, g_menu_scroll_base_index, g_menu_job_ids);
            g_world_learn_job_list_initialized = 0;
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            return MENU_JOB_LIST_STEP_ABILITIES;
        }
        world_text_show_message_and_play_sound(0xC007, MAIN_SFX_ERROR_MESSAGE);
    }
    menu_run_script_with_palette_mode(
        &g_world_learn_job_list_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    return MENU_JOB_LIST_STEP_JOBS;
}

/* Store the selected ability-name text ID, then build the prompt. */
void world_menu_build_selected_ability_prompt(void) {
    g_text_substitution_values[0] = g_menu_ability_entries[g_menu_list_selected_index] + TEXT_ID_ABILITY_NAME_BASE;
    menu_icon_linked_entry_thread();
}

/* Menu callback: the selected formation unit's JP in the job chosen in the
 * learn menu. The row argument is not read. */
s32 world_job_get_selected_unit_job_points_entry(s32 unused_row) {
    return g_world_unit_pointers[g_unit_selected_index]->job_points.current.values[g_world_learn_selected_job_index];
}

/* Row callback: return the cached level of the job chosen in the learn menu; the row is ignored. */
s32 world_job_get_cached_level(s32 unused_row) {
    return g_menu_job_current_level;
}

/* Row callback: return the cached JP the chosen job needs for its next level; the row is ignored. */
s32 world_job_get_cached_level_jp_requirement(s32 unused_row) {
    return g_menu_job_next_level_jp_requirement;
}

/* Row callback: return the cached total JP earned in the chosen job; the row is ignored. */
s32 world_job_get_cached_total_jp(s32 unused_row) {
    return g_world_job_cached_total_jp;
}

/* Row callback: return whether the chosen job is mastered (nothing left to learn); the row is
 * ignored. */
s32 world_job_get_cached_skillset_empty_flag(s32 unused_row) {
    return g_job_current_mastered;
}
