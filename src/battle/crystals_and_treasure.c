/* Crystal and treasure pickups, enemy level statistics, unit stat lookups and the unit action menu set. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/at_list.h"
#include "fft/battle_map.h"
#include "fft/command.h"
#include "fft/crystal.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/memory.h"
#include "fft/move.h"
#include "fft/party.h"
#include "fft/reward.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/status.h"
#include "fft/turn.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/stddef.h"
#include "psx/types.h"

/* Move-Find scratch is private to this unit. The first-byte flags alias retains
 * the target's independent address load; accessing every byte through the record
 * instead merges that load with the returned record address. */
extern move_found_item_t g_move_find_rare_item_result;
extern move_find_result_data_t g_move_find_tile_result;
extern u8 g_move_find_result_flags;

static s32 move_find_test_or_set_item_flag(s32 map_id, s32 tile_index, s32 save);
static command_menu_e menu_action_get_unit_menu_id(const unit_stats_t* unit);
static s32 skillset_load(s32 unit_id, u8 skillset, u8 menu);
static void status_cancel_charge(unit_stats_t* unit);
static u8 crystal_generate_treasure_item(unit_stats_t* unit);

/* 1 when the unit, or its rider when it is a mount, is charging an attack of
 * the Charge skillset; with cancel set, that unit's charge is also canceled
 * (status_cancel_charge). A move or a knockback cancels a Charge
 * through this test. */
s32 status_check_charging_charge(unit_stats_t* unit, s32 cancel) {
    u8 mount_info;

    for (;;) {
        if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)] & STATUS_BYTE_MASK(STATUS_ID_CHARGING))
            && unit->charged_ability_ct != 0xFF && unit->action_command.skillset_id == SKILLSET_ID_CHARGE) {
            if (cancel != 0) {
                status_cancel_charge(unit);
            }
            return 1;
        }
        mount_info = unit->mount_info;
        if (!(mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT)) {
            break;
        }
        unit = &g_unit_stats[mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK];
    }
    return 0;
}

/* Clears the unit's Charging, Jump, Defending and Performing statuses (action state none). */
static void status_cancel_charge(unit_stats_t* unit) {
    status_set_action_state(unit, STATUS_ACTION_STATE_NONE);
}

/* Pick the rare or common move-find item on the unit's tile and mark the tile found.
 *
 * The rare item is chosen with a (100 - Brave)% chance. */
move_found_item_t* move_find_determine_item(unit_stats_t* unit) {
    move_find_result_data_t* find_result
        = move_find_get_result(unit->x, unit->position.y, unit->position.higher_elevation);
    u8 item_id;

    if (random_roll_pass_fail(100, 100 - unit->brave) == 0) {
        item_id = find_result->rare_item_id;
    } else {
        item_id = find_result->common_item_id;
    }
    g_move_find_rare_item_result.item_id = item_id;
    move_find_test_or_set_item_flag(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), find_result->entry_index, 1);
    g_move_find_rare_item_result._unknown_00 = 0;
    return &g_move_find_rare_item_result;
}

/*
 * Fill the move-find result record for tile (x, y, layer): MOVE_FIND_RESULT_OCCUPIED
 * and the unit id when a unit stands there; then, when the tile is one of the
 * map's four move-find tiles, its index, MOVE_FIND_RESULT_ITEM and the rare and
 * common items while the item is still unfound, and MOVE_FIND_RESULT_TRAP with
 * the first trap its flags name unless the tile has no trap.
 *
 * The occupant lookup does not depend on the loop counter, so the retail code
 * repeats the same search up to 21 times. The final flag update must go
 * through a pointer to keep the target's register address.
 */
