/* Resident unit state shared by battles and menus: status changes, battle resets,
 * sprite identity, inventory totals and action records. The native span also retains
 * the small memory, random and error routines used by these services. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/memory.h"
#include "fft/party.h"
#include "fft/random.h"
#include "fft/script_variable.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_entd.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/unit_runtime.h"
#include "fft/zodiac.h"
#include "psx/types.h"

static u8 sprite_identity_resolve(const unit_stats_t* unit, u8* palette);

extern const u16 g_zodiac_day_limits[ZODIAC_SIGN_ORDINARY_COUNT];

/* Clear the per-battle unit state: manual auto-battle with no target, CT 0, no active turn or
 * ability outcome, no charging ability (0xff), not mounted and no KOs. */
void unit_battle_state_reset(unit_stats_t* unit) {
    unit->auto_battle_setting = AI_AUTOBATTLE_MANUAL;
    unit->auto_battle_target = 0;
    unit->ct = 0;
    unit->has_turn = 0;
    unit->ability_outcome = 0;
    unit->charged_ability_ct = 0xff;
    unit->mount_info = 0;
    unit->ko_count = 0;
}

/* Raise a Speed of 0 to 1, optionally clear the war trophy and bonus money, and
 * initialize the unit's status data (status_unit_init). */
void unit_status_rewards_init(unit_stats_t* unit, s32 clear_rewards) {
    if (unit->attributes[UNIT_ATTRIBUTE_SPEED] == 0) {
        unit->attributes[UNIT_ATTRIBUTE_SPEED] = 1;
    }
    if (clear_rewards != 0) {
        unit->war_trophy = ITEM_ID_NOTHING;
        unit->bonus_money_modifier = 0;
    }
    status_unit_init(unit);
}

/* The range byte of an ordinary ability's range record; 0 for an id from
 * ABILITY_ID_ITEM_FIRST up, which has no such record. Nothing in the tree
 * calls it. */
u32 ability_range_get(u32 ability_id) {
    u16 index = ability_id;

    return index < ABILITY_ID_ITEM_FIRST ? g_ability_range_data[index].range : 0;
}

/* The area-of-effect byte of an ordinary ability's range record; 0 for an id from
 * ABILITY_ID_ITEM_FIRST up, which has no such record. Nothing in the tree calls
 * it. */
u32 ability_aoe_get(u32 ability_id) {
    u16 index = ability_id;

    return index < ABILITY_ID_ITEM_FIRST ? g_ability_range_data[index].aoe : 0;
}

/* Resolve the unit's spritesheet and palette (sprite_identity_resolve), store both in
 * the unit and return the spritesheet id. */
u8 unit_sprite_identity_set(unit_stats_t* unit, u8* palette) {
    u8 spritesheet = sprite_identity_resolve(unit, palette);

    unit->spritesheet_id = spritesheet;
    unit->job_portrait_palette = *palette;
    return spritesheet;
}

/* Roll 0..maximum-1 and compare it with threshold: returns 0 when the roll is
 * below threshold and 1 otherwise. An event tested with `== 0` therefore has
 * threshold-in-maximum odds. */
s32 random_roll_pass_fail(s32 maximum, s32 threshold) {
    s32 roll = rand() * maximum / 0x8000;

    return roll >= threshold;
}

/* Resolve a unit's character identity to a spritesheet id; *palette is 0 except
 * for a monster.
 *
 * An identity below 0x80 is its own spritesheet. The generic male and female
 * selectors give the job's sheet, 0x60 + (job - JOB_ID_SQUIRE) * 2 plus one for a
 * female, with Dancer and Mime counted one job lower. The monster selector
 * returns the spritesheet and palette already in the unit (from its job record).
 * Any higher identity gives job_id + 0x28. */
