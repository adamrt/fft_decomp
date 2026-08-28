#include "fft/battle.h"
#include "psx/types.h"

/* Request animation on every unit, keeping its camera-facing quadrant. */
void battle_unit_store_camera_facing_animation_for_all(u32 animation) {
    battle_unit_misc_data_t* unit = g_battle_unit_last_misc_data;

    while (unit != 0) {
        battle_unit_store_animation_facing(animation, unit->camera_facing_quadrant.s, unit);
        unit = unit->previous;
    }
}