move_find_result_data_t* move_find_get_result(s32 x, u8 y, u8 layer) {
    move_find_entry_t* tile;
    s32 unit;
    s32 i;
    s32 trap = MOVE_FIND_TRAP_NONE;
    u8* result;

    g_move_find_result_flags = 0;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = unit_find_at_tile((u8)x, y, layer, UNIT_TILE_FILTER_TRANSPARENT);
        if (unit >= 0) {
            break;
        }
    }
    if (i != UNIT_SLOT_COUNT) {
        g_move_find_result_flags = MOVE_FIND_RESULT_OCCUPIED;
        g_move_find_tile_result.occupying_unit_id = unit;
    }
    for (i = 0; i < 4; i++) {
        tile = &g_move_find_current_map_data.entries[i];
        if (tile->x == (u8)x && tile->y == y && tile->elevation == layer) {
            break;
        }
    }
    if (i != 4) {
        g_move_find_tile_result.entry_index = i;
        if (!(tile->flags & BATTLE_MAP_MOVE_FIND_ENTRY_NO_ITEM)) {
            if (move_find_test_or_set_item_flag(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), i, 0) != 0) {
                g_move_find_result_flags |= MOVE_FIND_RESULT_ITEM;
                g_move_find_tile_result.rare_item_id = tile->rare_item_id;
                g_move_find_tile_result.common_item_id = tile->common_item_id;
            }
        }
        if (!(tile->flags & BATTLE_MAP_MOVE_FIND_ENTRY_NO_TRAP)) {
            if (tile->flags & BATTLE_MAP_MOVE_FIND_ENTRY_TRAP_DEGENERATOR) {
                trap = MOVE_FIND_TRAP_DEGENERATOR;
            } else if (tile->flags & BATTLE_MAP_MOVE_FIND_ENTRY_TRAP_DEATHTRAP) {
                trap = MOVE_FIND_TRAP_DEATHTRAP;
            } else if (tile->flags & BATTLE_MAP_MOVE_FIND_ENTRY_TRAP_SLEEPING_GAS) {
                trap = MOVE_FIND_TRAP_SLEEPING_GAS;
            } else if (tile->flags & BATTLE_MAP_MOVE_FIND_ENTRY_TRAP_STEEL_NEEDLE) {
                trap = MOVE_FIND_TRAP_STEEL_NEEDLE;
            }
            if (trap != MOVE_FIND_TRAP_NONE) {
                result = &g_move_find_result_flags;
                g_move_find_tile_result.trap_id = trap;
                *result |= MOVE_FIND_RESULT_TRAP;
            }
        }
    }
    return &g_move_find_tile_result;
}

/* Test (save == 0) or set the found flag of one of a map's four move-find item tiles.
 *
 * Each flag byte holds two maps, four tiles each, MSB first with the even map in
 * the high nibble; the test returns 1 while the item is still unfound. */
static s32 move_find_test_or_set_item_flag(s32 map_id, s32 tile_index, s32 save) {
    s32 byte_index = map_id / 2;
    s32 bit = 0x80 >> (((map_id & 1) * 4) + tile_index);

    if (save == 0) {
        return (g_item_location_flags[byte_index] & bit) == 0;
    }
    g_item_location_flags[byte_index] = bit | g_item_location_flags[byte_index];
    return 1;
}

/* Resolve what a moving unit picks up on its tile into g_crystal_pickup_result: NONE with no
 * crystal or treasure there; TREASURE and a generated item for a treasure; otherwise CRYSTAL, and
 * when neither unit is a monster, each action ability the crystal knows and the mover does not (in
 * the mover's unlocked jobs, and the special job only for the same character) is learned on a roll
 * against its learn rate, adding LEARNED when any is. */
