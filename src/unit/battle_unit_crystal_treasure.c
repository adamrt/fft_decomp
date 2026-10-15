/* Crystal/treasure pickup, slot lookups by identity and war trophies. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Resolve a crystal or treasure pickup for the unit that moved onto its tile.
 *
 * A treasure gives one item (result 4). A crystal (result 2) offers a human
 * mover the abilities of a human crystal, job by job: the base job only when
 * both share a character identity, the others when the mover has the job
 * unlocked (never Bard for a woman or Dancer for a man). Each ability the
 * crystal knows and the mover does not is inherited when a roll of 0..99 is
 * below its learn rate; result bit 0 is set when at least one is.
 *
 * The two one-trip loops around the crystal result store emit no code; they
 * count that use of the CSE'd &g_main_crystal_pickup_result pseudo two loop levels deeper
 * (8 refs), so global-alloc ranks it above `mover` and gives it s1 as in the
 * target. */
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
            mem_zero(g_main_crystal_pickup_result.learned_abilities, 0x40);
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

    action_clear_current_data(&unit->action);
    unit->action.miss_type = ACTION_MISS_TYPE_CANCELED;
    unit->action.hit = 0;
    if (flags & BATTLE_CRYSTAL_TAKE_TREASURE) {
        return action_increment_item_quantity_for_steal_break(unit, g_main_crystal_treasure_item_id);
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
u8 battle_unit_generate_treasure(unit_stats_t* unit) {
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
 * an active unit has it (-3 if it is crystallised or a treasure), -2/-3 for an
 * absent unit whose existence byte is 0/BATTLE_UNIT_EXISTENCE_DISABLED, -4
 * when a party roster entry
 * has it, else -5; -1 for selector identities.
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
    s32 count;
    s32 money;
    u8 lowest;
    s32 i;
    unit_stats_t* unit;
    u8 value;

    count = 0;
    money = 0;
    lowest = UNIT_LEVEL_MAX;
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
