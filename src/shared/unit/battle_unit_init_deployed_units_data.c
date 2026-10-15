#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Initialize deployed units from map coordinates and return a failure bitmask.
 *
 * Normal deployment fills slots 0x10..0x14; red team debug deployment fills
 * slots 0..4 and clears the remaining ENTD slots. The target reloads each row
 * from g_main_unit_deployed_coords at its distinct uses.
 */
s32 battle_unit_init_deployed_units_data(unit_deployment_coordinates_t* formation, s32 is_red_team_debug) {
    s32 failures = 0;
    s32 weight = 1;
    s32 i;
    s32 slot_step;
    s32 slot_start;
    s32 next_generated_unit_id;
    u8 palette;

    g_main_unit_deployed_coords = formation;
    if (is_red_team_debug) {
        next_generated_unit_id = 0xfe;
        slot_step = -1;
        slot_start = 0;
    } else {
        next_generated_unit_id = 0x78;
        slot_step = 1;
        slot_start = BATTLE_DEPLOYED_UNIT_SLOT_FIRST;
    }
    for (i = 0; i < 5; i++, weight *= 2) {
        s32 battle_id = i + slot_start;
        unit_stats_t* unit = &g_battle_unit_stats[battle_id];
        u8 sprite;
        u32 identity;

        unit->battle_id = battle_id;
        if (g_main_unit_deployed_coords[i].party_id != PARTY_ID_NONE) {
            unit->entd_slot = battle_id;
            unit->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
            if (main_unit_init_job_data(unit, g_main_unit_deployed_coords[i].party_id, 0) != 0) {
                main_system_handle_battle_load_exception(battle_id + 0x1f4);
            } else if (battle_unit_set_placement_and_validate(battle_id, &g_main_unit_deployed_coords[i]) != 0) {
                main_system_handle_battle_load_exception(battle_id + 0x190);
            } else {
                sprite = main_unit_calculate_palette_spritesheet(unit, &palette);
                identity = unit->character_identity;
                if ((identity & 0xff) == 0 || (identity & 0xff) >= 4) {
                    identity = next_generated_unit_id;
                    next_generated_unit_id += slot_step;
                }
                unit->unit_id = identity;
                if (is_red_team_debug) {
                    unit->team_flags = BATTLE_TEAM_RED | BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
                    unit->initial_team_flags = BATTLE_TEAM_RED | BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
                    unit->sprite_palette = 1;
                }
                if (g_main_unit_deployed_coords[i].flags.off_field) {
                    unit->existence = BATTLE_UNIT_EXISTENCE_OFF_FIELD;
                    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
                } else {
                    battle_gfx_append_unit_graphics_load_descriptor(unit->x, unit->position.y,
                        unit->position.higher_elevation, unit->position.facing, sprite, palette, 0xff, unit, 0);
                    main_unit_init_status_and_rewards(unit, 1);
                    unit->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
                }
                continue;
            }
            failures += weight;
        }
        unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
        unit->existence = BATTLE_UNIT_EXISTENCE_NONE;
    }
    if (is_red_team_debug) {
        for (i = 5; i < ENTD_UNITS_PER_ENCOUNTER; i++) {
            unit_stats_t* unit = &g_battle_unit_stats[i];
            unit->battle_id = i;
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_NONE;
        }
    }
    return failures;
}
