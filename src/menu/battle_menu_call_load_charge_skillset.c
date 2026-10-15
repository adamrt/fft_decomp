#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "psx/types.h"

/* battle_menu_load_charge_skillset for a caller that only wants the ability ids;
 * the charge times and turn counts go to scratch arrays. */
s32 battle_menu_call_load_charge_skillset(s32 unit_id, u8 skillset_id, s16* ability_ids) {
    u8 charge_times[16];
    u8 turn_counts[16];
    return battle_menu_load_charge_skillset(unit_id, skillset_id, ability_ids, charge_times, turn_counts);
}