crystal_pickup_result_t* crystal_build_pickup_result(unit_stats_t* mover) {
    u8 gained[UNIT_LEARNED_ABILITY_BYTES_PER_JOB];
    u8 known[UNIT_LEARNED_ABILITY_BYTES_PER_JOB];
    u8 offered[UNIT_LEARNED_ABILITY_BYTES_PER_JOB];
    s32 learned_count;
    s32 mover_identity;
    s32 crystal_identity;
    s32 mover_flags;
    s32 crystal_flags;
    s32 unlocked_jobs_mask;
    unit_stats_t* crystal;
    s32 crystal_index;
    s32 job_id;
    s32 job_index;
    s32 j;
    s32 byte_index;
    s32 mask;
    s32 skillset;
    s32 ability;
    u16 ability_id;

    crystal_index = unit_find_at_tile(mover->x, mover->position.y, mover->position.higher_elevation,
        UNIT_TILE_FILTER_CRYSTAL | UNIT_TILE_FILTER_TREASURE);
    if (crystal_index < 0) {
        g_crystal_pickup_result.result = CRYSTAL_PICKUP_NONE;
        return &g_crystal_pickup_result;
    }
    g_crystal_pickup_result.result = CRYSTAL_PICKUP_NOTHING;
    g_crystal_pickup_result.unit_index = crystal_index;
    crystal = &g_unit_stats[crystal_index];
    if (crystal->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
        g_crystal_pickup_result.result = CRYSTAL_PICKUP_TREASURE;
        g_crystal_pickup_result.treasure_item = crystal_generate_treasure_item(crystal);
    } else {
        /* The empty loops are measured residue: without them the code differs. */
        do {
            do {
                g_crystal_pickup_result.result = CRYSTAL_PICKUP_CRYSTAL;
            } while (0);
        } while (0);
        mover_flags = mover->unit_flags;
        crystal_flags = crystal->unit_flags;
        if (!(mover_flags & UNIT_FLAG_MONSTER) && !(crystal_flags & UNIT_FLAG_MONSTER)) {
            learned_count = 0;
            unlocked_jobs_mask = (mover->career.unlocked_jobs[0] << 16) + (mover->career.unlocked_jobs[1] << 8)
                + mover->career.unlocked_jobs[2];
            mover_identity = mover->character_identity;
            crystal_identity = crystal->character_identity;
            mem_zero(g_crystal_pickup_result.learned_abilities, sizeof(g_crystal_pickup_result.learned_abilities));
            for (job_index = UNIT_CAREER_JOB_INDEX_BASE; job_index < UNIT_LEARNED_ABILITY_JOB_COUNT; job_index++) {
                for (j = 0; j < UNIT_LEARNED_ABILITY_BYTES_PER_JOB; j++) {
                    g_crystal_pickup_result.learned[job_index][j] = 0;
                    gained[j] = 0;
                    known[j] = mover->career.learned_abilities[job_index][j];
                    offered[j] = crystal->career.learned_abilities[job_index][j];
                }
                job_id = job_index + JOB_ID_GENERIC_FIRST;
                if (job_index == UNIT_CAREER_JOB_INDEX_BASE) {
                    if (mover_identity != crystal_identity) {
                        continue;
                    }
                    if (mover_identity < CHARACTER_IDENTITY_SELECTOR_FIRST) {
                        job_id = mover_identity;
                    }
                } else if (!(unlocked_jobs_mask & (UNIT_UNLOCKED_JOB_SQUIRE >> job_index))) {
                    continue;
                }
                if (job_index == UNIT_CAREER_JOB_INDEX_BARD && (mover_flags & UNIT_FLAG_FEMALE)) {
                    continue;
                }
                if (job_index == UNIT_CAREER_JOB_INDEX_DANCER && (mover_flags & UNIT_FLAG_MALE)) {
                    continue;
                }
                skillset = g_job_data_pointer[job_id].skillset;
                /* Learned bits run MSB first in the job's three bytes; the
                 * result's learned_abilities set is indexed by ability id, LSB first. */
                for (j = 0; j < SKILLSET_ABILITY_LIST_COUNT; j++) {
                    byte_index = j / 8;
                    mask = 0x80 >> (j - byte_index * 8);
                    if (!(known[byte_index] & mask) && (offered[byte_index] & mask)) {
                        ability = ability_get_id_from_skillset(skillset, j);
                        ability_id = ability;
                        if (ability_id != 0 && random_roll_pass_fail(100, g_ability_data[ability_id].learn_rate) == 0) {
                            gained[byte_index] = mask | gained[byte_index];
                            learned_count++;
                            if (ability_id < ABILITY_ID_COUNT) {
                                g_crystal_pickup_result.learned_abilities[ability_id >> 3]
                                    = (1 << (ability & 7)) | g_crystal_pickup_result.learned_abilities[ability_id >> 3];
                            }
                        }
                    }
                }
                for (j = 0; j < UNIT_LEARNED_ABILITY_BYTES_PER_JOB; j++) {
                    g_crystal_pickup_result.learned[job_index][j] = gained[j];
                }
            }
            if (learned_count != 0) {
                g_crystal_pickup_result.result |= CRYSTAL_PICKUP_LEARNED;
            }
        }
    }
    return &g_crystal_pickup_result;
}

