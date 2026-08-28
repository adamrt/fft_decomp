#include "fft/battle_ai.h"

/*
 * Dispatch the acting unit's special automatic-battle behavior.
 *
 * A result of -2 chains recovery into retreat, or fading-life behavior into
 * recovery. All dispatched routines otherwise collapse success to 1 while
 * preserving the suspended result -1.
 */
s32 battle_ai_handle_autobattle(void) {
    s32 result;
    /* Pin: unpinned, GCC routes the result through $a0 and loses the $v0 delay-slot fills (4 bytes larger). */
    register s32 direct_result __asm__("$2");

    /* A -2 result falls back to another policy's case; those jumps stay gotos. */
    switch (g_battle_ai_data_base.autobattle_setting) {
    case BATTLE_AI_AUTOBATTLE_TARGETED:
        direct_result = battle_ai_select_targeted_action();
        if (direct_result != -1)
            return 1;
        return -1;
    case BATTLE_AI_AUTOBATTLE_RECOVERY:
    case_e:
        result = battle_ai_select_recovery_action();
        if (result == -1) {
            return -1;
        }
        if (result == -2) {
            goto case_11;
        }
        return 1;
    case BATTLE_AI_AUTOBATTLE_FADING_LIFE:
        result = battle_ai_save_fading_life_auto_battle();
        if (result == -1) {
            return -1;
        }
        if (result == -2) {
            goto case_e;
        }
        return 1;
    case BATTLE_AI_AUTOBATTLE_RETREAT:
    case_11:
        direct_result = battle_ai_select_retreat_action();
        if (direct_result != -1)
            return 1;
        return -1;
    default:
        return 0;
    }
}
