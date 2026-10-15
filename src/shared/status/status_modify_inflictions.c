#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Filters the target's pending status inflictions and removals.
 *
 * The rider whitelist always masks pending inflictions. Unless removal_only
 * is set, further masks use the current (sets 0-2) or innate (sets 3-4) statuses, immunities,
 * the immortal blacklist and each status's can't-stack list, then every
 * remaining infliction clears the statuses it cancels. Inflicting Frog on a
 * frog turns into its removal. Removals always drop innate statuses and keep
 * only statuses the target has. Returns the sum of the remaining bytes, so 0
 * means nothing is left to inflict or remove. */
s32 status_modify_inflictions(s32 removal_only) {
    action_data_t* action;
    u8 inflicted;
    s32 immortal;
    s32 total;
    s32 i;
    s32 j;
    s32 byte;
    s32 mask;

    total = 0;
    immortal = g_action_target->team_flags & BATTLE_TEAM_FLAG_IMMORTAL;
    if (g_action_target->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_action_target_data->status_changes.infliction[i] &= g_battle_rider_status_infliction_mask[i];
        }
    }
    action = g_action_target_data;
    inflicted = action->status_changes.infliction[2];
    if ((inflicted & STATUS_BYTE_MASK(STATUS_ID_FROG))
        && (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG))
        && removal_only == 0) {
        action->status_changes.infliction[2] = inflicted & ~STATUS_BYTE_MASK(STATUS_ID_FROG);
        g_action_target_data->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
            |= STATUS_BYTE_MASK(STATUS_ID_FROG);
    }
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        if (removal_only == 0) {
            if (i < 3) {
                g_action_target_data->status_changes.infliction[i]
                    &= ~(g_action_target->status_sets.immunity[i] | g_action_target->status_sets.current[i]);
            } else {
                g_action_target_data->status_changes.infliction[i]
                    &= ~(g_action_target->status_sets.immunity[i] | g_action_target->status_sets.innate[i]);
            }
            if (immortal) {
                g_action_target_data->status_changes.infliction[i]
                    &= ~g_main_status_check_sets[MAIN_STATUS_CHECK_SET_FORMATION_IMMUNITY][i];
            }
        }
        g_action_target_data->status_changes.removal[i] = g_action_target_data->status_changes.removal[i]
            & ~g_action_target->status_sets.innate[i] & g_action_target->inflicted_status[i];
    }
    if (removal_only == 0) {
        for (i = 0; i < STATUS_COUNT; i++) {
            byte = i / 8;
            mask = 0x80 >> (i & 7);
            if (g_action_target_data->status_changes.infliction[byte] & mask) {
                for (j = 0; j < STATUS_BYTE_COUNT; j++) {
                    if (g_action_target->status_sets.current[j] & g_main_status_effect_data[i].cant_stack[j]) {
                        g_action_target_data->status_changes.infliction[byte] &= ~mask;
                    }
                }
            }
        }
        for (i = 0; i < STATUS_COUNT; i++) {
            byte = i / 8;
            mask = 0x80 >> (i & 7);
            if (g_action_target_data->status_changes.infliction[byte] & mask) {
                for (j = 0; j < STATUS_BYTE_COUNT; j++) {
                    g_action_target_data->status_changes.infliction[j] &= ~g_main_status_effect_data[i].cancels[j];
                }
            }
        }
    }
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        total = total + g_action_target_data->status_changes.infliction[i]
            + g_action_target_data->status_changes.removal[i];
    }
    return total;
}
