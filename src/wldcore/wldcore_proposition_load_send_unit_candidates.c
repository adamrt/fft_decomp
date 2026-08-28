#include "fft/text.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Build the list of available non-guest human units for a proposition.
 *
 * Units already assigned to a proposition, absent roster entries, monsters,
 * and special-character sprite sets are excluded. */
s32 wldcore_proposition_load_send_unit_candidates(wldcore_menu_send_unit_level_t* level) {
    s32 i;
    s32 j;
    party_data_t* unit;

    i = 0;
    level->unit_count = 0;
    for (; i < PARTY_GUEST_SLOT_FIRST; i++) {
        unit = wldcore_get_party_data_pointer(i);
        if (unit->proposition_status != 0) {
            continue;
        }
        if (unit->party_id == PARTY_ID_NONE) {
            continue;
        }
        if (unit->gender_flags & UNIT_FLAG_MONSTER) {
            continue;
        }
        if (unit->sprite_set < CHARACTER_IDENTITY_BOY) {
            continue;
        }
        for (j = 0; j < g_wldcore_proposition_send_unit_count; j++) {
            if (g_wldcore_proposition_send_units[j] == i) {
                break;
            }
        }
        if (j < g_wldcore_proposition_send_unit_count) {
            g_wldcore_list_row_flags[level->unit_count] = 8;
        } else {
            g_wldcore_list_row_flags[level->unit_count] = 0;
        }
        g_wldcore_list_entry_values[level->unit_count] = i + TEXT_ID_UNIT_NAME_BASE;
        g_wldcore_proposition_candidate_levels[level->unit_count] = unit->level;
        level->unit_count = level->unit_count + 1;
    }
    if (level->unit_count < 6) {
        i = level->unit_count;
    } else {
        i = 6;
    }
    return i;
}
