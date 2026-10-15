#include "fft/menu.h"
#include "fft/sound.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

s32 world_ability_is_category_action(s32 unused_row) {
    return g_world_ability_category == MENU_ABILITY_CATEGORY_ACTION;
}
