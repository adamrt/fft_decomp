#include "fft/battle.h"

void battle_effect_copy_on_hit_data_to_second_section(void) {
    on_hit_effects_data_second_section = on_hit_effects_data;
}
