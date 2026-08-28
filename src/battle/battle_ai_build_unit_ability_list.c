#include "fft/battle_ai.h"
#include "fft/data.h"

/* Byte-oriented view of the same four-byte ability entry. */
typedef struct battle_ai_ability_entry_byte_view {
    u8 ability_id_low;
    u8 unit_and_id_high;
    u8 skillset;
    u8 usage_flags;
} battle_ai_ability_entry_byte_view_t;
typedef char battle_ai_ability_entry_byte_view_must_be_4[sizeof(battle_ai_ability_entry_byte_view_t) == 4 ? 1 : -1];

/*
 * Build a unit's ability list and weapon-attack properties.
 *
 * Non-monster skillsets include Attack and a secondary skillset. Frog Attack
 * is appended for every unit; a terminator is written only if space remains.
 */
void battle_ai_build_unit_ability_list(s32 unit_id) {
    battle_stats_t* unit = &g_battle_unit_stats[unit_id];
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_ai_weapon_data_t* weapon;
    s32 list_id = ai->unit_battle_ids[unit_id];
    s32 count = 0;
    s32 weapon_flags;

    if (unit->primary_skillset < SKILLSET_ID_MONSTER_FIRST) {
        ai->ability_lists[list_id][0].skillset_flags.bytes.skillset = SKILLSET_ID_ATTACK;
        ai->ability_lists[list_id][0].id.packed_id = unit_id << 10;
        weapon = &ai->unit_weapon_data[list_id];
        weapon->words[0] = 0;
        weapon->words[1] = 0;
        weapon->bytes.flags_1 |= 0x80;
        weapon->bytes.flags_2 |= 2;
        weapon->bytes.flags_3 |= 1;
        weapon_flags = 0;
        if (!(unit->unit_flags & UNIT_FLAG_MONSTER)) {
            battle_ai_store_weapon_attack_data(
                weapon, unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON], &weapon_flags);
            battle_ai_store_weapon_attack_data(
                weapon, unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON], &weapon_flags);
        }
        switch (weapon_flags) {
        case 0:
            weapon->bytes.range = 1;
            /* Fall through to normal enemy targeting. */
        case 1:
            weapon->bytes.flags_1 |= ABILITY_SECONDARY_FLAG_1_AUTO;
            break;
        case 2:
            weapon->bytes.flags_1 |= ABILITY_SECONDARY_FLAG_1_AUTO;
            weapon->bytes.flags_2 |= 0x20;
            break;
        case 4:
            weapon->bytes.flags_1 |= ABILITY_SECONDARY_FLAG_1_CANNOT_TARGET_SELF;
            weapon->bytes.flags_2 |= 0x20;
            break;
        }
        count++;
    }
    count = battle_ai_add_usable_skillset_abilities(unit_id, unit->primary_skillset, count);
    if (unit->primary_skillset < SKILLSET_ID_MONSTER_FIRST) {
        count = battle_ai_add_usable_skillset_abilities(unit_id, unit->secondary_skillset, count);
    }
    ai->ability_lists[list_id][count].skillset_flags.bytes.skillset = SKILLSET_ID_FROG_ATTACK;
    ai->ability_lists[list_id][count].id.packed_id = (unit_id << 10) + ABILITY_ID_FROG_ATTACK;
    /* The byte view preserves the target's separate final-store address. */
    ((battle_ai_ability_entry_byte_view_t*)&ai->ability_lists[list_id][count])->usage_flags
        = BATTLE_AI_ABILITY_ENTRY_USABLE | BATTLE_AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN;
    count++;
    if (count < 34) {
        ai->ability_lists[list_id][count].skillset_flags.bytes.skillset = SKILLSET_ID_NONE;
    }
}