static u8 sprite_identity_resolve(const unit_stats_t* unit, u8* palette) {
    u8 job_id;
    u8 sprite_set;

    *palette = 0;
    job_id = unit->job_id;
    sprite_set = unit->character_identity;
    if ((job_id == JOB_ID_DANCER) || (job_id == JOB_ID_MIME)) {
        job_id--;
    }
    if (sprite_set < CHARACTER_IDENTITY_SELECTOR_FIRST) {
        return sprite_set;
    }
    if (sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
        sprite_set = job_id * 2 - (JOB_ID_SQUIRE * 2 - CHARACTER_IDENTITY_FIXED_MALE_SQUIRE);
        return sprite_set;
    }
    if (sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
        sprite_set = job_id * 2 - (JOB_ID_SQUIRE * 2 - CHARACTER_IDENTITY_FIXED_FEMALE_SQUIRE);
        return sprite_set;
    }
    if (sprite_set == CHARACTER_IDENTITY_MONSTER) {
        *palette = unit->job_portrait_palette;
        return unit->spritesheet_id;
    }
    sprite_set = job_id + 0x28;
    return sprite_set;
}

/* Return 1 when the unit currently has any status in one row of g_status_check_sets, else 0. */
s32 status_unit_set_test(const unit_stats_t* unit, status_check_set_e status_set_index) {
    const u8* status_set = g_status_check_sets[0];
    s32 index = 0;

    status_set += status_set_index * STATUS_BYTE_COUNT;

    for (; index < STATUS_BYTE_COUNT; index++) {
        if ((unit->status_sets.current[index] & status_set[index]) != 0) {
            return 1;
        }
    }
    return 0;
}

/* Return the learned-ability bits of one job slot of career.learned_abilities
 * (0 = base job) as a 24-bit value, first byte most significant. */
u32 ability_learned_get(const unit_stats_t* unit, u32 job_index) {
    return (unit->career.learned_abilities[job_index][0] << 16) + (unit->career.learned_abilities[job_index][1] << 8)
        | unit->career.learned_abilities[job_index][2];
}

/* Copy one action command record byte by byte. An aggregate assignment would
 * replace the retail byte-copy loop. */
void action_command_copy(const action_command_t* source, action_command_t* destination) {
    const u8* from = (const u8*)source;
    u8* to = (u8*)destination;
    u32 index;

    for (index = 0; index < sizeof(*destination); index++) {
        *to++ = *from++;
    }
}

/* A second native entry with the same copy loop as mem_copy_bytes_source_first. */
void mem_copy_byte_data_source_first(const void* source, void* destination, int count) {
    const u8* src = source;
    u8* dst = destination;
    int i;

    for (i = 0; i < count; i++) {
        *dst++ = *src++;
    }
}

/* Count the copies of an item the player owns: the inventory stock plus every roster human's
 * equipped copies. With include_battle set, roster humans deployed in the battle are counted from
 * their battle records instead, along with every other non-Mime human that started on the blue
 * team. */
s32 item_total_quantity_get(u8 item_id, s32 include_battle) {
    s32 total = g_item_quantities[item_id];
    s32 i;
    s32 j;
    party_unit_t* party;
    unit_stats_t* unit;
    s32 deployed;
    s32 party_id;

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        party = party_unit_get(i);
        party_id = party->party_id;
        if (party_id == PARTY_ID_NONE) {
            continue;
        }
        if (party->gender_flags & UNIT_FLAG_MONSTER) {
            continue;
        }
        if (include_battle != 0) {
            deployed = 0;
            for (j = 0; j < UNIT_SLOT_COUNT; j++) {
                unit = &g_unit_stats[j];
                if (unit->existence != UNIT_EXISTENCE_NONE && !(unit->unit_flags & UNIT_FLAG_MONSTER)
                    && unit->formation_index == party_id) {
                    deployed = 1;
                    break;
                }
            }
            if (deployed != 0) {
                continue;
            }
        }
        for (j = 0; j < UNIT_EQUIPMENT_SLOT_COUNT; j++) {
            if (party->equipment[j] == item_id) {
                total++;
            }
        }
    }

    if (include_battle == 0) {
        return total;
    }

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        if (unit->existence != UNIT_EXISTENCE_NONE && !(unit->unit_flags & UNIT_FLAG_MONSTER)
            && !(unit->initial_team_flags & UNIT_TEAM_MASK) && unit->job_id != JOB_ID_MIME) {
            for (j = 0; j < UNIT_EQUIPMENT_SLOT_COUNT; j++) {
                if (unit->equipment[j] == item_id) {
                    total++;
                }
            }
        }
    }
    return total;
}

