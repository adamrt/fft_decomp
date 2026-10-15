#include "fft/battle_unit.h"

/* Distortion animation 10 of g_battle_unit_distortion_animation_handlers: ends at once. Animation 11
 * is a second copy of this routine. */
void battle_unit_clear_distortion_animation_id_10(battle_unit_t* unit) {
    unit->distortion_animation_id = BATTLE_DISTORTION_ANIMATION_NONE;
}
