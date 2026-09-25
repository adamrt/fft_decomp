#include "fft/main.h"

void main_unit_generate_raw_stats(battle_stats_t* unit) {
    s32 unit_type;

    if ((unit->unit_flags & UNIT_FLAG_MALE) != 0) {
        unit_type = MAIN_UNIT_TYPE_MALE;
    } else if ((unit->unit_flags & UNIT_FLAG_FEMALE) != 0) {
        unit_type = MAIN_UNIT_TYPE_FEMALE;
    } else {
        unit_type = MAIN_UNIT_TYPE_MONSTER;
    }
    main_unit_generate_base_raw_stats(unit->raw_stats, unit_type);
}
