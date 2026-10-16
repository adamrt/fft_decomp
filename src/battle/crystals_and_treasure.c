/* Crystal and treasure generation, enemy level data, unit stat lookups and the unit action menu set. */

#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/main.h"
#include "fft/main_item.h"
#include "fft/main_unit.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/unit.h"
#include "psx/libc.h"
#include "psx/types.h"

static s32 battle_map_calculate_move_find_item_flag(s32 map_id, s32 tile_index, s32 save);
static battle_menu_id_e battle_menu_get_unit_action_menu_id(const unit_stats_t* unit);
static s32 battle_menu_load_skillset(s32 unit_id, u8 skillset, u8 menu);
static void battle_status_disable_acting(unit_stats_t* unit);
static u8 battle_unit_generate_treasure(unit_stats_t* unit);

/* 1 when the unit, or its rider when it is a mount, is charging an attack of
 * the Charge skillset; with do_disable, battle_status_disable_acting is called
 * on that unit. */
s32 battle_status_remove_charging_ability_ct(unit_stats_t* unit, s32 do_disable) {
    u8 mount_info;

    for (;;) {
        if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)] & STATUS_BYTE_MASK(STATUS_ID_CHARGING))
            && unit->charged_ability_ct != 0xFF && unit->action_command.skillset_id == SKILLSET_ID_CHARGE) {
            if (do_disable != 0) {
                battle_status_disable_acting(unit);
            }
            return 1;
        }
        mount_info = unit->mount_info;
        if (!(mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
            break;
        }
        unit = &g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
    }
    return 0;
}

/* Clears the unit's Charging, Jump, Defending and Performing statuses (action state 0). */
static void battle_status_disable_acting(unit_stats_t* unit) {
    main_status_set_action_state(unit, 0);
}

/* Pick the rare or common move-find item on the unit's tile and mark the tile found.
 *
 * The rare item is chosen with a (100 - Brave)% chance; the tile is the
 * unit's x and position (y in the low byte, the layer in bit 15). */
move_found_item_t* battle_map_determine_rare_common_item(unit_stats_t* unit) {
    move_find_result_data_t* find_result
        = battle_map_get_move_find_result(unit->x, unit->position.y, unit->position.higher_elevation);
    u8 item_id;

    if (main_util_roll_pass_fail(0x64, 0x64 - unit->brave) == 0) {
        item_id = find_result->rare_item_id;
    } else {
        item_id = find_result->common_item_id;
    }
    g_battle_map_rare_item_result.item_id = item_id;
    battle_map_calculate_move_find_item_flag(
        script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), find_result->entry_index, 1);
    g_battle_map_rare_item_result._unknown_00 = 0;
    return &g_battle_map_rare_item_result;
}

/*
 * Fill the move-find result record for tile (x, y, layer): flag 4 and the
 * unit id when a unit stands there, then, for a matching move-find tile, its
 * index, flag 1 and the rare/common items when the item is still unfound,
 * and flag 2 with the trap number (0 degenerator, 1 deathtrap, 2 sleeping gas,
 * 3 steel needle) unless the trap is disabled. The final flag
 * update must go through a pointer to keep the target's register address.
 */
