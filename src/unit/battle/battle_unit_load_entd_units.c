#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/types.h"

/* Loads the 16 ENTD units of the encounter into the battle unit slots.
 *
 * Each present unit is initialized. A unit that would need a new spritesheet
 * beyond the sheets left for the ENTD (nine less the deployment unit limit) is
 * taken off the field, and the units placed on the field are capped at that
 * number + 7; the exception reports use the error bases 600, 300, 100 and 200
 * plus the slot. `status` holds both the initialization result and the
 * graphics descriptor's not-deployed flag: sharing the one variable is what
 * gives the flag enough uses to win s1 ahead of battle_id. */
void battle_unit_load_entd_units(entd_encounter_t* entd_data, s32 mode) {
    s32 battle_id;
    s32 enemy_count;
    s32 free_spritesheets;
    s32 enemy_limit;
    s32 duplicate_message_shown = 0;
    s32 limit_message_shown = 0;
    s32 status;
    u8 next_unused_unit_id = 0xfe;
    u8 palette_id;
    u8 spritesheet_id;
    s32 unit_id;
    u8 presence;
    unit_stats_t* unit;
    entd_unit_t* entd;
    entd_unit_t* entd_list = entd_data->units;

    g_battle_player_unit_fallen = 0;
    g_battle_enemy_level_sum = 0;
    g_battle_highest_enemy_level = 0;
    main_party_calculate_highest_level();
    free_spritesheets = script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT);
    free_spritesheets = (free_spritesheets < 10) ? 9 - free_spritesheets : 0;
    enemy_limit = free_spritesheets + 7;
    enemy_count = 0;
    battle_unit_add_unique_entd_spritesheet(0xff, 0, 0);
    for (battle_id = 0, entd = entd_list; battle_id < ENTD_UNITS_PER_ENCOUNTER; entd++, battle_id++) {
        unit = &g_battle_unit_stats[battle_id];
        unit->battle_id = battle_id;
        g_main_entd_current_unit = entd;
        if (entd->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_NONE;
            continue;
        }

        unit->entd_slot = battle_id;
        unit->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
        status = main_unit_init_for_battle(unit, g_main_entd_current_unit, 0, 0);
        if (status != 0) {
            main_system_handle_battle_load_exception(battle_id + 600);
        }
        spritesheet_id = main_unit_calculate_palette_spritesheet(unit, &palette_id);
        main_unit_init_status_and_rewards(unit, 0);
        /* s32 so the fallback id is zero-extended once here and passed on
         * unmasked, like the target. */
        unit_id = g_main_entd_current_unit->unit_id;
        if (unit_id == 0xff) {
            unit_id = next_unused_unit_id--;
        }
        unit->unit_id = unit_id;

        presence = unit->team_flags & BATTLE_TEAM_PRESENCE_MASK;
        if (presence == 0 || (presence == BATTLE_TEAM_FLAG_RANDOMLY_PRESENT && (rand() & 1))) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_OFF_FIELD;
            main_unit_init_position_and_rewards(unit, g_main_entd_current_unit);
            continue;
        }

        main_unit_init_position_and_rewards(unit, g_main_entd_current_unit);
        if (battle_unit_validate_placement(battle_id) != 0) {
            main_system_handle_battle_load_exception(battle_id + 300);
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_NONE;
            continue;
        }
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            continue;
        }
        if (battle_unit_add_unique_entd_spritesheet(battle_id, spritesheet_id, free_spritesheets) != 0) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_OFF_FIELD;
            if (duplicate_message_shown != 0) {
                continue;
            }
            main_system_handle_battle_load_exception(battle_id + 100);
            duplicate_message_shown = 1;
            continue;
        }

        if (presence != BATTLE_TEAM_PRESENCE_MASK && enemy_count >= enemy_limit) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_NONE;
            if (limit_message_shown != 0) {
                continue;
            }
            main_system_handle_battle_load_exception(battle_id + 200);
            limit_message_shown = 1;
            enemy_count++;
            continue;
        }
        if (presence == BATTLE_TEAM_PRESENCE_MASK) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = BATTLE_UNIT_EXISTENCE_OFF_FIELD;
            status = 1;
        } else {
            status = 0;
            battle_unit_set_enemy_level_data(unit);
        }
        battle_gfx_append_unit_graphics_load_descriptor(unit->x, unit->position.y, unit->position.higher_elevation,
            unit->position.facing, spritesheet_id, palette_id, unit_id, unit, status);
        if (presence != BATTLE_TEAM_PRESENCE_MASK) {
            enemy_count++;
        }
    }
}
