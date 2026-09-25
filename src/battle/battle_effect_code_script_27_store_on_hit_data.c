#include "fft/battle_effect.h"
#include "fft/effect.h"

s32 battle_effect_code_script_27_store_on_hit_data(effect_record_t* effect) {
    battle_effect_store_first_section_of_on_hit_data((battle_effect_on_hit_vector_t*)g_effect_geometry_table->gravity);
    battle_effect_set_inertia_threshold(g_effect_geometry_table->inertia_threshold);
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
