#include "fft/battle.h"
#include "psx/types.h"

extern s32 battle_menu_load_charge_skillset(
    s32 unit_id, s32 skillset_id, s16* ability_ids, u8* charge_times, u8* turn_counts);

s32 battle_menu_call_load_charge_skillset(s32 unit_id, s32 skillset_id, s16* ability_ids) {
    s32 charge_times[4];
    s32 turn_counts[4];
    return battle_menu_load_charge_skillset(unit_id, (u8)skillset_id, ability_ids, (u8*)charge_times, (u8*)turn_counts);
}
