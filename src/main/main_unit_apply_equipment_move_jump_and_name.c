#include "fft/main.h"

void main_unit_apply_equipment_move_jump_and_name(battle_stats_t* unit) {
    main_unit_set_equipment_stats(unit);
    main_unit_set_equipment_attributes(unit, 1);
    main_unit_calculate_move_jump(unit, 0);
    main_unit_store_character_names(unit);
}
