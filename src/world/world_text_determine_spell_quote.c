#include "fft/battle.h"
#include "fft/world.h"

/* Leading bytes of an Ability Data 1 record (8 bytes each at 0x8005ebf0). */
typedef struct world_text_spell_quote_ability_data {
    u16 jp_cost;     /* 0x00 */
    u8 learn_chance; /* 0x02 */
    u8 flags;        /* 0x03; 0x40 is tested before the no-quote path */
} world_text_spell_quote_ability_data_t;

struct battle_ai_command_action;

/* Chooses whether an ability announcement shows a spell quote.
 *
 * Unknown skillsets (not in the list and not the unit's flagged primary/secondary)
 * get quote command 5 unless the ability repeats the last quote
 * (g_world_spell_quote_last_ability_id), the unit is a Calculator or Mime, the ability is >= 0x170 or
 * lacks the spell-quote flag, status blocks it, options disable it, or
 * EVENT_SCRIPT_VAR_RANDOM_VALUE is 4 or more. Everything else uses command 7. */
void world_text_determine_spell_quote(world_unit_command_action_t* action, s32 unit_id, s32 enabled) {
    world_text_spell_quote_ability_data_t* ability;
    ability_secondary_data_t* secondary;
    battle_stats_t* unit;
    s32 known;

    main_ability_calculate_pointers_and_type(action->ability_id & ABILITY_ID_MASK, (u8**)&ability, (u8**)&secondary);
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    if (action->skillset != 0) {
        known = world_list_contains_value(action->skillset);
    } else {
        known = 1;
    }
    if (action->skillset == unit->primary_skillset
        && (unit->position.bits.spell_quote_skillsets & BATTLE_UNIT_SPELL_QUOTE_PRIMARY_SKILLSET)) {
        known = 1;
    }
    if (action->skillset == unit->secondary_skillset
        && (unit->position.bits.spell_quote_skillsets & BATTLE_UNIT_SPELL_QUOTE_SECONDARY_SKILLSET)) {
        known = 1;
    }
    if (known == 0 && g_world_spell_quote_last_ability_id != (action->ability_id & ABILITY_ID_MASK)
        && unit->job_id != JOB_ID_CALCULATOR && unit->job_id != JOB_ID_MIME
        && action->ability_id < ABILITY_ID_ITEM_FIRST && (secondary->flags_3 & ABILITY_SECONDARY_FLAG_3_SPELL_QUOTE)
        && !(unit->status_sets.current[2]
            & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TRANSPARENT) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG)))
        && g_main_game_options.fields.ability_names == GAME_OPTION_ON
        && (world_script_get_variable(EVENT_SCRIPT_VAR_RANDOM_VALUE) & 0xF) < 4) {
        g_world_spell_quote_last_ability_id = action->ability_id & ABILITY_ID_MASK;
        world_menu_dispatch_system_function(5, 0, unit_id, 0, enabled, (struct battle_ai_command_action*)action);
        return;
    }
    if (action->skillset != 0) {
        known = world_list_contains_value(action->skillset);
    } else {
        known = 0;
    }
    if ((known == 0 && (ability->flags & ABILITY_DATA_FLAG_SPELL_QUOTE)
            && g_main_game_options.fields.ability_names == GAME_OPTION_ON)
        || action->skillset == SKILLSET_ID_ITEM || action->skillset == SKILLSET_ID_THROW) {
        world_menu_dispatch_system_function(7, 0, unit_id, 0, enabled, (struct battle_ai_command_action*)action);
    } else {
        world_menu_dispatch_system_function(7, 0, unit_id, 0, 0, (struct battle_ai_command_action*)action);
    }
}
