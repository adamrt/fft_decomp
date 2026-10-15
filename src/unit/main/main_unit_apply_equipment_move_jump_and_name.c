#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

void main_unit_apply_equipment_move_jump_and_name(unit_stats_t* unit) {
    main_unit_set_equipment_stats(unit);
    main_unit_set_equipment_attributes(unit, 1);
    main_unit_calculate_move_jump(unit, 0);
    main_unit_store_character_names(unit);
}