/* Fill a unit's action result for picking up a crystal or treasure
 * (CRYSTAL_* flags).
 *
 * The result starts as a canceled miss. TAKE_TREASURE adds
 * g_crystal_treasure_item_id to the inventory and returns that call's
 * result, ignoring the other flags. RESTORE_HP_MP turns the result into a hit
 * that heals the missing HP and MP. LEARN_ABILITIES ORs
 * g_crystal_learnable_abilities into the unit's learned-ability bits. These
 * return 0. */
s32 crystal_apply_choice(unit_stats_t* unit, s32 flags) {
    s32 i;
    u8* dst;
    u8* src;
    u8* learned;
    u8* crystal_bits;
    u8* end;

    action_clear_current_data(&unit->action);
    unit->action.miss_type = ACTION_MISS_TYPE_CANCELED;
    unit->action.hit = 0;
    if (flags & CRYSTAL_TAKE_TREASURE) {
        return item_give(unit, g_crystal_treasure_item_id);
    }
    if (flags & CRYSTAL_RESTORE_HP_MP) {
        unit->action.hit = 1;
        unit->action.attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_MP_HEALING;
        unit->action.miss_type = ACTION_MISS_TYPE_HIT;
        unit->action.hp_healing = unit->max_hp - unit->hp;
        unit->action.mp_healing = unit->max_mp - unit->mp;
    }
    if (flags & CRYSTAL_LEARN_ABILITIES) {
        /* OR the crystal's ability bits into learned_abilities, three bytes
         * per job for 19 jobs; walked as raw bytes to keep the biased pointers. */
        i = 0;
        src = g_crystal_learnable_abilities;
        dst = (u8*)unit;
        do {
            learned = dst + offsetof(unit_stats_t, career.learned_abilities);
            crystal_bits = src;
            end = dst + offsetof(unit_stats_t, career.learned_abilities) + UNIT_LEARNED_ABILITY_BYTES_PER_JOB;
            do {
                *learned |= *crystal_bits;
                learned++;
                crystal_bits++;
                /* The signed compare is byte-required (slt, not sltu). */
            } while ((s32)learned < (s32)end);
            src += 3;
            i++;
            dst += 3;
        } while (i < UNIT_LEARNED_ABILITY_JOB_COUNT);
    }
    return 0;
}

/* Item a fallen unit leaves as treasure: one of its equipped items at random,
 * or with nothing equipped one of the thirteen consumables other than Elixir.
 * Decided only when executing; in any other state the function returns without
 * a value. */
static u8 crystal_generate_treasure_item(unit_stats_t* unit) {
    u8 candidates[UNIT_EQUIPMENT_SLOT_COUNT];
    s32 count;
    s32 i;
    u32 item_id;

    if (g_action_state != ACTION_STATE_EXECUTE) {
        return;
    }

    count = 0;
    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        candidates[count] = unit->equipment[i];
        item_id = candidates[count];
        if (item_id >= ITEM_ID_END) {
            continue;
        }
        if (item_id == 0) {
            continue;
        }
        count++;
    }

    if (count == 0) {
        /* 0 to 12 less 16 wraps in the u8 to the consumable ids from Potion;
         * skipping Elixir reaches the thirteenth. */
        candidates[0] = ((rand() * 13) / 0x8000) - (ITEM_ID_COUNT - ITEM_ID_CONSUMABLE_FIRST);
        if (candidates[0] >= ITEM_ID_ELIXIR) {
            candidates[0] = candidates[0] + 1;
        }
        count = 1;
    }

    i = (rand() * count) / 0x8000;
    return candidates[i];
}

