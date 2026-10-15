#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Rebuilds the 40-entry AT list: charged and performing actions first, then
 * each unit's next turns simulated tick by tick under Stop/Sleep, Slow and
 * Haste. Returns -1 when no entry was added.
 *
 * One u8 local holds the Sleep CT, the charging CT and the per-tick CT gain,
 * as the target's shared $v1 shows. Its early live range conflicts with $v0,
 * which keeps the gain out of $v0 so reorg can copy the join's zero-extension
 * into both Haste delay slots. The u16 wait and the value + wait order keep
 * the target's addu operand order through combine. The perform loop's key must
 * be a multi-set u32: a single-set shift temporary gets sched1's birth boost
 * and lands after the a1 load. The 0x50-byte array is the unreferenced frame
 * area the target reserves. */
s32 action_calculate_at_list(battle_at_entry_t* list, battle_at_list_mode_e mode) {
    unit_stats_t* unit;
    u8 unused[0x50];
    s32 placed;
    s32 slot;
    s32 phase_tick;
    u16 wait;
    u8 value;
    u16 tick;
    s32 turns;
    s32 slow;
    s32 haste;
    u8 speed;
    u16 ct;
    u32 remainder;
    s32 added;
    u8 current;
    u8 innate;
    u32 key;

    placed = 0;
    phase_tick = g_battle_between_turn_resume_state == BATTLE_BETWEEN_TURN_STATE_PICK_UNIT_AFTER_CHARGE;
    for (slot = 0; slot < BATTLE_AT_LIST_CAPACITY; slot++) {
        list[slot].unit_code = 0xff;
        list[slot].action_id_low = 0xff;
        list[slot].sort_key = 0xffff;
    }

    if (mode != BATTLE_AT_LIST_MODE_ACTIONS_ONLY) {
        unit = battle_unit_find_active_data_pointer();
        if (unit != 0) {
            action_sort_at_list(unit->battle_id, 0, 0, list);
        }
    }

    for (slot = 0; slot < 0x15; slot++) {
        unit = &g_battle_unit_stats[slot];
        if (status_is_unit_absent_dead_crystal_treasure_petrified_or_ridden(unit) != 0) {
            continue;
        }

        wait = 0;
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_STOP)] & STATUS_BYTE_MASK(STATUS_ID_STOP)) {
            wait = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_STOP)];
        }
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)] & STATUS_BYTE_MASK(STATUS_ID_SLEEP)) {
            value = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_SLEEP)];
            if (wait < value) {
                wait = value;
            }
        }

        value = unit->charged_ability_ct;
        if (value != 0xff && mode != BATTLE_AT_LIST_MODE_TURNS_ONLY) {
            tick = value + wait;
            if (value != 0 || unit->has_turn == 0 || mode != BATTLE_AT_LIST_MODE_PREVIEW) {
                tick += phase_tick;
            }
            placed += action_sort_at_list(slot, 1, (tick << 8) & 0xff00, list);
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)]
                & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) {
                main_unit_copy_last_ability_ct(unit);
                for (turns = 0; turns < BATTLE_AT_LIST_CAPACITY - 1; turns++) {
                    tick += unit->ability_ct;
                    key = tick;
                    if (key >= 0x100) {
                        break;
                    }
                    key <<= 8;
                    placed += action_sort_at_list(slot, 1, key & 0xff00, list);
                }
            }
        }

        if (mode == BATTLE_AT_LIST_MODE_ACTIONS_ONLY) {
            continue;
        }

        slow = 0;
        haste = 0;
        current = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLOW)];
        innate = unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_SLOW)];
        if (current & STATUS_BYTE_MASK(STATUS_ID_SLOW)) {
            slow = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_SLOW)];
            if (innate & STATUS_BYTE_MASK(STATUS_ID_SLOW)) {
                slow = 0x200;
            }
            slow -= wait;
            if (slow < 0) {
                slow = 0;
            }
        }
        if (current & STATUS_BYTE_MASK(STATUS_ID_HASTE)) {
            haste = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_HASTE)];
            if (innate & STATUS_BYTE_MASK(STATUS_ID_HASTE)) {
                haste = 0x200;
            }
            haste -= wait;
            if (haste < 0) {
                haste = 0;
            }
        }

        speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
        if (speed == 0) {
            speed = 1;
        }
        tick = wait;
        ct = unit->ct;
        for (turns = 0; turns < BATTLE_AT_LIST_CAPACITY; turns++) {
            while (ct < UNIT_TURN_CT_THRESHOLD) {
                tick++;
                value = speed;
                if (slow != 0) {
                    value >>= 1;
                    slow--;
                    if (value == 0) {
                        value = 1;
                    }
                    if (haste != 0) {
                        haste--;
                    }
                } else if (haste != 0) {
                    haste--;
                    value = value * 3 >> 1;
                }
                ct += value;
                if (tick >= 0x1f5) {
                    break;
                }
            }
            if (tick >= 0x100) {
                break;
            }
            remainder = ct;
            remainder %= UNIT_TURN_CT_THRESHOLD;
            ct = remainder;
            added = action_sort_at_list(slot, 0, ((tick << 8) + (UNIT_TURN_CT_THRESHOLD - remainder)) & 0xffff, list);
            if (added == 0) {
                break;
            }
            placed += added;
        }
    }

    return -(placed == 0);
}
