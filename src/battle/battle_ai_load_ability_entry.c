#include "fft/battle.h"

typedef struct battle_ai_loader_charge_record {
    u8 ct;
    u8 field_01;
} battle_ai_loader_charge_record_t;
typedef char battle_ai_loader_charge_record_must_be_2[sizeof(battle_ai_loader_charge_record_t) == 2 ? 1 : -1];

extern battle_ai_loader_charge_record_t g_main_charge_ability_data[8];

/*
 * Load an ability-list entry into the considered AI ability.
 *
 * Decode its physical unit and specialize inventory, weapon and timing data.
 */
void battle_ai_load_ability_entry(battle_ai_ability_entry_t* entry) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 ability_id, unit_id, menu, status_id;
    battle_stats_t* unit;
    battle_ai_weapon_data_t* weapon;
    ability_secondary_data_t* secondary;
    s32 value;

    battle_ai_clear_words((s32*)&ai->considered_ability, sizeof(battle_ai_considered_ability_t));
    battle_ai_clear_words((s32*)&ai->current_action.calculator_type, 4);
    ai->considered_ability.skillset = entry->skillset_flags.bytes.skillset;
    ability_id = g_battle_ai_data_base.considered_ability.ability_id = entry->id.packed_id & 0x3ff;
    unit_id = (s16)entry->id.packed_id >> 10;
    unit = &g_battle_unit_stats[unit_id];
    menu = g_main_action_menu_types_by_skillset[entry->skillset_flags.bytes.skillset];
    status_id = 0;
    switch (menu) {
    case ACTION_MENU_TYPE_ATTACK:
    attack:
        weapon = &ai->unit_weapon_data[ai->unit_battle_ids[unit_id]];
        ai->considered_ability.parameters.bytes.range = weapon->bytes.range;
        ai->considered_ability.element = weapon->bytes.element;
    weapon_flags:
        ai->considered_ability.parameters.bytes.item_id = weapon->bytes.weapon_id;
        ai->considered_ability.ai_flags.bytes.flags_1 = weapon->bytes.flags_1;
        ai->considered_ability.ai_flags.bytes.flags_2 = weapon->bytes.flags_2;
        ai->considered_ability.ai_flags.bytes.flags_3 = weapon->bytes.flags_3;
        ai->considered_ability.ai_flags.bytes.flags_4 = entry->skillset_flags.bytes.usage_flags;
        if (menu == ACTION_MENU_TYPE_JUMP)
            ai->considered_ability.ai_flags.word = (ai->considered_ability.ai_flags.word | 2) & ~1;
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        status_id = g_main_item_secondary_data[ability_id - ABILITY_ID_ITEM_FIRST].status_infliction_id;
        if (unit->support_abilities[2] & BATTLE_SUPPORT_SET_3_THROW_ITEM)
            ai->considered_ability.parameters.bytes.range = unit->move;
        else
            ai->considered_ability.parameters.bytes.range = 1;
        ai->considered_ability.parameters.bytes.aoe = 0;
        ai->considered_ability.parameters.bytes.item_id
            = g_main_item_ability_item_ids[ability_id - ABILITY_ID_ITEM_FIRST];
        goto common;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        ai->considered_ability.parameters.bytes.item_id = ability_id - 0x26;
        goto common;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY: {
        s32 thrown_ability_id;
        if ((u32)(ability_id - 0x7a) < 3)
            thrown_ability_id = ABILITY_ID_THROW_SHURIKEN;
        else if ((u32)(ability_id - 0x7d) < 3)
            thrown_ability_id = ABILITY_ID_THROW_BALL;
        else
            goto ordinary_throw;
        ai->considered_ability.ability_id = thrown_ability_id;
        ai->considered_ability.parameters.bytes.item_id = ability_id;
        ai->considered_ability.element = g_main_item_weapon_data[ability_id].element;
        goto throw_range;
    ordinary_throw:
        ai->considered_ability.ability_id = ai->throw_ability_id;
        ai->considered_ability.parameters.bytes.item_id = ai->throw_weapon_id;
    throw_range:
        ai->considered_ability.parameters.bytes.range = unit->move;
        ability_id = ai->considered_ability.ability_id;
        goto common;
    }
    case ACTION_MENU_TYPE_JUMP:
        if (unit_id == ai->acting_unit_id)
            ai->considered_ability.ct
                = ((100 - ai->acting_unit->ct) / ai->acting_unit->attributes[UNIT_ATTRIBUTE_SPEED]) / 2;
        else {
            if (unit->ct > 100) {
                ai->considered_ability.ct = 0;
                value = unit->ct - 100;
            } else {
                ai->considered_ability.ct = (100 - unit->ct) / unit->attributes[UNIT_ATTRIBUTE_SPEED];
                value = unit->attributes[UNIT_ATTRIBUTE_SPEED]
                    - (100 - unit->ct) % unit->attributes[UNIT_ATTRIBUTE_SPEED];
            }
            ai->considered_ability.ct += ((100 - value) / unit->attributes[UNIT_ATTRIBUTE_SPEED]) / 2;
        }
        if (!ability_id)
            ai->considered_ability.parameters.bytes.range = 1;
        else
            ai->considered_ability.parameters.bytes.range
                = g_main_jump_ability_data[ability_id - ABILITY_ID_JUMP_FIRST].range;
        weapon = &ai->unit_weapon_data[ai->unit_battle_ids[unit_id]];
        goto weapon_flags;
    case ACTION_MENU_TYPE_CHARGE:
        ai->considered_ability.ct = g_main_charge_ability_data[ability_id - ABILITY_ID_CHARGE_FIRST].ct;
        goto attack;
    case ACTION_MENU_TYPE_ARITHMETICKS: {
        u32 status_cursor;
        s32 included = 0xff;
        value = 4;
        /* A direct status pointer folds the member offset into its base;
         * retain the field-derived base cursor for the exact five-byte fill. */
        status_cursor = (u32)ai + value;
        do {
            *(u8*)(status_cursor + ((u32)&ai->considered_ability.parameters.bytes.status_infliction - (u32)ai))
                = included;
            value--;
            status_cursor--;
        } while (value >= 0);
        ai->considered_ability.ai_flags.bytes.flags_1 = 0xfc;
        ai->considered_ability.ai_flags.bytes.flags_4 = entry->skillset_flags.bytes.usage_flags;
        ai->considered_ability.ai_flags.word |= BATTLE_AI_ABILITY_TARGET_MAP_TILES;
        break;
    }
    default:
    common:
        battle_ai_transfer_halfword_values(
            (u16*)&ai->considered_ability.ai_flags, g_main_ability_data[ability_id].ai_flags.halfwords, 4);
        if ((u32)(menu - ACTION_MENU_TYPE_ITEM_INVENTORY) < 2)
            ai->considered_ability.ai_flags.word |= BATTLE_AI_ABILITY_DIRECT_TRAJECTORY;
        ai->considered_ability.ai_flags.bytes.flags_4 = entry->skillset_flags.bytes.usage_flags;
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            s32 ct;
            secondary = &g_main_ability_range_data[ability_id];
            ct = secondary->ct;
            status_id = secondary->status_infliction_id;
            ai->considered_ability.ct = ct;
            ai->considered_ability.element = secondary->element;
            ai->considered_ability.mp_cost = secondary->mp_cost;
            if (unit->support_abilities[1] & BATTLE_SUPPORT_SET_2_HALF_MP)
                ai->considered_ability.mp_cost >>= 1;
            if (secondary->flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE) {
                ai->considered_ability.parameters.bytes.range
                    = ai->unit_weapon_data[ai->unit_battle_ids[unit_id]].bytes.range;
                ai->considered_ability.parameters.bytes.aoe = 0;
            } else {
                ai->considered_ability.parameters.bytes.range = secondary->range;
                ai->considered_ability.parameters.bytes.aoe = secondary->aoe;
            }
            if (secondary->flags_3 & ABILITY_SECONDARY_FLAG_3_PERSEVERE)
                goto copy_status;
        }
        /* Preserve the native support-byte masks, including their CT quirk. */
        if (unit->support_abilities[3] & BATTLE_SUPPORT_SET_4_MONSTER_SKILL)
            ai->considered_ability.ct >>= 1;
        else if (unit->support_abilities[3] & BATTLE_SUPPORT_SET_4_DEFEND)
            ai->considered_ability.ct = 0;
    copy_status:
        battle_ai_transfer_byte_values(ai->considered_ability.parameters.bytes.status_infliction,
            g_main_status_infliction_data[status_id].statuses, 5);
    }
}