/* The battle stats in a unit slot, occupied or not; 0 for a battle_id past the last
 * slot. */
unit_stats_t* unit_get_stats(u32 battle_id) {
    if (battle_id >= UNIT_SLOT_COUNT) {
        return 0;
    }
    return &g_unit_stats[battle_id];
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
s32 unit_find_slot_by_identity(u32 character_identity) {
    unit_stats_t* unit;
    party_unit_t* party;
    s32 i;
    u8* status;
    u8 identity;

    if (character_identity >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        return UNIT_CHARACTER_SLOT_SELECTOR;
    }
    identity = character_identity;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE && unit->character_identity == identity) {
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
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE && unit->character_identity == identity
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
        party = party_get_unit(i);
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
 * The second loop's test of existence against active_battle_id, just found to be
 * UNIT_ACTIVE_BATTLE_ID_NONE, is a test against UNIT_EXISTENCE_NONE (both
 * 0xff). */
unit_stats_t* unit_entd_find_stats(s32 unit_id, s32* out_battle_id) {
    unit_stats_t* unit;
    s32 i;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE && unit->unit_id == (unit_id & 0xff)) {
            if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
                || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                    & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
                *out_battle_id = -3;
                return unit;
            }
            *out_battle_id = i;
            return unit;
        }
    }

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE && unit->unit_id == (unit_id & 0xff)
            && unit->existence != unit->active_battle_id) {
            if (unit->existence == UNIT_EXISTENCE_OFF_FIELD) {
                *out_battle_id = -2;
                return unit;
            }
            if (unit->existence == UNIT_EXISTENCE_DISABLED) {
                *out_battle_id = -3;
                return unit;
            }
            if (unit->existence == UNIT_EXISTENCE_PENDING_REMOVAL) {
                *out_battle_id = -6;
                return unit;
            }
        }
    }

    *out_battle_id = -3;
    return 0;
}

/* Count a unit of any team but blue into the enemy level statistics: its level
 * is added to g_reward_enemy_level_sum and raises g_reward_highest_enemy_level. */
void reward_count_enemy_level(unit_stats_t* unit) {
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
    unit_stats_t* unit;
    u8 value;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
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
    for (i = count; i < UNIT_SLOT_COUNT; i++) {
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
void status_clear_all(unit_stats_t* unit) {
    s32 i;
    s32 battle_id = unit->battle_id;

    for (i = 0; i < STATUS_COUNT; i++) {
        s32 byte_index = i / 8;
        s32 mask = 0x80 >> (i & 7);
        if (unit->status_sets.current[byte_index] & mask) {
            status_queue_display_flag_change(STATUS_HANDLER_INDEX(i), 0, battle_id);
        }
    }
    mem_zero(unit->status_sets.innate, 0x1F);
    mem_zero(unit->inflicted_status, STATUS_BYTE_COUNT);
}

/* Enable the renderer status flags for each of a unit's current statuses.
 * Returns -1 for an empty unit slot, otherwise 0. */
s32 status_init_display_flags(s32 unit_id) {
    unit_stats_t* unit = &g_unit_stats[unit_id];
    s32 i;

    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return -1;
    }
    for (i = 0; i < STATUS_COUNT; i++) {
        s32 byte_index = i / 8;
        s32 mask = 0x80 >> (i & 7);
        if (unit->status_sets.current[byte_index] & mask) {
            status_queue_display_flag_change(STATUS_HANDLER_INDEX(i), 1, unit_id);
        }
    }
    return 0;
}

/* Index of the unit's tile in g_battle_map_tile_data: layer * 256 + y * map width
 * + x, with the layer in bit 15 of the packed position. */
s32 battle_map_tile_calculate_unit_index(const unit_stats_t* unit) {
    u8 y = unit->position.y;
    u16 elevation = unit->position.higher_elevation;
    u8 x = unit->x;
    return (elevation << 8) + (y * g_battle_map_terrain_width) + x;
}

/* The battle stats of an occupied unit slot, or 0 for an empty slot or an id past
 * the last slot. */
unit_stats_t* unit_get_present_stats(u32 battle_id) {
    unit_stats_t* unit = 0;
    if (battle_id < UNIT_SLOT_COUNT) {
        unit = &g_unit_stats[battle_id];
        if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
            unit = 0;
        }
    }
    return unit;
}

