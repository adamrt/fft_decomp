#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Starts a unit's fade-out removal with a color flash.
 *
 * The unit moves to the head of the runtime list, enters BATTLE_UNIT_FADE_OUT_FLASH,
 * loses its shadow and its float and depth offsets, and is drawn with additive
 * blending while its palette fades by (red, green, blue) from the last applied
 * color. `mode` is the amount map_modify_palette steps the fade by, not a
 * color mode. battle_unit_update_fade_out_removal continues from there. */
void battle_gfx_start_unit_fade_out(s32 runtime_id, s32 mode, s16 red, s16 green, s16 blue) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id);
    if (unit != 0) {
        battle_unit_move_to_head(runtime_id);
        unit->fade_out_phase = BATTLE_UNIT_FADE_OUT_FLASH;
        unit->shadow_graphic_trigger = 0;
        unit->sprite_display_flags = (unit->sprite_display_flags & ~BATTLE_SPRITE_DISPLAY_BLEND_MASK)
            | (BATTLE_SPRITE_DISPLAY_BLEND_ADD | BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT);
        battle_gfx_apply_unit_palette_modulation_to_units(MAP_COLOR_MODE_BASE, mode, runtime_id, red, green, blue);
        unit->float_bob_offset = 0;
        unit->depth_height_offset = 0;
    }
}
