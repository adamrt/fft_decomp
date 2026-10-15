#include "fft/main.h"

/* Roll the unit's raw stats by its sex flags; a unit with neither flag is rolled
 * as a monster. */
void main_unit_generate_raw_stats(unit_stats_t* unit) {
    main_unit_generation_type_e unit_type;

    if ((unit->unit_flags & UNIT_FLAG_MALE) != 0) {
        unit_type = MAIN_UNIT_TYPE_MALE;
    } else if ((unit->unit_flags & UNIT_FLAG_FEMALE) != 0) {
        unit_type = MAIN_UNIT_TYPE_FEMALE;
    } else {
        unit_type = MAIN_UNIT_TYPE_MONSTER;
    }
    main_unit_generate_base_raw_stats(unit->raw_stats, unit_type);
}
