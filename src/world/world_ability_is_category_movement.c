#include "fft/world.h"
#include "psx/types.h"

/* Sibling of world_ability_is_category_action (category 0). */
s32 world_ability_is_category_movement(void) {
    return g_world_ability_category == 3;
}
