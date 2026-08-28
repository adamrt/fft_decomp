#include "fft/battle_effect.h"

void battle_effect_copy_second_section_to_on_hit_data(void) {
    on_hit_effects_data = on_hit_effects_data_second_section;
}
