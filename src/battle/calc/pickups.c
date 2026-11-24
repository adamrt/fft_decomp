/* Resolve Move-Find items, crystal and treasure pickups and canceled charges. */

#include "fft/battle/action.h"
#include "fft/battle/crystal.h"
#include "fft/battle/move.h"
#include "fft/game/ability.h"
#include "fft/game/battle_unit.h"
#include "fft/game/character_identity.h"
#include "fft/game/item.h"
#include "fft/game/job.h"
#include "fft/game/skillset.h"
#include "fft/game/status.h"
#include "fft/script/script_variable.h"
#include "fft/sys/math.h"
#include "fft/sys/memory.h"
#include "psx/libc.h"
#include "psx/types.h"

extern crystal_pickup_result_t g_crystal_pickup_result;

/* Move-Find scratch is private to this unit. The first-byte flags alias retains
 * the target's independent address load; accessing every byte through the record
 * instead merges that load with the returned record address. */
static move_found_item_t s_selected_item = { 0 };

static move_find_result_data_t s_tile_pickup_result = { 0 };

extern u8 g_move_find_result_flags;

static s32 test_or_set_item_flag(s32 map_id, s32 tile_index, s32 save);

static void clear_action_state(battle_unit_t* unit);

static u8 generate_treasure_item(battle_unit_t* unit);

/* 1 when the unit, or its rider when it is a mount, is charging an attack of
 * the Charge skillset; with cancel set, that unit's charge is also canceled
 * (clear_action_state). A move or a knockback cancels a Charge
 * through this test. */
s32 action_check_and_cancel_charge(battle_unit_t* unit, s32 cancel) {
    u8 mount_info;

    for (;;) {
        if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)] & STATUS_BYTE_MASK(STATUS_ID_CHARGING))
            && unit->charged_ability_ct != 0xFF && unit->action_command.skillset_id == SKILLSET_ID_CHARGE) {
            if (cancel != 0) {
                clear_action_state(unit);
            }
            return 1;
        }
        mount_info = unit->mount_info;
        if (!(mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT)) {
            break;
        }
        unit = &g_battle_units[mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK];
    }
    return 0;
}

/* Clears the unit's Charging, Jump, Defending and Performing statuses (action state none). */
static void clear_action_state(battle_unit_t* unit) {
    status_set_action_state(unit, STATUS_ACTION_STATE_NONE);
}

/* Pick the rare or common move-find item on the unit's tile and mark the tile found.
 *
 * The rare item is chosen with a (100 - Brave)% chance. */
move_found_item_t* move_find_determine_item(battle_unit_t* unit) {
    move_find_result_data_t* find_result
        = move_find_get_result(unit->x, unit->position.y, unit->position.higher_elevation);
    u8 item_id;

    if (random_roll_pass_fail(100, 100 - unit->brave) == 0) {
        item_id = find_result->rare_item_id;
    } else {
        item_id = find_result->common_item_id;
    }
    s_selected_item.item_id = item_id;
    test_or_set_item_flag(script_variable_get(SCRIPT_VAR_CURRENT_MAP), find_result->entry_index, 1);
    s_selected_item._unread_00 = 0;
    return &s_selected_item;
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
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = battle_unit_find_at_tile((u8)x, y, layer, UNIT_TILE_FILTER_TRANSPARENT);
        if (unit >= 0) {
            break;
        }
    }
    if (i != BATTLE_UNIT_SLOT_COUNT) {
        g_move_find_result_flags = MOVE_FIND_RESULT_OCCUPIED;
        s_tile_pickup_result.occupying_unit_id = unit;
    }
    for (i = 0; i < 4; i++) {
        tile = &g_move_find_current_map_data.entries[i];
        if (tile->x == (u8)x && tile->y == y && tile->elevation == layer) {
            break;
        }
    }
    if (i != 4) {
        s_tile_pickup_result.entry_index = i;
        if (!(tile->flags & MOVE_FIND_ENTRY_NO_ITEM)) {
            if (test_or_set_item_flag(script_variable_get(SCRIPT_VAR_CURRENT_MAP), i, 0) != 0) {
                g_move_find_result_flags |= MOVE_FIND_RESULT_ITEM;
                s_tile_pickup_result.rare_item_id = tile->rare_item_id;
                s_tile_pickup_result.common_item_id = tile->common_item_id;
            }
        }
        if (!(tile->flags & MOVE_FIND_ENTRY_NO_TRAP)) {
            if (tile->flags & MOVE_FIND_ENTRY_TRAP_DEGENERATOR) {
                trap = MOVE_FIND_TRAP_DEGENERATOR;
            } else if (tile->flags & MOVE_FIND_ENTRY_TRAP_DEATHTRAP) {
                trap = MOVE_FIND_TRAP_DEATHTRAP;
            } else if (tile->flags & MOVE_FIND_ENTRY_TRAP_SLEEPING_GAS) {
                trap = MOVE_FIND_TRAP_SLEEPING_GAS;
            } else if (tile->flags & MOVE_FIND_ENTRY_TRAP_STEEL_NEEDLE) {
                trap = MOVE_FIND_TRAP_STEEL_NEEDLE;
            }
            if (trap != MOVE_FIND_TRAP_NONE) {
                result = &g_move_find_result_flags;
                s_tile_pickup_result.trap_id = trap;
                *result |= MOVE_FIND_RESULT_TRAP;
            }
        }
    }
    return &s_tile_pickup_result;
}

