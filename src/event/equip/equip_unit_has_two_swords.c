/* Whether a unit has the Two Swords support ability set. */

#include "fft/event_equip.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Return nonzero when the unit has the Two Swords support ability set. */
s32 equip_unit_has_two_swords(s16 unit_id) {
    return g_equip_unit_data[unit_id]->support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_SWORDS;
}
