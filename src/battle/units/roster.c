/* Find battle-stat records and manage unit availability, status display, enemy levels and war trophies. */

#include "fft/item.h"
#include "fft/map.h"
#include "fft/map_unit.h"
#include "fft/memory.h"
#include "fft/party.h"
#include "fft/reward.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "psx/types.h"

/* The battle stats in a unit slot, occupied or not; 0 for a battle_unit_id past the last
 * slot. */
battle_unit_t* battle_unit_get(u32 battle_unit_id) {
    if (battle_unit_id >= BATTLE_UNIT_SLOT_COUNT) {
        return 0;
    }
    return &g_battle_units[battle_unit_id];
}

/*
 * Classify a character identity for battle entry: the stats slot when
 * an active unit has it (-3 if it is crystallized or a treasure), -2/-3 for an
 * absent unit whose existence is UNIT_EXISTENCE_OFF_FIELD/DISABLED, -4
 * when a party roster entry has it, else -5; -1 for selector identities.
 *
 * The two status tests must be separate `if`s with their own `return -3`: an
 * `||` lets the delay-slot pass steal `li v0,-3` into the first branch.
 */
s32 battle_unit_find_id_by_character_identity(u32 character_identity) {
    battle_unit_t* unit;
    party_unit_t* party;
    s32 i;
    u8* status;
    u8 identity;

    if (character_identity >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        return UNIT_CHARACTER_SLOT_SELECTOR;
    }
    identity = character_identity;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_units[i];
        if (unit->active_battle_unit_id != UNIT_ACTIVE_BATTLE_UNIT_ID_NONE && unit->character_identity == identity) {
            status = unit->status_sets.current;
            if (status[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL)) {
                return UNIT_CHARACTER_SLOT_UNAVAILABLE;
            }
            if (status[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
                return UNIT_CHARACTER_SLOT_UNAVAILABLE;
            }
            return i;
        }
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_units[i];
        if (unit->active_battle_unit_id == UNIT_ACTIVE_BATTLE_UNIT_ID_NONE && unit->character_identity == identity
            && unit->existence != UNIT_EXISTENCE_NONE) {
            if (unit->existence == UNIT_EXISTENCE_OFF_FIELD) {
                return UNIT_CHARACTER_SLOT_OFF_FIELD;
            }
            if (unit->existence == UNIT_EXISTENCE_DISABLED) {
                return UNIT_CHARACTER_SLOT_UNAVAILABLE;
            }
        }
    }
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        party = party_unit_get(i);
        if (party->party_id != PARTY_ID_NONE && party->sprite_set == identity) {
            return UNIT_CHARACTER_SLOT_IN_PARTY;
        }
    }
    return UNIT_CHARACTER_SLOT_ABSENT;
}

/* Find the battle-stats record whose ENTD unit ID matches unit_id.
 *
 * Present units are searched first: the result code is the battle index, or -3
 * for a Crystal or Treasure unit. Absent units are then matched by existence
 * state: off field gives -2, disabled -3 and pending removal -6. Otherwise
 * returns 0 with -3. Both loops compare against (unit_id & 0xff): the (u8) cast
 * adds a loop insn and stops loop.c hoisting the constant 2 in the second loop.
 * The second loop's test of existence against active_battle_unit_id, just found to be
 * UNIT_ACTIVE_BATTLE_UNIT_ID_NONE, is a test against UNIT_EXISTENCE_NONE (both
 * 0xff). */
battle_unit_t* battle_unit_find_by_unit_id(s32 unit_id, s32* out_battle_unit_id) {
    battle_unit_t* unit;
    s32 i;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_units[i];
        if (unit->active_battle_unit_id != UNIT_ACTIVE_BATTLE_UNIT_ID_NONE && unit->unit_id == (unit_id & 0xff)) {
            if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
                || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                    & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
                *out_battle_unit_id = -3;
                return unit;
            }
            *out_battle_unit_id = i;
            return unit;
        }
    }

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_units[i];
        if (unit->active_battle_unit_id == UNIT_ACTIVE_BATTLE_UNIT_ID_NONE && unit->unit_id == (unit_id & 0xff)
            && unit->existence != unit->active_battle_unit_id) {
            if (unit->existence == UNIT_EXISTENCE_OFF_FIELD) {
                *out_battle_unit_id = -2;
                return unit;
            }
            if (unit->existence == UNIT_EXISTENCE_DISABLED) {
                *out_battle_unit_id = -3;
                return unit;
            }
            if (unit->existence == UNIT_EXISTENCE_PENDING_REMOVAL) {
                *out_battle_unit_id = -6;
                return unit;
            }
        }
    }

    *out_battle_unit_id = -3;
    return 0;
}

/* Count a unit of any team but blue into the enemy level statistics: its level
 * is added to g_reward_enemy_level_sum and raises g_reward_highest_enemy_level. */