move_find_result_data_t* battle_map_get_move_find_result(s32 x, u8 y, u8 layer) {
    map_move_find_item_entry_t* tile;
    s32 unit;
    s32 i;
    s32 trap = BATTLE_TRAP_ID_NONE;
    u8* result;

    g_battle_move_find_result_flags = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = battle_unit_find_at_tile((u8)x, y, layer, BATTLE_UNIT_TILE_FILTER_TRANSPARENT);
        if (unit >= 0) {
            break;
        }
    }
    if (i != BATTLE_UNIT_SLOT_COUNT) {
        g_battle_move_find_result_flags = MOVE_FIND_RESULT_OCCUPIED;
        g_battle_move_find_occupying_unit_id = unit;
    }
    for (i = 0; i < 4; i++) {
        tile = &g_battle_current_map_move_find_item_data.entries[i];
        if (tile->x == (u8)x && tile->y == y && tile->elevation == layer) {
            break;
        }
    }
    if (i != 4) {
        g_battle_move_find_entry_index = i;
        if (!(tile->flags & MAP_MOVE_FIND_ENTRY_NO_ITEM)) {
            if (battle_map_calculate_move_find_item_flag(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), i, 0)
                != 0) {
                g_battle_move_find_result_flags |= MOVE_FIND_RESULT_ITEM;
                g_battle_move_find_rare_item_id = tile->rare_item_id;
                g_battle_move_find_common_item_id = tile->common_item_id;
            }
        }
        if (!(tile->flags & MAP_MOVE_FIND_ENTRY_NO_TRAP)) {
            if (tile->flags & MAP_MOVE_FIND_ENTRY_TRAP_DEGENERATOR) {
                trap = BATTLE_TRAP_ID_DEGENERATOR;
            } else if (tile->flags & MAP_MOVE_FIND_ENTRY_TRAP_DEATHTRAP) {
                trap = BATTLE_TRAP_ID_DEATHTRAP;
            } else if (tile->flags & MAP_MOVE_FIND_ENTRY_TRAP_SLEEPING_GAS) {
                trap = BATTLE_TRAP_ID_SLEEPING_GAS;
            } else if (tile->flags & MAP_MOVE_FIND_ENTRY_TRAP_STEEL_NEEDLE) {
                trap = BATTLE_TRAP_ID_STEEL_NEEDLE;
            }
            if (trap != BATTLE_TRAP_ID_NONE) {
                result = &g_battle_move_find_result_flags;
                g_battle_move_find_trap_id = trap;
                *result |= MOVE_FIND_RESULT_TRAP;
            }
        }
    }
    return &g_battle_move_find_tile_result;
}

/* Test (save == 0) or set the found flag of one of a map's four move-find item tiles.
 *
 * Each flag byte holds two maps, four tiles each; the test returns 1 while the item is
 * still unfound. */
static s32 battle_map_calculate_move_find_item_flag(s32 map_id, s32 tile_index, s32 save) {
    s32 byte_index = map_id / 2;
    s32 bit = 0x80 >> (((map_id & 1) * 4) + tile_index);

    if (save == 0) {
        return (g_main_item_location_flags[byte_index] & bit) == 0;
    }
    g_main_item_location_flags[byte_index] = bit | g_main_item_location_flags[byte_index];
    return 1;
}

/* Resolve what a moving unit picks up on its tile into g_main_crystal_pickup_result: -1 with no
 * crystal or treasure there; 4 and a generated item for a treasure; otherwise 2 for a crystal, and
 * when neither unit is a monster, each action ability the crystal knows and the mover does not (in
 * the mover's unlocked jobs, and the special job only for the same character) is learned on a roll
 * against its learn rate, setting bit 0 when any is. */
