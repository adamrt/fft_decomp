#include "fft/main.h"
#include "psx/types.h"

/* Load the party unit selected by an ENTD sprite set.
 *
 * Preserve the party unit's kind flags while taking the ENTD persistence
 * controls and team assignment. Returns -1 when no matching party slot exists. */
s32 main_unit_init_job_data_from_entd(battle_stats_t* unit, entd_unit_t* entd_unit) {
    s32 slot;

    slot = main_party_find_slot_by_sprite_set(entd_unit->sprite_set);
    if (slot != -1) {
        main_unit_init_job_data(unit, slot, g_main_unit_name_uses_world_text);
        /* Keep the party unit's kind flags and take the two persistence
         * controls from the ENTD definition. */
        unit->unit_flags &= ~(UNIT_FLAG_JOIN_AFTER_EVENT | UNIT_FLAG_SAVE_FORMATION);
        unit->unit_flags |= entd_unit->unit_flags & (UNIT_FLAG_JOIN_AFTER_EVENT | UNIT_FLAG_SAVE_FORMATION);
        unit->team_flags = entd_unit->battle_flags;
        unit->initial_team_flags = entd_unit->battle_flags;
        return 0;
    }
    return -1;
}
