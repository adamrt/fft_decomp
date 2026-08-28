#include "fft/battle.h"
struct battle_unit_misc_data;

void battle_unit_init_coordinates(struct battle_unit_misc_data* unit) {
    battle_unit_init_coordinates_animation_facing(unit);
}