/* The first unit whose has_turn is set (the unit taking its turn), or 0. */
unit_stats_t* unit_find_active(void) {
    s32 i = 0;
    unit_stats_t* unit = g_unit_stats;
    for (; i < UNIT_SLOT_COUNT; i++, unit++) {
        if (unit->has_turn != 0) {
            return unit;
        }
    }
    return 0;
}

/* The idle action menu for a unit id: menu_action_get_unit_menu_id, with
 * the four TURN menus replaced by their RIDER_TURN variants for a rider. Returns
 * MENU_COMMAND_ID_NONE for an empty slot. */
command_menu_e menu_action_get_menu_id(s32 battle_id) {
    unit_stats_t* unit = unit_get_present_stats(battle_id);
    s32 menu;

    if (unit == 0) {
        return MENU_COMMAND_ID_NONE;
    }
    menu = menu_action_get_unit_menu_id(unit);
    if ((unit->mount_info & UNIT_MOUNT_INFO_FLAG_RIDER) != 0) {
        switch (menu) {
        case MENU_COMMAND_ID_TURN:
            menu = MENU_COMMAND_ID_RIDER_TURN;
            break;
        case MENU_COMMAND_ID_TURN_MOVED:
            menu = MENU_COMMAND_ID_RIDER_TURN_MOVED;
            break;
        case MENU_COMMAND_ID_TURN_ACTED:
            menu = MENU_COMMAND_ID_RIDER_TURN_ACTED;
            break;
        case MENU_COMMAND_ID_TURN_MOVED_ACTED:
            menu = MENU_COMMAND_ID_RIDER_TURN_MOVED_ACTED;
            break;
        default:
            return menu;
        }
    }
    return menu;
}

/* Pick a unit's idle action menu (command_menu_e), before the rider variants.
 *
 * While the unit taking its turn is not a manually controlled player unit, every
 * unit gets an inspect menu: PLAYER_UNIT_INSPECT or OTHER_UNIT_INSPECT. Otherwise
 * the unit with the turn gets one of the four TURN menus, by whether its move is
 * gone (moved, or Don't Move) and its action is gone (acted, or any action
 * restriction), and any other unit gets PLAYER_UNIT or OTHER_UNIT.
 *
 */
static command_menu_e menu_action_get_unit_menu_id(const unit_stats_t* unit) {
    unit_stats_t* active;
    s32 inspect_only = 0;
    s32 player_controlled;
    s32 player_flag;
    s32 dont_move;
    s32 restriction;
    u8 moved;
    u8 acted;

    active = unit_find_active();
    if (active != 0) {
        if ((active->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) == 0
            || active->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
            inspect_only = 1;
        }
    }
    player_flag = unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED;
    player_controlled = player_flag != 0;
    if (inspect_only != 0) {
        if (player_controlled != 0) {
            return MENU_COMMAND_ID_PLAYER_UNIT_INSPECT;
        }
        return MENU_COMMAND_ID_OTHER_UNIT_INSPECT;
    }
    dont_move = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)];
    restriction = unit_get_action_restriction(unit);
    moved = unit->movement_taken | (dont_move & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE));
    acted = unit->action_taken | restriction;
    if (unit->has_turn != 0) {
        if (moved == 0 && acted == 0) {
            return MENU_COMMAND_ID_TURN;
        }
        if (moved != 0 && acted == 0) {
            return MENU_COMMAND_ID_TURN_MOVED;
        }
        if (moved == 0 && acted != 0) {
            return MENU_COMMAND_ID_TURN_ACTED;
        }
        return MENU_COMMAND_ID_TURN_MOVED_ACTED;
    }
    if (player_controlled != 0) {
        return MENU_COMMAND_ID_PLAYER_UNIT;
    }
    return MENU_COMMAND_ID_OTHER_UNIT;
}

