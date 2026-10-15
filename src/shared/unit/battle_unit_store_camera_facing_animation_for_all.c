#include "fft/battle_unit.h"
#include "psx/types.h"

/* Request animation on every unit, keeping its camera-facing quadrant. */
void battle_unit_store_camera_facing_animation_for_all(u32 animation) {
    battle_unit_t* unit = g_battle_unit_list_head;

    while (unit != 0) {
        battle_unit_store_animation_facing(animation, unit->camera_facing_quadrant, unit);
        unit = unit->previous;
    }
}