crystal_pickup_result_t* battle_unit_generate_crystal_or_treasure(unit_stats_t* mover) {
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

    crystal_index = battle_unit_find_at_tile(mover->x, mover->position.y, mover->position.higher_elevation,
        BATTLE_UNIT_TILE_FILTER_CRYSTAL | BATTLE_UNIT_TILE_FILTER_TREASURE);
    if (crystal_index < 0) {
        g_main_crystal_pickup_result.result = -1;
        return &g_main_crystal_pickup_result;
    }
    g_main_crystal_pickup_result.result = 0;
    g_main_crystal_pickup_result.unit_index = crystal_index;
    crystal = &g_battle_unit_stats[crystal_index];
    if (crystal->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
        g_main_crystal_pickup_result.result = 4;
        g_main_crystal_pickup_result.treasure_item = battle_unit_generate_treasure(crystal);
    } else {
        do {
            do {
                g_main_crystal_pickup_result.result = 2;
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
            main_mem_zero(g_main_crystal_pickup_result.learned_abilities, 0x40);
            for (job_index = UNIT_CAREER_JOB_INDEX_BASE; job_index < UNIT_LEARNED_ABILITY_JOB_COUNT; job_index++) {
                for (j = 0; j < UNIT_LEARNED_ABILITY_BYTES_PER_JOB; j++) {
                    g_main_crystal_pickup_result.learned[job_index][j] = 0;
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
                skillset = g_main_job_data_pointer[job_id].skillset;
                for (j = 0; j < 24; j++) {
                    byte_index = j / 8;
                    mask = 0x80 >> (j - byte_index * 8);
                    if (!(known[byte_index] & mask) && (offered[byte_index] & mask)) {
                        ability = main_ability_get_id_from_skillset(skillset, j);
                        ability_id = ability;
                        if (ability_id != 0
                            && main_util_roll_pass_fail(100, g_main_ability_data[ability_id].learn_rate) == 0) {
                            gained[byte_index] = mask | gained[byte_index];
                            learned_count++;
                            if (ability_id < ABILITY_ID_COUNT) {
                                g_main_crystal_pickup_result.learned_abilities[ability_id >> 3] = (1 << (ability & 7))
                                    | g_main_crystal_pickup_result.learned_abilities[ability_id >> 3];
                            }
                        }
                    }
                }
                for (j = 0; j < UNIT_LEARNED_ABILITY_BYTES_PER_JOB; j++) {
                    g_main_crystal_pickup_result.learned[job_index][j] = gained[j];
                }
            }
            if (learned_count != 0) {
                g_main_crystal_pickup_result.result |= 1;
            }
        }
    }
    return &g_main_crystal_pickup_result;
}

/* Fills a unit's action result for picking up a crystal or treasure
 * (BATTLE_CRYSTAL_* flags).
 *
 * The result starts as a canceled miss. TAKE_TREASURE adds
 * g_main_crystal_treasure_item_id to the inventory and returns that call's
 * result, ignoring the other flags. RESTORE_HP_MP turns the result into a hit
 * that heals the missing HP and MP. LEARN_ABILITIES ORs
 * g_main_crystal_learnable_abilities into the unit's learned-ability bits. These
 * return 0. */
s32 battle_unit_learn_from_crystal(unit_stats_t* unit, s32 flags) {
    s32 i;
    u8* dst;
    u8* src;
    u8* learned;
    u8* crystal_bits;
    u8* end;

    battle_action_clear_current_data(&unit->action);
    unit->action.miss_type = ACTION_MISS_TYPE_CANCELED;
    unit->action.hit = 0;
    if (flags & BATTLE_CRYSTAL_TAKE_TREASURE) {
        return battle_action_increment_item_quantity_for_steal_break(unit, g_main_crystal_treasure_item_id);
    }
    if (flags & BATTLE_CRYSTAL_RESTORE_HP_MP) {
        unit->action.hit = 1;
        unit->action.attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_MP_HEALING;
        unit->action.miss_type = ACTION_MISS_TYPE_HIT;
        unit->action.hp_healing = unit->max_hp - unit->hp;
        unit->action.mp_healing = unit->max_mp - unit->mp;
    }
    if (flags & BATTLE_CRYSTAL_LEARN_ABILITIES) {
        /* OR the crystal's ability bits into learned_abilities, three bytes
         * per job for 19 jobs; walked as raw bytes to keep the biased pointers. */
        i = 0;
        src = g_main_crystal_learnable_abilities;
        dst = (u8*)unit;
        do {
            learned = dst + 0x99;
            crystal_bits = src;
            end = dst + 0x9C;
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
static u8 battle_unit_generate_treasure(unit_stats_t* unit) {
    u8 candidates[UNIT_EQUIPMENT_SLOT_COUNT];
    s32 count;
    s32 i;
    u32 item_id;

    if (g_battle_action_state != ACTION_STATE_EXECUTE) {
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
        candidates[0] = ((rand() * 13) / 0x8000) - (ITEM_ID_COUNT - ITEM_ID_CONSUMABLE_FIRST);
        if (candidates[0] >= ITEM_ID_ELIXIR) {
            candidates[0] = candidates[0] + 1;
        }
        count = 1;
    }

    i = (rand() * count) / 0x8000;
    return candidates[i];
}

/* The battle stats in a unit slot, occupied or not; 0 for an battle_id past the last
 * slot. */
unit_stats_t* battle_unit_get_stats_from_battle_id(u32 battle_id) {
    if (battle_id >= BATTLE_UNIT_SLOT_COUNT) {
        return 0;
    }
    return &g_battle_unit_stats[battle_id];
}

/*
 * Classify a character identity for battle entry: the stats slot when
 * an active unit has it (-3 if it is crystallized or a treasure), -2/-3 for an
 * absent unit whose existence is BATTLE_UNIT_EXISTENCE_OFF_FIELD/DISABLED, -4
 * when a party roster entry has it, else -5; -1 for selector identities.
 *
 * The two status tests must be separate `if`s with their own `return -3`: an
 * `||` lets the delay-slot pass steal `li v0,-3` into the first branch.
 */
s32 battle_unit_find_slot_by_character_identity(u32 character_identity) {
    unit_stats_t* unit;
    party_unit_t* party;
    s32 i;
    u8* status;
    u8 identity;

    if (character_identity >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        return BATTLE_CHARACTER_SLOT_SELECTOR;
    }
    identity = character_identity;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && unit->character_identity == identity) {
            status = unit->status_sets.current;
            if (status[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL)) {
                return BATTLE_CHARACTER_SLOT_UNAVAILABLE;
            }
            if (status[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
                return BATTLE_CHARACTER_SLOT_UNAVAILABLE;
            }
            return i;
        }
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE && unit->character_identity == identity
            && unit->existence != BATTLE_UNIT_EXISTENCE_NONE) {
            if (unit->existence == BATTLE_UNIT_EXISTENCE_OFF_FIELD) {
                return BATTLE_CHARACTER_SLOT_OFF_FIELD;
            }
            if (unit->existence == BATTLE_UNIT_EXISTENCE_DISABLED) {
                return BATTLE_CHARACTER_SLOT_UNAVAILABLE;
            }
        }
    }
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        party = main_party_get_unit(i);
        if (party->party_id != PARTY_ID_NONE && party->sprite_set == identity) {
            return BATTLE_CHARACTER_SLOT_IN_PARTY;
        }
    }
    return BATTLE_CHARACTER_SLOT_ABSENT;
}

/* Find the battle-stats record whose ENTD unit ID matches unit_id.
 *
 * Present units (entd_slot != 0xff) are searched first: the result code is the
 * battle index, or -3 for a Crystal or Treasure unit. Absent units are then
 * matched by existence state: 0 gives -2, disabled -3 and pending removal -6.
 * Otherwise returns 0 with -3. Both loops compare against (unit_id & 0xff):
 * the (u8) cast adds a loop insn and stops loop.c hoisting the constant 2 in
 * the second loop. */
unit_stats_t* battle_unit_find_stats_by_entd_unit_id(s32 unit_id, s32* out_battle_id) {
    unit_stats_t* unit;
    s32 i;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && unit->unit_id == (unit_id & 0xff)) {
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

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE && unit->unit_id == (unit_id & 0xff)
            && unit->existence != unit->entd_slot) {
            if (unit->existence == BATTLE_UNIT_EXISTENCE_OFF_FIELD) {
                *out_battle_id = -2;
                return unit;
            }
            if (unit->existence == BATTLE_UNIT_EXISTENCE_DISABLED) {
                *out_battle_id = -3;
                return unit;
            }
            if (unit->existence == BATTLE_UNIT_EXISTENCE_PENDING_REMOVAL) {
                *out_battle_id = -6;
                return unit;
            }
        }
    }

    *out_battle_id = -3;
    return 0;
}

