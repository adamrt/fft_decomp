#include "fft/battle.h"
#include "psx/types.h"

/*
 * Queue the post-action "effect message" popups for one unit.
 *
 * Walks the 11 reportable outcomes recorded in the unit's
 * battle_action_data_t and appends one 8-byte record per outcome to the
 * queue at g_battle_action_post_effect_msgs: message code at +0, the acting unit's misc ID at +1,
 * and the reported value at +4. g_battle_action_post_effect_msg_counter is
 * the record count, drained by battle_action_resume_attack_phase_control.
 *
 * The 11-way dispatch really is a jump table in the target (at 0x80067548),
 * which is why the .ld places a .rodata output section there.
 */
void battle_action_queue_post_effect_messages_for_unit(battle_unit_misc_data_t* misc) {
    battle_action_data_t* action;
    s32 i;
    s32 queued;
    s32 off;

    if (misc == 0) {
        return;
    }
    if (misc->battle_data == 0) {
        return;
    }
    if (g_battle_action_post_effect_msg_counter >= 0x10) {
        return;
    }
    action = &misc->battle_data->action;
    if (action->hit == 0) {
        return;
    }

    for (i = 0; i < 11; i++) {
        queued = 0;
        switch (i) {
        case 0:
            if (action->sp_change != 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x28;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->sp_change;
                queued = 1;
            }
            break;
        case 1:
            if (action->ct_change != 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x27;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->ct_change;
                queued = 1;
            }
            break;
        case 2:
            if (action->brave_change != 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x29;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->brave_change;
                queued = 1;
            }
            break;
        case 3:
            if (action->faith_change != 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x2a;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->faith_change;
                queued = 1;
            }
            break;
        case 4:
            if (action->pa_change != 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x2b;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->pa_change;
                queued = 1;
            }
            break;
        case 5:
            if (action->ma_change != 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x2c;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->ma_change;
                queued = 1;
            }
            break;
        case 6:
            if (action->gil_change > 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x2e;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->gil_change;
                queued = 1;
            }
            break;
        case 7:
            if (action->exp_change & 0x80) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x2f;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->exp_change;
                queued = 1;
            }
            break;
        case 8:
            if (action->special_effect & 0x180) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x2d;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value
                    = action->special_effect;
                queued = 1;
            }
            break;
        case 9:
            if (action->special_effect & 0x1004) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x1d;
                off = g_battle_action_post_effect_msg_counter;
                g_battle_action_post_effect_msgs[off].value = action->item_lost;
                if (action->special_effect & 0x1000) {
                    g_battle_action_post_effect_msgs[off].value = action->item_lost | 0x8000;
                }
                queued = 1;
            }
            break;
        case 10:
            if (action->special_effect & 0x10) {
                queued = 1;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x1c;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->item_lost;
            }
            break;
        }
        if (queued != 0) {
            g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].unit
                = misc->battle_data->misc_unit_id;
            g_battle_action_post_effect_msg_counter += 1;
        }
    }
}