/* Test (save == 0) or set the found flag of one of a map's four move-find item tiles.
 *
 * Each flag byte holds two maps, four tiles each, MSB first with the even map in
 * the high nibble; the test returns 1 while the item is still unfound. */
static s32 test_or_set_item_flag(s32 map_id, s32 tile_index, s32 save) {
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
crystal_pickup_result_t* crystal_pickup_build_result(battle_unit_t* mover) {
    u8 gained[UNIT_LEARNED_ABILITY_BYTES_PER_JOB];
    u8 known[UNIT_LEARNED_ABILITY_BYTES_PER_JOB];
    u8 offered[UNIT_LEARNED_ABILITY_BYTES_PER_JOB];
    s32 learned_count;
    s32 mover_identity;
    s32 crystal_identity;
    s32 mover_flags;
    s32 crystal_flags;
    s32 unlocked_jobs_mask;
    battle_unit_t* crystal;
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
        UNIT_TILE_FILTER_CRYSTAL | UNIT_TILE_FILTER_TREASURE);
    if (crystal_index < 0) {
        g_crystal_pickup_result.result = CRYSTAL_PICKUP_NONE;
        return &g_crystal_pickup_result;
    }
    g_crystal_pickup_result.result = CRYSTAL_PICKUP_NOTHING;
    g_crystal_pickup_result.unit_index = crystal_index;
    crystal = &g_battle_units[crystal_index];
    if (crystal->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
        g_crystal_pickup_result.result = CRYSTAL_PICKUP_TREASURE;
        g_crystal_pickup_result.treasure_item = generate_treasure_item(crystal);
    } else {
        /* The empty loops are measured residue: without them s1 and s2 swap. */
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
                        ability = skillset_get_ability_id(skillset, j);
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
 * the result's treasure item to the inventory and returns that call's
 * result, ignoring the other flags. RESTORE_HP_MP turns the result into a hit
 * that heals the missing HP and MP. LEARN_ABILITIES ORs
 * the result's per-job learned bits into the unit's learned-ability bits. These
 * return 0. */
s32 crystal_pickup_apply_choice(battle_unit_t* unit, s32 flags) {
    s32 i;
    s32 j;

    action_reset_result(&unit->action);
    unit->action.miss_type = ACTION_MISS_TYPE_CANCELED;
    unit->action.hit = 0;
    if (flags & CRYSTAL_PICKUP_CHOICE_TAKE_TREASURE) {
        return item_give(unit, g_crystal_pickup_result.treasure_item);
    }
    if (flags & CRYSTAL_PICKUP_CHOICE_RESTORE_HP_MP) {
        unit->action.hit = 1;
        unit->action.attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_MP_HEALING;
        unit->action.miss_type = ACTION_MISS_TYPE_HIT;
        unit->action.hp_healing = unit->max_hp - unit->hp;
        unit->action.mp_healing = unit->max_mp - unit->mp;
    }
    if (flags & CRYSTAL_PICKUP_CHOICE_LEARN_ABILITIES) {
        for (i = 0; i < UNIT_LEARNED_ABILITY_JOB_COUNT; i++) {
            for (j = 0; j < UNIT_LEARNED_ABILITY_BYTES_PER_JOB; j++) {
                unit->career.learned_abilities[i][j] |= g_crystal_pickup_result.learned[i][j];
            }
        }
    }
    return 0;
}

/* Item a fallen unit leaves as treasure: one of its equipped items at random,
 * or with nothing equipped one of the thirteen consumables other than Elixir.
 * Decided only when executing; in any other state the function returns without
 * a value. */
static u8 generate_treasure_item(battle_unit_t* unit) {
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
            candidates[0]++;
        }
        count = 1;
    }

    i = (rand() * count) / 0x8000;
    return candidates[i];
}