/* Counts a unit of any team but blue into the enemy level statistics: its level
 * is added to g_battle_enemy_level_sum and raises g_battle_highest_enemy_level. */
void battle_unit_set_enemy_level_data(unit_stats_t* unit) {
    u8 lvl;
    if (unit->team_flags & BATTLE_TEAM_MASK) {
        lvl = unit->level;
        g_battle_enemy_level_sum = g_battle_enemy_level_sum + lvl;
        if (g_battle_highest_enemy_level < lvl) {
            g_battle_highest_enemy_level = lvl;
        }
    }
}

/* Collect the war trophies and bonus money of every present unit into
 * `result`, zero-filling the unused trophy slots.
 *
 * bonus_money is the sum of each present unit's bonus_money_modifier * 100.
 * level_sum_money is g_battle_enemy_level_sum * 100. level_bonus_money is
 * (highest enemy level - lowest level among the present party units) * 100, or
 * 0 when that is not positive or a player unit has fallen. */
void battle_unit_find_war_trophies_and_bonus_money(battle_war_result_t* result) {
    s32 count = 0;
    s32 money = 0;
    u8 lowest = UNIT_LEVEL_MAX;
    s32 i;
    unit_stats_t* unit;
    u8 value;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->existence != BATTLE_UNIT_EXISTENCE_NONE && unit->existence != BATTLE_UNIT_EXISTENCE_OFF_FIELD) {
            value = unit->war_trophy;
            if (value != ITEM_ID_NOTHING) {
                result->item_ids[count] = value;
                count++;
            }
            money += unit->bonus_money_modifier * 100;
            if (unit->formation_index != BATTLE_FORMATION_INDEX_NONE) {
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
    result->level_sum_money = g_battle_enemy_level_sum * 100;
    if (g_battle_player_unit_fallen != 0 || lowest >= g_battle_highest_enemy_level) {
        result->level_bonus_money = 0;
    } else {
        result->level_bonus_money = (g_battle_highest_enemy_level - lowest) * 100;
    }
}

/* Strip a unit of every status: queue each current status's display flag off, then zero the
 * innate, immunity and current sets with the 16 status CTs that follow them (one 0x1F-byte clear
 * from status_sets) and the inflicted statuses. Used before a Morbol transformation. */
void battle_status_clear_all(unit_stats_t* unit) {
    s32 i;
    s32 battle_id = unit->battle_id;

    for (i = 0; i < STATUS_COUNT; i++) {
        s32 byte_index = i / 8;
        s32 mask = 0x80 >> (i & 7);
        if (unit->status_sets.current[byte_index] & mask) {
            battle_status_queue_runtime_graphics_flag_change(STATUS_HANDLER_INDEX(i), 0, battle_id);
        }
    }
    main_mem_zero(unit->status_sets.innate, 0x1F);
    main_mem_zero(unit->inflicted_status, STATUS_BYTE_COUNT);
}

/* Enable the renderer status flags for each of a unit's current statuses.
 * Returns -1 for an empty unit slot, otherwise 0. */
s32 battle_status_init_special_flag_enabling(s32 unit_id) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    s32 i;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    for (i = 0; i < STATUS_COUNT; i++) {
        s32 byte_index = i / 8;
        s32 mask = 0x80 >> (i & 7);
        if (unit->status_sets.current[byte_index] & mask) {
            battle_status_queue_runtime_graphics_flag_change(STATUS_HANDLER_INDEX(i), 1, unit_id);
        }
    }
    return 0;
}

