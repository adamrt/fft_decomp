#include "fft/world.h"
#include "psx/types.h"

/* Menu callback: 1 when the cached ability type is ABILITY_TYPE_DEFAULT, an
 * ability with a range record. */
s32 world_ability_is_selected_action_type(s32 unused_row) {
    return g_menu_ability_type == 0;
}
