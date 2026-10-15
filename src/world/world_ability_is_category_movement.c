#include "fft/menu.h"
#include "fft/sound.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Sibling of world_ability_is_category_action (category 0). */
s32 world_ability_is_category_movement(s32 unused_row) {
    return g_world_ability_category == MENU_ABILITY_CATEGORY_MOVEMENT;
}