/* Index of the unit's tile in g_battle_map_tile_data: layer * 256 + y * map width
 * + x, with the layer in bit 15 of the packed position. */
s32 battle_map_calculate_location(const unit_stats_t* unit) {
    u8 y = unit->position.y;
    u16 elevation = unit->position.higher_elevation;
    u8 x = unit->x;
    return (elevation << 8) + (y * g_battle_map_max_x) + x;
}

/* The battle stats of an occupied unit slot, or 0 for an empty slot or an id past
 * the last slot. */
unit_stats_t* battle_unit_get_existing_pointer(u32 battle_id) {
    unit_stats_t* unit = 0;
    if (battle_id < BATTLE_UNIT_SLOT_COUNT) {
        unit = &g_battle_unit_stats[battle_id];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            unit = 0;
        }
    }
    return unit;
}

/* The first unit whose has_turn is set (the unit taking its turn), or 0. */
unit_stats_t* battle_unit_find_active_data_pointer(void) {
    s32 i = 0;
    unit_stats_t* unit = g_battle_unit_stats;
    for (; i < BATTLE_UNIT_SLOT_COUNT; i++, unit++) {
        if (unit->has_turn != 0) {
            return unit;
        }
    }
    return 0;
}

/* The idle action menu for a unit id: battle_menu_get_unit_action_menu_id, with
 * the four TURN menus replaced by their RIDER_TURN variants for a rider. Returns
 * BATTLE_MENU_ID_NONE for an empty slot. */
battle_menu_id_e battle_menu_get_id_based_on_mount_moveable_actable(s32 battle_id) {
    unit_stats_t* unit = battle_unit_get_existing_pointer(battle_id);
    s32 menu;

    if (unit == 0) {
        return BATTLE_MENU_ID_NONE;
    }
    menu = battle_menu_get_unit_action_menu_id(unit);
    if ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) != 0) {
        switch (menu) {
        case BATTLE_MENU_ID_TURN:
            menu = BATTLE_MENU_ID_RIDER_TURN;
            break;
        case BATTLE_MENU_ID_TURN_MOVED:
            menu = BATTLE_MENU_ID_RIDER_TURN_MOVED;
            break;
        case BATTLE_MENU_ID_TURN_ACTED:
            menu = BATTLE_MENU_ID_RIDER_TURN_ACTED;
            break;
        case BATTLE_MENU_ID_TURN_MOVED_ACTED:
            menu = BATTLE_MENU_ID_RIDER_TURN_MOVED_ACTED;
            break;
        default:
            return menu;
        }
    }
    return menu;
}

