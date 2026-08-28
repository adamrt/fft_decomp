#include "fft/battle.h"
#include "fft/main_runtime.h"

struct battle_effect_secondary_init;

/* Build the evade secondary effect data, then start the element effect of the
 * used weapon; is_bow is 1 for the bow hardcoding path and 0 otherwise. */
void battle_effect_set_evade_type_data_and_weapon_element(battle_unit_misc_data_t* unit, s32 is_bow) {
    u8 secondary_init[0xc8];
    battle_effect_build_secondary_init_from_action(unit, (struct battle_effect_secondary_init*)secondary_init);
    /* The target passes is_bow as a third argument the two-parameter callee ignores. */
    ((void (*)(u8, u8*, s32))battle_effect_init_ninja_ball_secondary)(
        g_main_item_weapon_data[unit->used_item_or_weapon_id].element, secondary_init, is_bow);
}
