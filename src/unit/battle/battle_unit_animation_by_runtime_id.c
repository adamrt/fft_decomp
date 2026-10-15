/* Animation requests addressed by runtime id. */

#include "fft/battle_unit.h"
#include "psx/types.h"

void battle_unit_update_graphics_by_runtime_id(u32 runtime_id) {
    battle_unit_update_display_by_runtime_id(runtime_id);
}

void battle_unit_set_mounted_animation_by_runtime_id(u32 runtime_id) {
    battle_unit_set_animation_based_on_mounted_state(battle_unit_get_by_runtime_id(runtime_id & 0xffff));
}

/* battle_unit_set_idle_animation_for_movement by unit id. No retail code calls
 * it. */
void battle_unit_set_movement_idle_animation_by_runtime_id(u32 runtime_id) {
    battle_unit_set_idle_animation_for_movement(battle_unit_get_by_runtime_id(runtime_id & 0xffff));
}

void battle_unit_set_status_animation_by_runtime_id(u32 runtime_id) {
    battle_unit_set_animation_based_on_status(battle_unit_get_by_runtime_id(runtime_id & 0xffff));
}

/* Request `animation` for a unit by runtime id, keeping its facing. */
void battle_unit_set_specific_animation_by_runtime_id(u32 runtime_id, u32 animation) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0)
        battle_unit_store_animation_facing(animation, unit->facing, unit);
}

/* Request animation on every unit, keeping its camera-facing quadrant. */
void battle_unit_store_camera_facing_animation_for_all(u32 animation) {
    battle_unit_t* unit = g_battle_unit_list_head;

    while (unit != 0) {
        battle_unit_store_animation_facing(animation, unit->camera_facing_quadrant, unit);
        unit = unit->previous;
    }
}