/* Pick a unit's idle action menu (battle_menu_id_e), before the rider variants.
 *
 * While the unit taking its turn is not a manually controlled player unit, every
 * unit gets an inspect menu: PLAYER_UNIT_INSPECT or OTHER_UNIT_INSPECT. Otherwise
 * the unit with the turn gets one of the four TURN menus, by whether its move is
 * gone (moved, or Don't Move) and its action is gone (acted, or any action
 * restriction), and any other unit gets PLAYER_UNIT or OTHER_UNIT.
 *
 * The local can_evade holds the action restriction. */
static battle_menu_id_e battle_menu_get_unit_action_menu_id(const unit_stats_t* unit) {
    unit_stats_t* act;
    s32 flag = 0;
    s32 player_controlled;
    s32 player_flag;
    s32 dont_move;
    s32 can_evade;
    u8 moved;
    u8 acted;

    act = battle_unit_find_active_data_pointer();
    if (act != 0) {
        if ((act->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) == 0
            || act->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
            flag = 1;
        }
    }
    player_flag = unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
    player_controlled = player_flag != 0;
    if (flag != 0) {
        if (player_controlled != 0) {
            return BATTLE_MENU_ID_PLAYER_UNIT_INSPECT;
        }
        return BATTLE_MENU_ID_OTHER_UNIT_INSPECT;
    }
    dont_move = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)];
    can_evade = battle_unit_get_action_restriction(unit);
    moved = unit->movement_taken | (dont_move & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE));
    acted = unit->action_taken | can_evade;
    if (unit->has_turn != 0) {
        if (moved == 0 && acted == 0) {
            return BATTLE_MENU_ID_TURN;
        }
        if (moved != 0 && acted == 0) {
            return BATTLE_MENU_ID_TURN_MOVED;
        }
        if (moved == 0 && acted != 0) {
            return BATTLE_MENU_ID_TURN_ACTED;
        }
        return BATTLE_MENU_ID_TURN_MOVED_ACTED;
    }
    if (player_controlled != 0) {
        return BATTLE_MENU_ID_PLAYER_UNIT;
    }
    return BATTLE_MENU_ID_OTHER_UNIT;
}

/* Returns the unit's battle_unit_action_restriction_e.
 *
 * CANNOT_ACT for Don't Act or a ridden mount. DEEP_WATER on a tile of depth 2 or
 * more, unless the unit has Float, Chicken or Frog, one of the Move in Water,
 * Walk on Water or Float movement abilities, or is a rider. */
battle_unit_action_restriction_e battle_unit_get_action_restriction(const unit_stats_t* unit) {
    s32 depth;

    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_ACT)] & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))
        != 0) {
        return BATTLE_UNIT_ACTION_RESTRICTION_CANNOT_ACT;
    }
    if ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) != 0) {
        return BATTLE_UNIT_ACTION_RESTRICTION_CANNOT_ACT;
    }
    depth = g_battle_map_tile_data[battle_map_calculate_location(unit)].depth_half_height.depth;
    if (depth < 2) {
        return BATTLE_UNIT_ACTION_RESTRICTION_NONE;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & (STATUS_BYTE_MASK(STATUS_ID_FLOAT) | STATUS_BYTE_MASK(STATUS_ID_CHICKEN)
                | STATUS_BYTE_MASK(STATUS_ID_FROG)))
        != 0) {
        return BATTLE_UNIT_ACTION_RESTRICTION_NONE;
    }
    if ((unit->ability_flags.movement[2]
            & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_WALK_ON_WATER | BATTLE_MOVEMENT_SET_3_FLOAT))
        != 0) {
        return BATTLE_UNIT_ACTION_RESTRICTION_NONE;
    }
    return ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) == 0) << 1;
}

/* Builds a unit's 0xff-terminated action-menu skillset list and loads each
 * entry's abilities.
 *
 * `menu_types` receives each entry's low-nibble menu type and `empty` is set
 * to 1 where battle_menu_load_skillset returned 0. A monster skillset in either
 * slot yields a single unloaded primary entry and returns 1; a unit that cannot
 * act or is carrying a rider gets an empty list. Returns -1 when the unit does
 * not exist, otherwise 0.
 *
 * The walk rewinds `entries` from `list` rather than reading `list` in the
 * loop: that keeps `entries` as CSE's canonical copy, so the list stores use
 * the incremented pointer instead of offsets from the saved base. `supports`
 * is declared first so it wins the allocation tie with `primary`. */
