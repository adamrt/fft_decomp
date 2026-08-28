#include "fft/battle.h"
#include "psx/types.h"

void battle_gfx_update_sprite_transparency_flag(battle_unit_misc_data_t* unit) {
    u16 sprite_flags;

    if ((unit->status_flags_5_6 & BATTLE_MISC_STATUS_TRANSPARENT) != 0) {
        sprite_flags = (unit->sprite_display_flags.half & 0xff9f) | 1;
    } else {
        sprite_flags = unit->sprite_display_flags.half & 0xfffe;
    }
    unit->sprite_display_flags.half = sprite_flags;
}
