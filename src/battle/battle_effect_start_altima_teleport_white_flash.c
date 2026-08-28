#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_start_altima_teleport_white_flash(battle_unit_misc_data_t* unit) {
    if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_FIRST_FORM
        || unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM) {
        battle_effect_play();
    }
    unit->sprite_display_flags.half = (unit->sprite_display_flags.half & 0xff9f) | 0x21;
    battle_gfx_start_misc_unit_palette_modulation(4, 2, unit->unit_id, 0x1f, 0x1f, 0x1f);
    unit->distortion_phase++;
    unit->distortion_timer = 0;
}
