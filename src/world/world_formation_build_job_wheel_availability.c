#include "fft/world.h"
#include "psx/types.h"

/*
 * Build the selected unit's job-wheel choices and availability flags.
 *
 * Units away on propositions have no choices. Fewer than 2 choices bypass
 * the wheel; otherwise its opening animation starts at 20 frames.
 */
void world_formation_build_job_wheel_availability(void) {
    s32 i;
    s16 unit;
    u16 entry;
    s32 id;

    g_world_job_wheel_job_count = world_job_build_unit_list(
        g_unit_selected_index, g_world_job_wheel_jobs, WORLD_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS);
    for (i = 0; i < g_world_job_wheel_job_count; i++) {
        unit = g_unit_selected_index;
        entry = g_world_job_wheel_jobs[i];
        id = entry & WORLD_JOB_LIST_ENTRY_ID_MASK;
        if (id == g_world_unit_pointers[unit]->identity.job_id || !(entry & WORLD_JOB_LIST_ENTRY_UNAVAILABLE)) {
            g_world_job_wheel_mastered_jobs[i] = ability_build_list_variant(unit, id, MENU_ABILITY_CATEGORY_ALL, 0,
                                                     WORLD_ABILITY_LIST_MODE_COUNT_UNLEARNED)
                == 0;
        } else {
            g_world_job_wheel_mastered_jobs[i] = 0;
        }
    }
    if (g_world_unit_pointers[g_unit_selected_index]->proposition_status != 0) {
        g_world_job_wheel_job_count = 0;
    }
    g_world_job_wheel_cursor_index = 0;
    world_formation_build_job_wheel_job_name();
    if (g_world_job_wheel_job_count < 2) {
        g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL;
        return;
    }
    g_world_job_wheel_open_frame = 0x14;
    g_world_job_wheel_opening = 1;
}
