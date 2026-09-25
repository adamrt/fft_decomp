#include "fft/battle.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Battle Stats 0x005 presence bits copied from the ENTD entry; both set
 * means the unit is loaded (graphics and all) but not deployed. */
#define ENTD_PRESENCE_RANDOM 0x40
#define ENTD_PRESENCE_MASK   0xc0

/* Loads the 16 ENTD units of the encounter into the battle unit slots.
 *
 * Each present unit is initialised, deduplicated against the party limit and
 * capped at party limit + 7 enemies; the exception reports use the error
 * bases 600, 300, 100 and 200 plus the slot. `status` holds both the
 * initialisation result and the graphics descriptor's not-deployed flag:
 * sharing the one variable is what gives the flag enough uses to win s1 ahead
 * of battle_id. */
void battle_load_entd_units(entd_encounter_t* entd_data, s32 mode) {
    s32 battle_id;
    s32 enemy_count;
    s32 party_limit;
    s32 enemy_limit;
    s32 duplicate_message_shown = 0;
    s32 limit_message_shown = 0;
    s32 status;
    u8 next_unused_unit_id = 0xfe;
    u8 palette_id;
    u8 portrait_id;
    s32 unit_id;
    u8 presence;
    battle_stats_t* unit;
    entd_unit_t* entd;
    entd_unit_t* entd_list = entd_data->units;

    g_battle_player_unit_fallen = 0;
    g_enemy_level_sum = 0;
    g_highest_enemy_level = 0;
    main_party_calculate_highest_level();
    party_limit = battle_script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT);
    party_limit = (party_limit < 10) ? 9 - party_limit : 0;
    enemy_limit = party_limit + 7;
    enemy_count = 0;
    battle_ai_add_unique_value_to_list(0xff, 0, 0);
    for (battle_id = 0, entd = entd_list; battle_id < ENTD_UNITS_PER_ENCOUNTER; entd++, battle_id++) {
        unit = &g_battle_unit_stats[battle_id];
        unit->misc_unit_id = battle_id;
        g_current_entd_unit = entd;
        if (entd->sprite_set == 0) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = 0xff;
            continue;
        }

        unit->entd_slot = battle_id;
        unit->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
        status = main_unit_init_for_battle(unit, g_current_entd_unit, 0, 0);
        if (status != 0) {
            main_system_handle_battle_load_exception(battle_id + 600);
        }
        portrait_id = main_unit_calculate_palette_spritesheet(unit, &palette_id);
        main_unit_init_status_and_rewards(unit, 0);
        /* s32 so the fallback id is zero-extended once here and passed on
         * unmasked, like the target. */
        unit_id = g_current_entd_unit->unit_id;
        if (unit_id == 0xff) {
            unit_id = next_unused_unit_id--;
        }
        unit->unit_id = unit_id;

        presence = unit->team_flags & ENTD_PRESENCE_MASK;
        if (presence == 0 || (presence == ENTD_PRESENCE_RANDOM && (rand() & 1))) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = 0;
            main_unit_init_position_and_rewards(unit, g_current_entd_unit);
            continue;
        }

        main_unit_init_position_and_rewards(unit, g_current_entd_unit);
        if (battle_unit_validate_placement(battle_id) != 0) {
            main_system_handle_battle_load_exception(battle_id + 300);
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = 0xff;
            continue;
        }
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            continue;
        }
        if (battle_ai_add_unique_value_to_list(battle_id, portrait_id, party_limit) != 0) {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = 0;
            if (duplicate_message_shown != 0) {
                continue;
            }
            main_system_handle_battle_load_exception(battle_id + 100);
            duplicate_message_shown = 1;
            continue;
        }

        if (presence != ENTD_PRESENCE_MASK) {
            if (enemy_count >= enemy_limit) {
                unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
                unit->existence = 0xff;
                if (limit_message_shown != 0) {
                    continue;
                }
                main_system_handle_battle_load_exception(battle_id + 200);
                limit_message_shown = 1;
                enemy_count++;
                continue;
            }
        } else {
            unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
            unit->existence = 0;
            status = 1;
            goto append_graphics;
        }
        status = 0;
        battle_unit_set_enemy_level_data(unit);

    append_graphics:
        battle_gfx_append_unit_graphics_load_descriptor(unit->x, (u8)unit->position.raw, unit->position.raw >> 15,
            (unit->position.raw >> 8) & 0xf, portrait_id, palette_id, unit_id, unit, status);
        if (presence != ENTD_PRESENCE_MASK) {
            enemy_count++;
        }
    }
}
