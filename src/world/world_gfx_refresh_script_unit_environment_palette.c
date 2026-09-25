#include "fft/battle.h"
#include "fft/world.h"

/* Resolve the script's signed unit ID and refresh its environment palette
 * unless resolution returns the missing-unit sentinel. */
void world_gfx_refresh_script_unit_environment_palette(const u8* parameters) {
    s32 misc_id = world_get_misc_id(world_script_load_halfword(parameters));

    if (misc_id != EVENT_MISC_ID_NONE) {
        battle_gfx_apply_weather_time_tile_palette_mod_by_misc_id(misc_id);
    }
}
