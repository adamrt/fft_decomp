/* Gravity and inertia threshold setters with save/restore. */

#include "fft/battle_effect.h"

void effect_set_gravity(const vec3s32_t* src) {
    g_effect_gravity = *src;
}

void effect_set_inertia_threshold(s32 threshold) {
    g_effect_inertia_threshold = threshold;
}

void effect_save_gravity(void) {
    g_effect_saved_gravity = g_effect_gravity;
}

void effect_restore_gravity(void) {
    g_effect_gravity = g_effect_saved_gravity;
}
