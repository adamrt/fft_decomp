#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Draws a unit with the Transparent status semi-transparent with averaging blend
 * (the blend bits cleared), and any other unit opaque. */
void battle_gfx_update_sprite_transparency_flag(battle_unit_t* unit) {
    if ((unit->status_flags_5_6 & BATTLE_UNIT_STATUS_TRANSPARENT) != 0) {
        unit->sprite_display_flags
            = (unit->sprite_display_flags & ~BATTLE_SPRITE_DISPLAY_BLEND_MASK) | BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
    } else {
        unit->sprite_display_flags = unit->sprite_display_flags & ~BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
    }
}
