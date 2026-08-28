#include "fft/data.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * Build the selected unit's job-wheel choices and availability flags.
 *
 * Units away on propositions have no choices. Fewer than 2 choices bypass
 * the wheel; otherwise its opening animation starts at 20 frames.
 */
void world_formation_build_job_wheel_availability(void) {
    s16* available;
    s32 i;
    /* Pin: unpinned, job and the loop counter exchange $s2 and $s1. */
    register u16* job __asm__("$18");
    /* Preserve the target's 8-byte gap between outgoing arguments and saves. */
    s32 unused[2];
    s16 count;
    s16 unit;
    u16 entry;
    s32 id;

    job = g_world_job_wheel_jobs;
    count = world_job_build_unit_list(g_world_formation_selected_unit_index, (s16*)g_world_job_wheel_jobs, 2);
    i = 0;
    g_world_job_wheel_job_count = count;
    if (count > 0) {
        available = g_world_job_wheel_mastered_jobs;
    /* A C loop hoists the unit-pointer table base into an extra saved register. */
    next_job: {
        unit = g_world_formation_selected_unit_index;
        entry = *job;
        id = entry & 0x3FF;
        if (id == g_world_formation_unit_pointers[unit]->job_id || !(entry & 0x4000)) {
            *available
                = world_ability_find_unit_abilities(unit, id, 0xF, 0, WORLD_ABILITY_LIST_MODE_COUNT_UNLEARNED) == 0;
        } else {
            *available = 0;
        }
        available++;
        job++;
    }
        count = g_world_job_wheel_job_count;
        i++;
        if (i < count) {
            goto next_job;
        }
    }
    if (g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->proposition_status != 0) {
        g_world_job_wheel_job_count = 0;
    }
    g_world_job_wheel_cursor_index = 0;
    world_formation_build_job_wheel_job_name();
    if (g_world_job_wheel_job_count < 2) {
        g_world_formation_current_menu = 3;
        return;
    }
    g_world_job_wheel_open_frame = 0x14;
    g_world_job_wheel_opening = 1;
}
