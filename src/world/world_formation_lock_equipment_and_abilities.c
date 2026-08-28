#include "fft/data.h"
#include "fft/job.h"
#include "fft/world.h"

s32 world_formation_lock_equipment_and_abilities(s32 unit_id) {
    world_formation_unit_t* unit;

    unit = g_world_formation_unit_pointers[unit_id];
    if (unit->proposition_status != 0)
        return -2;
    if (unit->uses_monster_skillset != 0)
        return -3;
    if (world_job_is_special_monster(unit->job_id) != 0)
        return -3;
    {
        world_formation_unit_t* u2 = g_world_formation_unit_pointers[unit_id];
        if (u2->gender_flags & UNIT_FLAG_EGG)
            return -4;
        if (u2->job_id == JOB_ID_MIME)
            return -6;
    }
    return 1;
}
