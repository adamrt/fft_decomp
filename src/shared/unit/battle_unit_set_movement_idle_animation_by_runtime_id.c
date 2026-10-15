#include "fft/battle_unit.h"
#include "psx/types.h"

/* battle_unit_set_idle_animation_for_movement by unit id. No retail code calls
 * it. */
void battle_unit_set_movement_idle_animation_by_runtime_id(u32 runtime_id) {
    battle_unit_set_idle_animation_for_movement(battle_unit_get_by_runtime_id(runtime_id & 0xffff));
}
