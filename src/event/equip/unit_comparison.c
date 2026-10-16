/* The equipment screen's comparison slot copy and the Two Swords and Two
 * Hands support checks. */

#include "fft/event_equip.h"
#include "fft/unit.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Copy the record of unit 0 to unit 1, the comparison slot.
 * Neither argument is read. */
void equip_unit_copy_data_to_compare_slot(s32 unused_selected_index, s32 unused_1) {
    bcopy(g_equip_unit_data[0], g_equip_unit_data[1], sizeof(equip_unit_data_t));
}

/* Return nonzero when the unit has the Two Swords support ability set. */
s32 equip_unit_has_two_swords(s16 unit_id) {
    return g_equip_unit_data[unit_id]->support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_SWORDS;
}

/* Return nonzero when the unit has the Two Hands support ability set. Nothing in EQUIP.OUT calls
 * it. */
s32 equip_unit_has_two_hands(s16 unit_id) {
    return g_equip_unit_data[unit_id]->support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS;
}
