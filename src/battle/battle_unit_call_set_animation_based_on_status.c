#include "fft/battle.h"
#include "psx/types.h"

struct battle_unit_misc_data;

void battle_unit_call_set_animation_based_on_status(struct battle_unit_misc_data* unit) {
    battle_unit_set_animation_based_on_status(unit);
}
