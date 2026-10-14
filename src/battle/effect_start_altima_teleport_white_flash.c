#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"

/* Teleport distortion: begin the departure flash. Altima's forms release the
 * ability effect that effect_start_altima_teleport_departure requested;
 * the sprite switches to additive semi-transparency and a palette modulation of
 * +31 on each color component starts. The distortion moves to its next phase
 * with the timer at 0. */
void effect_start_altima_teleport_white_flash(battle_unit_t* unit) {
    if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_FIRST_FORM
        || unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM) {
        effect_play();
    }
    unit->sprite_display_flags = (unit->sprite_display_flags & ~BATTLE_SPRITE_DISPLAY_BLEND_MASK)
        | (BATTLE_SPRITE_DISPLAY_BLEND_ADD | BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT);
    battle_gfx_apply_unit_palette_modulation_to_units(MAP_COLOR_MODE_BASE, 2, unit->runtime_id, 0x1f, 0x1f, 0x1f);
    unit->distortion_phase++;
    unit->distortion_timer = 0;
}
