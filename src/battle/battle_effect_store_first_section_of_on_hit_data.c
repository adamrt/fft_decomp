#include "fft/battle.h"

void battle_effect_store_first_section_of_on_hit_data(battle_effect_on_hit_vector_t* src) {
    on_hit_effects_data = *src;
}