void reward_count_enemy_level(battle_unit_t* unit) {
    u8 level;

    if (unit->team_flags & UNIT_TEAM_MASK) {
        level = unit->level;
        g_reward_enemy_level_sum += level;
        if (g_reward_highest_enemy_level < level) {
            g_reward_highest_enemy_level = level;
        }
    }
}

/* Collect the war trophies and bonus money of every present unit into
 * `result`, zero-filling the unused trophy slots.
 *
 * bonus_money is the sum of each present unit's bonus_money_modifier * 100.
 * level_sum_money is g_reward_enemy_level_sum * 100. level_bonus_money is
 * (highest enemy level - lowest level among the present party units) * 100, or
 * 0 when that is not positive or a player unit has fallen. */
void reward_find_war_trophies_and_bonus_money(reward_war_result_t* result) {
    s32 count = 0;
    s32 money = 0;
    u8 lowest = UNIT_LEVEL_MAX;
    s32 i;
    battle_unit_t* unit;
    u8 value;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_units[i];
        if (unit->existence != UNIT_EXISTENCE_NONE && unit->existence != UNIT_EXISTENCE_OFF_FIELD) {
            value = unit->war_trophy;
            if (value != ITEM_ID_NOTHING) {
                result->item_ids[count] = value;
                count++;
            }
            money += unit->bonus_money_modifier * 100;
            if (unit->formation_index != UNIT_FORMATION_INDEX_NONE) {
                value = unit->level;
                if (value < lowest) {
                    lowest = value;
                }
            }
        }
    }
    result->item_count = count;
    for (i = count; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        result->item_ids[i] = ITEM_ID_NOTHING;
    }
    result->bonus_money = money;
    result->level_sum_money = g_reward_enemy_level_sum * 100;
    if (g_reward_player_unit_fallen != 0 || lowest >= g_reward_highest_enemy_level) {
        result->level_bonus_money = 0;
    } else {
        result->level_bonus_money = (g_reward_highest_enemy_level - lowest) * 100;
    }
}

/* Strip a unit of every status: queue each current status's display flag off, then zero the
 * innate, immunity and current sets with the 16 status CTs that follow them (one 0x1F-byte clear
 * from status_sets) and the inflicted statuses. Used before a Morbol transformation. */
void status_clear_all(battle_unit_t* unit) {
    s32 i;
    s32 battle_unit_id = unit->battle_unit_id;

    for (i = 0; i < STATUS_COUNT; i++) {
        s32 byte_index = i / 8;
        s32 mask = 0x80 >> (i & 7);
        if (unit->status_sets.current[byte_index] & mask) {
            status_queue_display_flag_change(STATUS_HANDLER_INDEX(i), STATUS_DISPLAY_CHANGE_REMOVE, battle_unit_id);
        }
    }
    mem_zero(unit->status_sets.innate, 0x1F);
    mem_zero(unit->inflicted_status, STATUS_BYTE_COUNT);
}

/* Enable the renderer status flags for each of a unit's current statuses.
 * Returns -1 for an empty unit slot, otherwise 0. */
s32 status_init_display_flags(s32 battle_unit_id) {
    battle_unit_t* unit = &g_battle_units[battle_unit_id];
    s32 i;

    if (unit->active_battle_unit_id == UNIT_ACTIVE_BATTLE_UNIT_ID_NONE) {
        return -1;
    }
    for (i = 0; i < STATUS_COUNT; i++) {
        s32 byte_index = i / 8;
        s32 mask = 0x80 >> (i & 7);
        if (unit->status_sets.current[byte_index] & mask) {
            status_queue_display_flag_change(STATUS_HANDLER_INDEX(i), STATUS_DISPLAY_CHANGE_ADD, battle_unit_id);
        }
    }
    return 0;
}

/* Index of the unit's tile in g_map_tile_data: layer * 256 + y * map width
 * + x, with the layer in bit 15 of the packed position. */
s32 battle_unit_get_tile_index(const battle_unit_t* unit) {
    u8 y = unit->position.y;
    u16 elevation = unit->position.higher_elevation;
    u8 x = unit->x;
    return (elevation << 8) + (y * g_map_terrain_width) + x;
}

/* The battle stats of an occupied unit slot, or 0 for an empty slot or an id past
 * the last slot. */
battle_unit_t* battle_unit_get_if_present(u32 battle_unit_id) {
    battle_unit_t* unit = 0;
    if (battle_unit_id < BATTLE_UNIT_SLOT_COUNT) {
        unit = &g_battle_units[battle_unit_id];
        if (unit->active_battle_unit_id == UNIT_ACTIVE_BATTLE_UNIT_ID_NONE) {
            unit = 0;
        }
    }
    return unit;
}

/* The first unit whose has_turn is set (the unit taking its turn), or 0. */
battle_unit_t* battle_unit_find_with_turn(void) {
    s32 i = 0;
    battle_unit_t* unit = g_battle_units;
    for (; i < BATTLE_UNIT_SLOT_COUNT; i++, unit++) {
        if (unit->has_turn != 0) {
            return unit;
        }
    }
    return 0;
}
