#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "psx/types.h"

/* Initialize deployed units from map coordinates and return a failure bitmask.
 *
 * Normal deployment fills slots 0x10..0x14; red team debug deployment fills
 * slots 0..4 and clears the remaining ENTD slots. The target reloads each row
 * from g_main_current_formation_entry at its distinct uses.
 */
s32 battle_unit_init_deployed_units_data(battle_deployed_coords_t* formation, s32 is_red_team_debug) {
    /* Pin: unpinned, GCC permutes $s4, $s6 and $s7 among the loop constants and counters. */
    register s32 failures __asm__("$23") = 0;
    /* Pin: unpinned, GCC swaps $s4 and $s5. */
    register s32 weight __asm__("$21") = 1;
    s32 i;
    s32 slot_step;
    s32 slot_start;
    s32 next_generated_unit_id;
    u8 palette;

    g_main_current_formation_entry = formation;
    if (is_red_team_debug) {
        next_generated_unit_id = 0xfe;
        slot_step = -1;
        slot_start = 0;
    } else {
        next_generated_unit_id = 0x78;
        slot_step = 1;
        slot_start = 0x10;
    }
    for (i = 0; i < 5; i++, weight *= 2) {
        s32 battle_id = i + slot_start;
        battle_stats_t* unit = &g_battle_unit_stats[battle_id];
        u8 sprite;
        s32 identity;

        unit->misc_unit_id = battle_id;
        if (g_main_current_formation_entry[i].party_id != 0xff) {
            unit->entd_slot = battle_id;
            unit->existence = 1;
            if (main_unit_init_job_data(unit, g_main_current_formation_entry[i].party_id, 0) != 0) {
                main_system_handle_battle_load_exception(battle_id + 0x1f4);
                /* Stops cross-jumping from merging this arm with the placement failure. */
                __asm__ volatile("" : : "r"(battle_id));
                failures += weight;
            } else if (battle_unit_set_placement_and_validate(battle_id, &g_main_current_formation_entry[i]) != 0) {
                main_system_handle_battle_load_exception(battle_id + 0x190);
                failures += weight;
            } else {
                sprite = main_unit_calculate_palette_spritesheet(unit, &palette);
                identity = unit->character_identity;
                if ((identity & 0xff) == 0 || (u32)(identity & 0xff) >= 4) {
                    identity = next_generated_unit_id;
                    next_generated_unit_id += slot_step;
                }
                unit->unit_id = identity;
                if (is_red_team_debug) {
                    unit->team_flags = 0x18;
                    unit->initial_team_flags = 0x18;
                    unit->sprite_palette = 1;
                }
                if (g_main_current_formation_entry[i].facing_elevation_flags & 0x10) {
                    unit->existence = 0;
                    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
                } else {
                    battle_gfx_append_unit_graphics_load_descriptor(unit->x, unit->position.bits.y,
                        unit->position.bits.higher_elevation, unit->position.bits.facing, sprite, palette, 0xff, unit,
                        0);
                    main_unit_init_status_and_rewards(unit, 1);
                    unit->existence = 1;
                }
                continue;
            }
        }
        unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
        unit->existence = 0xff;
    }
    if (is_red_team_debug) {
        for (i = 5; i < 16; i++) {
            battle_stats_t* unit = &g_battle_unit_stats[i];
            unit->misc_unit_id = i;
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = 0xff;
        }
    }
    return failures;
}