s32 battle_menu_build_unit_action_menus(s32 unit_id, u8* entries, u8* empty, u8* menu_types) {
    unit_stats_t* unit = battle_unit_get_existing_pointer(unit_id);
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
        menu_types[0] = g_main_action_menu_types_by_skillset[primary];
        entries[1] = SKILLSET_ID_NONE;
        return 1;
    }
    supports = unit->ability_flags.support[3];
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        supports = 0;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_ACT)] & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))
        || (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
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
    if (supports & BATTLE_SUPPORT_SET_4_DEFEND) {
        *entries++ = SKILLSET_ID_DEFEND;
    }
    if (supports & BATTLE_SUPPORT_SET_4_EQUIP_CHANGE) {
        *entries++ = SKILLSET_ID_EQUIP_CHANGE;
    }
    *entries = SKILLSET_ID_NONE;
    entries = list;
    for (i = 0; i < 6; i++) {
        id = entries[i];
        if (id == SKILLSET_ID_NONE) {
            break;
        }
        menu_type = g_main_action_menu_types_by_skillset[id] & 0xf;
        menu_types[i] = menu_type;
        if (battle_menu_load_skillset(unit_id, id, menu_type) == 0) {
            empty[i] = 1;
        } else {
            empty[i] = 0;
        }
    }
    return 0;
}

/* Loads the unit's usable abilities for an action menu type into scratch lists.
 *
 * A frog only keeps the default, blank, monster and attack menus. Returns the
 * menu loader's result, or whether the menu type is below 15 for menus without
 * a loader. */
static s32 battle_menu_load_skillset(s32 unit_id, u8 skillset, u8 menu) {
    u8 items[0x58];
    s16 abilities[0x44];
    u8 mp_costs[0x18];
    u8 ability_ct[0x18];
    u8 ability_flags[0x18];
    u8 turns[0x18];
    s32 result;

    if ((g_battle_unit_stats[unit_id].status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
            & STATUS_BYTE_MASK(STATUS_ID_FROG))
        && menu != ACTION_MENU_TYPE_DEFAULT && menu != ACTION_MENU_TYPE_MONSTER && menu != ACTION_MENU_TYPE_ATTACK
        && menu != ACTION_MENU_TYPE_BLANK_05) {
        return 0;
    }
    switch (menu) {
    case ACTION_MENU_TYPE_DEFAULT:
        return battle_menu_get_unit_skillset_ability_data(
            unit_id, skillset, abilities, mp_costs, ability_ct, 0, ability_flags, turns);
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        return battle_menu_load_item_abilities(unit_id, skillset, items);
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        return battle_menu_collect_throwable_items(unit_id, skillset, items);
    case ACTION_MENU_TYPE_ARITHMETICKS:
        result = battle_menu_load_math_skill_attributes(unit_id, skillset, abilities);
        if (result != 0) {
            result = battle_menu_load_math_skill_multiples(unit_id, skillset, abilities);
            if (result != 0) {
                result = battle_menu_collect_calculator_abilities(unit_id, skillset, abilities);
            }
        }
        return result;
    case ACTION_MENU_TYPE_ELEMENTS:
        return battle_menu_load_elemental_abilities(unit_id, skillset);
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        return battle_menu_collect_draw_out_katanas(unit_id, skillset, items);
    case ACTION_MENU_TYPE_CHARGE:
        return battle_menu_call_load_charge_skillset(unit_id, skillset, abilities);
    default:
        return menu < 15;
    }
}

/* Count the AT-list entries that resolve before an action by battle_id
 * charging for ct; returns 0xff when no such position exists within 40. */
u8 battle_action_get_number_of_turns_to_resolve(s32 battle_id, s32 ct, const battle_at_entry_t* entries) {
    s32 count;
    if (g_battle_between_turn_resume_state == BATTLE_BETWEEN_TURN_STATE_PICK_UNIT_AFTER_CHARGE && ct != 0) {
        ct += 1;
    }
    ct <<= 8;
    for (count = 0; count < BATTLE_AT_LIST_CAPACITY; count++, entries++) {
        s32 entry_unit = entries->unit_code;
        s32 is_action = entry_unit & BATTLE_AT_ACTION_ENTRY;
        s32 entry_ct;
        entry_unit &= BATTLE_AT_UNIT_ID_MASK;
        if (entry_unit == BATTLE_AT_END_OF_LIST)
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
