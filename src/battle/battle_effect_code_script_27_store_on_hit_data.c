#include "fft/battle_effect.h"
#include "fft/effect.h"

s32 battle_effect_code_script_27_store_on_hit_data(effect_record_t* effect) {
    battle_effect_store_first_section_of_on_hit_data(
        (battle_effect_on_hit_vector_t*)(g_effect_particle_system_data + 4));
    battle_effect_set_inertia_threshold(*(s32*)(g_effect_particle_system_data + 0x10));
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
