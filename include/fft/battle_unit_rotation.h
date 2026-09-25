#ifndef FFT_BATTLE_UNIT_ROTATION_H
#define FFT_BATTLE_UNIT_ROTATION_H

#include "fft/world.h"

/* BATTLE and WORLD use the same seven-byte per-unit rotation state. */
typedef world_unit_animation_state_t battle_unit_rotation_state_t;

extern battle_unit_rotation_state_t g_battle_unit_misc_rotation_data[];

#endif