/* Return the unit's unit_action_restriction_e.
 *
 * CANNOT_ACT for Don't Act or a ridden mount. DEEP_WATER on a tile of depth 2 or
 * more, unless the unit has Float, Chicken or Frog, one of the Move in Water,
 * Walk on Water or Float movement abilities, or is a rider; the last test
 * computes DEEP_WATER (2) as the rider test shifted left once. */
unit_action_restriction_e unit_get_action_restriction(const unit_stats_t* unit) {
    s32 depth;

    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_ACT)] & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))
        != 0) {
        return UNIT_ACTION_RESTRICTION_CANNOT_ACT;
    }
    if ((unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) != 0) {
        return UNIT_ACTION_RESTRICTION_CANNOT_ACT;
    }
    depth = g_battle_map_tile_data[battle_map_tile_calculate_unit_index(unit)].depth_half_height.depth;
    if (depth < 2) {
        return UNIT_ACTION_RESTRICTION_NONE;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & (STATUS_BYTE_MASK(STATUS_ID_FLOAT) | STATUS_BYTE_MASK(STATUS_ID_CHICKEN)
                | STATUS_BYTE_MASK(STATUS_ID_FROG)))
        != 0) {
        return UNIT_ACTION_RESTRICTION_NONE;
    }
    if ((unit->ability_flags.movement[2]
            & (UNIT_MOVEMENT_SET_3_MOVE_IN_WATER | UNIT_MOVEMENT_SET_3_WALK_ON_WATER | UNIT_MOVEMENT_SET_3_FLOAT))
        != 0) {
        return UNIT_ACTION_RESTRICTION_NONE;
    }
    return ((unit->mount_info & UNIT_MOUNT_INFO_FLAG_RIDER) == 0) << 1;
}

/* Build a unit's 0xff-terminated action-menu skillset list and load each
 * entry's abilities.
 *
 * `menu_types` receives each entry's low-nibble menu type and `empty` is set
 * to 1 where skillset_load returned 0. A monster skillset in either
 * slot yields a single unloaded primary entry and returns 1; a unit that cannot
 * act or is carrying a rider gets an empty list. Returns -1 when the unit does
 * not exist, otherwise 0.
 *
 * The walk rewinds `entries` from `list` rather than reading `list` in the
 * loop: that keeps `entries` as CSE's canonical copy, so the list stores use
 * the incremented pointer instead of offsets from the saved base. `supports`
 * is declared first so it wins the allocation tie with `primary`. */
s32 skillset_build_menu_list(s32 unit_id, u8* entries, u8* empty, u8* menu_types) {
    unit_stats_t* unit = unit_get_present_stats(unit_id);
    u8* list;
    u8 supports;
    u8 primary;
    u8 secondary;
    s32 i;
    u8 id;
    u8 menu_type;

    if (unit == 0) {
        return -1;
    }
    primary = unit->primary_skillset;
    secondary = unit->secondary_skillset;
    list = entries;
    if (primary >= SKILLSET_ID_MONSTER_FIRST || secondary >= SKILLSET_ID_MONSTER_FIRST) {
        entries[0] = primary;
        empty[0] = 0;
        menu_types[0] = g_command_menu_types_by_skillset[primary];
        entries[1] = SKILLSET_ID_NONE;
        return 1;
    }
    supports = unit->ability_flags.support[3];
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        supports = 0;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_ACT)] & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))
        || (unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT)) {
        *entries = SKILLSET_ID_NONE;
        return 0;
    }
    *entries++ = SKILLSET_ID_ATTACK;
    if (primary != 0 && primary != SKILLSET_ID_MIMIC) {
        *entries++ = primary;
    }
    if (secondary != 0) {
        *entries++ = secondary;
    }
    if (supports & UNIT_SUPPORT_SET_4_DEFEND) {
        *entries++ = SKILLSET_ID_DEFEND;
    }
    if (supports & UNIT_SUPPORT_SET_4_EQUIP_CHANGE) {
        *entries++ = SKILLSET_ID_EQUIP_CHANGE;
    }
    *entries = SKILLSET_ID_NONE;
    entries = list;
    for (i = 0; i < 6; i++) {
        id = entries[i];
        if (id == SKILLSET_ID_NONE) {
            break;
        }
        menu_type = g_command_menu_types_by_skillset[id] & 0xf;
        menu_types[i] = menu_type;
        if (skillset_load(unit_id, id, menu_type) == 0) {
            empty[i] = 1;
        } else {
            empty[i] = 0;
        }
    }
    return 0;
}

