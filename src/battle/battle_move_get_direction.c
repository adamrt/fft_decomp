#include "fft/battle.h"

s32 battle_move_get_direction(const battle_unit_misc_data_t* unit) {
    s32 direction;

    if (unit->map_y < unit->movement.bytes.destination_y) {
        direction = 2;
    } else if (unit->movement.bytes.destination_y < unit->map_y) {
        direction = 0;
    } else if (unit->map_x > unit->movement.bytes.destination_x) {
        direction = 3;
    } else if (unit->map_x < unit->movement.bytes.destination_x) {
        direction = 1;
    } else {
        direction = 2;
    }
    return direction;
}
