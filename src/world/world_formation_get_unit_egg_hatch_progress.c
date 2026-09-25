#include "fft/world.h"

s32 world_formation_get_unit_egg_hatch_progress(s32 unit_id) {
    world_formation_unit_t* unit = g_world_formation_unit_pointers[unit_id];
    s32 low = unit->birthday & 0xF;
    return (unit->birthday >> 4) - low;
}
