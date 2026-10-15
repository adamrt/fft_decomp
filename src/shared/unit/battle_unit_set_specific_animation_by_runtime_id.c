#include "fft/battle_unit.h"

/* Request `animation` for a unit by runtime id, keeping its facing. */
void battle_unit_set_specific_animation_by_runtime_id(u32 runtime_id, u32 animation) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0)
        battle_unit_store_animation_facing(animation, unit->facing, unit);
}
