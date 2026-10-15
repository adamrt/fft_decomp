/* Formation. A small object between boundaries the binary proves. */

#include "fft/world.h"

s32 world_formation_get_unit_level(s32 unit_id) {
    return g_world_unit_pointers[unit_id]->status.level;
}

s32 world_formation_get_unit_experience(s32 unit_id) {
    return g_world_unit_pointers[unit_id]->status.experience;
}

s32 world_formation_get_unit_brave(s32 unit_id) {
    return g_world_unit_pointers[unit_id]->identity.brave;
}

s32 world_formation_get_unit_faith(s32 unit_id) {
    return g_world_unit_pointers[unit_id]->identity.faith;
}
