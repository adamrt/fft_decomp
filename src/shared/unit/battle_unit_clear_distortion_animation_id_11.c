#include "fft/battle_unit.h"

/* Distortion animation 11: ends at once. The same routine as animation 10, compiled twice. */
void battle_unit_clear_distortion_animation_id_11(battle_unit_t* unit) {
    unit->distortion_animation_id = BATTLE_DISTORTION_ANIMATION_NONE;
}