/* Copy the ENTD unit's map X and Y, elevation, facing and spell-quote bits, war trophy and bonus
 * money into the battle unit. */
void unit_position_rewards_init(unit_stats_t* unit, const entd_unit_t* entd) {
    unit->x = entd->x;
    unit->position.y = entd->position.y;
    unit->position.higher_elevation = entd->position.higher_elevation;
    unit->position.facing = entd->position.facing;
    unit->position.spell_quote_skillsets = entd->position.spell_quote_skillsets;

    unit->war_trophy = entd->war_trophy;
    unit->bonus_money_modifier = entd->bonus_money_modifier;
}

/*
 * Return the highest-order status changed by an action. Bits 0-6 encode the
 * one-based status index, 0x80 marks removal, and 0x100 reflects flags_2 bit 3.
 */
s32 status_action_priority_find(const action_data_t* action) {
    s32 highest_order = -1;
    s32 result = 0;
    s32 status_index;
    u8 mask;
    s32 byte_index;
    u8 current_order;

    for (status_index = 0; status_index < STATUS_COUNT; status_index++) {
        byte_index = status_index / 8;
        mask = 0x80 >> (status_index & 7);
        if (action->status_changes.removal[byte_index] & mask) {
            current_order = g_status_effect_data[status_index].order;
            if (current_order > highest_order) {
                highest_order = current_order;
                result = status_index + 1 + STATUS_ACTION_RESULT_REMOVAL;
                if (g_status_effect_data[status_index].flags_2 & STATUS_EFFECT_FLAG_2_SCRIPT_FILTER_EXCLUDED) {
                    result
                        = status_index + 1 + STATUS_ACTION_RESULT_REMOVAL + STATUS_ACTION_RESULT_SCRIPT_FILTER_EXCLUDED;
                }
            }
        }
        if (action->status_changes.infliction[byte_index] & mask) {
            current_order = g_status_effect_data[status_index].order;
            if (current_order > highest_order) {
                highest_order = current_order;
                result = status_index + 1;
                if (g_status_effect_data[status_index].flags_2 & STATUS_EFFECT_FLAG_2_SCRIPT_FILTER_EXCLUDED) {
                    result = status_index + 1 + STATUS_ACTION_RESULT_SCRIPT_FILTER_EXCLUDED;
                }
            }
        }
    }
    return result;
}

/* Return the zodiac sign for a day-of-year birthday.
 *
 * Counts the sign boundaries at or before the day, then rotates the count
 * into zodiac order with (count + 9) % 12. */
s32 zodiac_sign_calculate(u16 birthday) {
    s32 zodiac = 0;
    s32 i;

    for (i = 0; i < ZODIAC_SIGN_ORDINARY_COUNT; i++) {
        zodiac += birthday >= g_zodiac_day_limits[i];
    }
    zodiac = (zodiac + 9) % ZODIAC_SIGN_ORDINARY_COUNT;
    return (u16)zodiac;
}

/* Clear count bytes one at a time; a count of 0 or less clears nothing. */
void mem_zero(void* destination, s32 count) {
    u8* dst = destination;
    s32 index = 0;

    while (index < count) {
        *dst++ = 0;
        index++;
    }
}

/* Count a KO in the script variables: a unit that started on the blue team adds to
 * EVENT_SCRIPT_VAR_CASUALTIES, any other unit to EVENT_SCRIPT_VAR_INJURED; both stop at 9999. */
void unit_casualties_count(const unit_stats_t* unit) {
    s32 id;
    s32 count;

    id = (unit->initial_team_flags & UNIT_TEAM_MASK) != UNIT_TEAM_BLUE ? EVENT_SCRIPT_VAR_INJURED
                                                                       : EVENT_SCRIPT_VAR_CASUALTIES;
    count = script_variable_get(id);
    if (count < EVENT_CASUALTY_COUNT_MAX) {
        count++;
    }
    script_variable_set(id, count);
}

