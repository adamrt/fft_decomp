#include "fft/battle.h"

/* Status display image IDs, eight per status_infliction/status_removal byte,
 * indexed from the byte's high bit: a non-const [5][8] view of
 * g_battle_action_status_display_image_ids, whose flat const declaration
 * changes this function's code. */
extern u8 g_battle_action_status_display_image_ids_view[5][8];

/* Queues the post-action displays for a unit's resolved action.
 *
 * A hit sets one action_display_flags bit per HP/MP, stat, level, steal and
 * break result and lists the inflicted/removed status images (removals
 * marked 0x80); a miss queues MISSED, GUARDED or CAUGHT by miss type. The
 * first display is built immediately unless a numeric display is active. */
void battle_gfx_prepare_post_action_display(battle_unit_misc_data_t* unit) {
    battle_stats_t* stats;
    battle_action_data_t* action;
    s32 i;
    s32 j;
    s32 bit;
    u8 count;
    u8 image;
    u8 miss;

    unit->numeric_displays[0]->spritesheet_id = unit->numeric_displays[1]->spritesheet_id
        = unit->numeric_displays[2]->spritesheet_id = 0x1f;
    stats = unit->battle_data;
    action = &stats->action;
    if (action->hit != 0) {
        if (action->attack_type & BATTLE_ACTION_TYPE_HP_DAMAGE) {
            unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_HP_DAMAGE;
        }
        if (action->attack_type & BATTLE_ACTION_TYPE_HP_HEALING) {
            unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_HP_HEALING;
        }
        if (action->attack_type & BATTLE_ACTION_TYPE_MP_DAMAGE) {
            unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_MP_DAMAGE;
        }
        if (action->attack_type & BATTLE_ACTION_TYPE_MP_HEALING) {
            unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_MP_HEALING;
        }
        if (action->attack_type & BATTLE_ACTION_TYPE_PSEUDO_STATUS) {
            if (action->sp_change != 0) {
                if (action->sp_change & 0x80) {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_SPEED_GAIN;
                } else {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_SPEED_DAMAGE;
                }
            }
            if (action->ct_change != 0) {
                if (action->ct_change == 0xff) {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_QUICK;
                } else if (action->ct_change == 0x7f) {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_CT_ZERO;
                } else if (action->ct_change & 0x80) {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_CT_GAIN;
                } else {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_CT_DAMAGE;
                }
            }
            if (action->pa_change != 0) {
                if (action->pa_change & 0x80) {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_PA_GAIN;
                } else {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_PA_DAMAGE;
                }
            }
            if (action->ma_change != 0) {
                if (action->ma_change & 0x80) {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_MA_GAIN;
                } else {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_MA_DAMAGE;
                }
            }
            if (action->brave_change != 0) {
                if (action->brave_change & 0x80) {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_BRAVE_GAIN;
                } else {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_BRAVE_DAMAGE;
                }
            }
            if (action->faith_change != 0) {
                if (action->faith_change & 0x80) {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_FAITH_GAIN;
                } else {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_FAITH_DAMAGE;
                }
            }
        }
        if (action->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_LEVEL_UP) {
            unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_LEVEL_UP;
        }
        if (action->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_LEVEL_DOWN) {
            unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_LEVEL_DOWN;
        }
        if (action->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
            unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_STOLEN;
        }
        if (action->special_effect
            & (BATTLE_ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | BATTLE_ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN)) {
            unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_BROKEN;
        }
        if (action->attack_type & BATTLE_ACTION_TYPE_STATUS_CHANGE) {
            unit->status_display_image_count = 0;
            for (i = 0; i < 5; i++) {
                for (j = 0; j < 8; j++) {
                    count = unit->status_display_image_count;
                    bit = 0x80 >> j;
                    if (count < BATTLE_ACTION_STATUS_DISPLAY_CAPACITY) {
                        if (action->status_infliction[i] & bit) {
                            image = g_battle_action_status_display_image_ids_view[i][j];
                            unit->status_display_image_list[count] = image;
                            if (image != 0) {
                                unit->status_display_image_count++;
                            }
                        } else if (action->status_removal[i] & bit) {
                            image = g_battle_action_status_display_image_ids_view[i][j];
                            unit->status_display_image_list[count] = image;
                            if (image != 0) {
                                unit->status_display_image_list[unit->status_display_image_count] |= 0x80;
                                unit->status_display_image_count++;
                            }
                        }
                    }
                }
            }
        }
    } else {
        miss = action->miss_type;
        if (miss != BATTLE_ACTION_MISS_TYPE_HIT) {
            if ((u32)(action->miss_type - 1) < 2 || miss == BATTLE_ACTION_MISS_TYPE_LEFT_HAND_EVADE
                || miss == BATTLE_ACTION_MISS_TYPE_FORCED_FAILURE
                || miss == BATTLE_ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD) {
                if ((action->status_change & 3) == 1) {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_MISSED;
                } else {
                    unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_GUARDED;
                }
            } else if (miss == BATTLE_ACTION_MISS_TYPE_CATCH) {
                unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_CAUGHT;
            } else if (miss != BATTLE_ACTION_MISS_TYPE_CANCELLED && miss != 0
                && miss != BATTLE_ACTION_MISS_TYPE_REFLECTED) {
                unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_MISSED;
            }
        }
    }
    if (unit->numeric_display_active == 0) {
        unit->numeric_display_progress = 0;
        battle_gfx_build_next_action_result_display(unit);
    }
}
