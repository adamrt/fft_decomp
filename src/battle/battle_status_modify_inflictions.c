#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "fft/status.h"
#include "psx/types.h"

/* Filters the target's pending status inflictions and removals.
 *
 * Unless removal_only is set, inflictions are masked by the rider whitelist,
 * the target's current (sets 0-2) or innate (sets 3-4) statuses, immunities,
 * the immortal blacklist and each status's can't-stack list, then every
 * remaining infliction clears the statuses it cancels. Inflicting Frog on a
 * frog turns into its removal. Removals always drop innate statuses and keep
 * only statuses the target has. Returns the sum of the remaining bytes, so 0
 * means nothing is left to inflict or remove. */
s32 battle_status_modify_inflictions(s32 removal_only) {
    battle_action_data_t* action;
    u8 inflicted;
    s32 immortal;
    s32 total;
    s32 i;
    s32 j;
    s32 byte;
    s32 mask;

    total = 0;
    immortal = g_battle_action_target->team_flags & BATTLE_TEAM_FLAG_IMMORTAL;
    if (g_battle_action_target->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
            g_battle_action_target_data->status_infliction[i] &= g_battle_rider_status_infliction_mask[i];
        }
    }
    action = g_battle_action_target_data;
    inflicted = action->status_infliction[2];
    if ((inflicted & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG))
        && (g_battle_action_target->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG))
        && removal_only == 0) {
        action->status_infliction[2] = inflicted & ~BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG);
        g_battle_action_target_data->status_removal[2] |= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG);
    }
    for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
        if (removal_only == 0) {
            if (i < 3) {
                g_battle_action_target_data->status_infliction[i] &= ~(
                    g_battle_action_target->status_sets.immunity[i] | g_battle_action_target->status_sets.current[i]);
            } else {
                g_battle_action_target_data->status_infliction[i] &= ~(
                    g_battle_action_target->status_sets.immunity[i] | g_battle_action_target->status_sets.innate[i]);
            }
            if (immortal) {
                g_battle_action_target_data->status_infliction[i]
                    &= ~g_main_status_check_sets[MAIN_STATUS_CHECK_SET_FORMATION_IMMUNITY][i];
            }
        }
        g_battle_action_target_data->status_removal[i] = g_battle_action_target_data->status_removal[i]
            & ~g_battle_action_target->status_sets.innate[i] & g_battle_action_target->inflicted_status[i];
    }
    if (removal_only == 0) {
        for (i = 0; i < BATTLE_STATUS_COUNT; i++) {
            byte = i / 8;
            mask = 0x80 >> (i & 7);
            if (g_battle_action_target_data->status_infliction[byte] & mask) {
                for (j = 0; j < BATTLE_STATUS_BYTE_COUNT; j++) {
                    if (g_battle_action_target->status_sets.current[j] & g_main_status_effect_data[i].cant_stack[j]) {
                        g_battle_action_target_data->status_infliction[byte] &= ~mask;
                    }
                }
            }
        }
        for (i = 0; i < BATTLE_STATUS_COUNT; i++) {
            byte = i / 8;
            mask = 0x80 >> (i & 7);
            if (g_battle_action_target_data->status_infliction[byte] & mask) {
                for (j = 0; j < BATTLE_STATUS_BYTE_COUNT; j++) {
                    g_battle_action_target_data->status_infliction[j] &= ~g_main_status_effect_data[i].cancels[j];
                }
            }
        }
    }
    for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
        total = total + g_battle_action_target_data->status_infliction[i]
            + g_battle_action_target_data->status_removal[i];
    }
    return total;
}