/* Add, remove, or replace a status-set flag and refresh the unit's current status. */
void status_unit_change(unit_stats_t* unit, s32 status_byte_index, u8 status_flag, s32 change_type) {
    u8 updated;

    /* An enum selector removes the native signed range test. */
    switch (change_type) {
    case STATUS_ADD:
        updated = unit->inflicted_status[status_byte_index] | status_flag;
        break;
    case STATUS_REMOVE:
        updated = unit->inflicted_status[status_byte_index] & ~status_flag;
        break;
    case STATUS_REPLACE:
        updated = status_flag;
        break;
    }

    unit->inflicted_status[status_byte_index] = updated;
    unit->status_sets.current[status_byte_index] = updated | unit->status_sets.innate[status_byte_index];
}

/* Recompute the current status set as innate | inflicted. */
void status_current_update(unit_stats_t* unit) {
    s32 status_set;

    for (status_set = 0; status_set < STATUS_BYTE_COUNT; status_set++) {
        unit->status_sets.current[status_set]
            = unit->status_sets.innate[status_set] | unit->inflicted_status[status_set];
    }
}

/* Charge time of the unit's last ability: the low seven bits of the ability
 * record's CT byte. */
void unit_ability_ct_set(unit_stats_t* unit) {
    unit->ability_ct = g_ability_range_data[unit->action_command.ability_id].ct & 0x7f;
}

/* Make Charging, Jump, Defending or Performing the unit's only action status, or clear all four
 * with STATUS_ACTION_STATE_NONE, and enable or disable the special flags of each status that
 * changed. NONE and DEFENDING also clear the charging ability CT; PERFORMING sets it from the last
 * ability's CT. KEEP_PERFORMING keeps Performing if the unit has it and otherwise acts as NONE. */
void status_action_state_set(unit_stats_t* unit, u8 action_state) {
    u8 old_status = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)];
    s32 battle_id = unit->battle_id;
    u8 status;
    u8 cleared;
    s32 status_id;
    u8 mask;
    u8 changed;

    if (action_state == STATUS_ACTION_STATE_KEEP_PERFORMING) {
        /* The Performing bit (0x01) shifted left 3 is PERFORMING (8); without it, NONE (0). */
        action_state = (old_status & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) << 3;
    }
    switch (action_state) {
    case STATUS_ACTION_STATE_NONE:
        status = 0;
        unit->charged_ability_ct = 0xff;
        break;
    case STATUS_ACTION_STATE_CHARGING:
        status = STATUS_BYTE_MASK(STATUS_ID_CHARGING);
        break;
    case STATUS_ACTION_STATE_JUMPING:
        status = STATUS_BYTE_MASK(STATUS_ID_JUMP);
        break;
    case STATUS_ACTION_STATE_DEFENDING:
        status = STATUS_BYTE_MASK(STATUS_ID_DEFENDING);
        unit->charged_ability_ct = 0xff;
        break;
    case STATUS_ACTION_STATE_PERFORMING:
        status = STATUS_BYTE_MASK(STATUS_ID_PERFORMING);
        unit_ability_ct_set(unit);
        unit->charged_ability_ct = unit->ability_ct;
        break;
    }
    cleared = status
        ^ (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
            | STATUS_BYTE_MASK(STATUS_ID_DEFENDING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
    status_unit_change(unit, STATUS_BYTE_INDEX(STATUS_ID_CHARGING), status, STATUS_ADD);
    status_unit_change(unit, STATUS_BYTE_INDEX(STATUS_ID_CHARGING), cleared, STATUS_REMOVE);

    changed = old_status ^ unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)];
    mask = STATUS_BYTE_MASK(STATUS_ID_CHARGING);
    for (status_id = STATUS_HANDLER_INDEX(STATUS_ID_CHARGING); status_id <= STATUS_HANDLER_INDEX(STATUS_ID_PERFORMING);
        status_id++) {
        if (changed & mask) {
            if ((old_status & mask) != 0) {
                status_queue_display_flag_change_if_executing(status_id, STATUS_DISPLAY_CHANGE_REMOVE, battle_id);
            } else {
                status_queue_display_flag_change_if_executing(status_id, STATUS_DISPLAY_CHANGE_ADD, battle_id);
            }
        }
        mask >>= 1;
    }
}

/* Pass a BATTLE.BIN unit-setup error (system_battle_load_exception_e) to the empty
 * system_error_report hook as source 3. */
void system_battle_load_exception_handle(s32 value) {
    system_error_report(3, value);
}
