#include "fft/battle.h"
#include "psx/types.h"

s32 battle_gfx_load_unit_graphics_by_battle_id(s32 battle_id, s32 flags) {
    battle_stats_t* unit;

    g_unit_graphics_load_count = 0;
    g_battle_gfx_state_words[0] = 0;
    g_battle_gfx_state_words[1] = 0;
    g_battle_gfx_state_words[2] = 0;
    g_battle_gfx_state_words[3] = 0;
    unit = battle_unit_get_stats_from_battle_id(battle_id);
    battle_unit_set_enemy_level_data_by_battle_id(battle_id);
    battle_gfx_append_unit_graphics_load_descriptor(unit->x, unit->position.bits.y,
        unit->position.bits.higher_elevation, unit->position.bits.facing, unit->spritesheet_id,
        unit->job_portrait_palette, 0xFF, unit, flags);
    return 1;
}