/* Load the unit's usable abilities for an action menu type into scratch lists.
 *
 * A frog only keeps the default, blank, monster and attack menus. Returns the
 * menu loader's result, or whether the menu type is below 15 for menus without
 * a loader. */
static s32 skillset_load(s32 unit_id, u8 skillset, u8 menu) {
    u8 items[0x58];
    s16 abilities[0x44];
    u8 mp_costs[0x18];
    u8 ability_ct[0x18];
    u8 ability_flags[0x18];
    u8 turns[0x18];
    s32 result;

    if ((g_unit_stats[unit_id].status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
            & STATUS_BYTE_MASK(STATUS_ID_FROG))
        && menu != ACTION_MENU_TYPE_DEFAULT && menu != ACTION_MENU_TYPE_MONSTER && menu != ACTION_MENU_TYPE_ATTACK
        && menu != ACTION_MENU_TYPE_BLANK_05) {
        return 0;
    }
    switch (menu) {
    case ACTION_MENU_TYPE_DEFAULT:
        return skillset_get_unit_ability_data(
            unit_id, skillset, abilities, mp_costs, ability_ct, 0, ability_flags, turns);
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        return skillset_collect_item_abilities(unit_id, skillset, items);
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        return skillset_collect_throwable_items(unit_id, skillset, items);
    case ACTION_MENU_TYPE_ARITHMETICKS:
        result = skillset_math_load_attributes(unit_id, skillset, abilities);
        if (result != 0) {
            result = skillset_math_load_multiples(unit_id, skillset, abilities);
            if (result != 0) {
                result = skillset_math_collect_calculator_abilities(unit_id, skillset, abilities);
            }
        }
        return result;
    case ACTION_MENU_TYPE_ELEMENTS:
        return skillset_get_elemental_ability(unit_id, skillset);
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        return skillset_collect_draw_out_katanas(unit_id, skillset, items);
    case ACTION_MENU_TYPE_CHARGE:
        return skillset_load_charge_ability_ids(unit_id, skillset, abilities);
    default:
        return menu < 15;
    }
}

/* Count the AT-list entries that resolve before an action by battle_id
 * charging for ct; returns 0xff when no such position exists within 40.
 *
 * Keys hold the tick in their high byte. On an equal key the action resolves
 * after only another unit's charged action with a lower battle id. Between the
 * charge pass and the next tick (PICK_UNIT_AFTER_CHARGE) a nonzero charge lands
 * one tick later. */
u8 at_list_count_turns_to_resolve(s32 battle_id, s32 ct, const at_entry_t* entries) {
    s32 count;

    if (g_turn_clock_resume_state == TURN_CLOCK_STATE_PICK_UNIT_AFTER_CHARGE && ct != 0) {
        ct += 1;
    }
    ct <<= 8;
    for (count = 0; count < AT_LIST_CAPACITY; count++, entries++) {
        s32 entry_unit = entries->unit_code;
        s32 is_action = entry_unit & AT_ACTION_ENTRY;
        s32 entry_ct;
        entry_unit &= AT_UNIT_ID_MASK;
        if (entry_unit == AT_END_OF_LIST)
            return 0xFF;
        entry_ct = entries->sort_key;
        if (entry_ct >= ct) {
            if (ct != entry_ct)
                return count;
            if (battle_id == entry_unit && is_action != 0)
                return count;
            if (entry_unit >= battle_id)
                return count;
            if (is_action == 0)
                return count;
        }
    }
    return 0xFF;
}
