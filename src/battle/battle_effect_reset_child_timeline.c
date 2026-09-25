#include "fft/battle.h"

void battle_effect_reset_child_timeline(battle_effect_temp_data_t* data, s16 value) {
    s32 index;

    index = 0;
    do {
        data->values_5e[index] = 0;
        data->values_44[index] = 0;
        data->values_2a[index] = 0;
        index++;
    } while (index < 13);
    data->frame = 0;
    data->target_index = value;
}
