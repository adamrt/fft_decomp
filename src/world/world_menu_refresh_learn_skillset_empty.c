#include "fft/data.h"
#include "fft/world.h"
#include "psx/types.h"

/* Caches and returns whether the `index`th unlocked job's skillset has nothing to show. */
s32 world_menu_refresh_learn_skillset_empty(s32 index) {
    return g_world_job_cached_skillset_empty
        = world_ability_find_unit_abilities(g_world_formation_selected_unit_index,
              g_world_menu_learn_unlocked_jobs[index], 0xf, 0, WORLD_ABILITY_LIST_MODE_COUNT_UNLEARNED)
        == 0;
}
