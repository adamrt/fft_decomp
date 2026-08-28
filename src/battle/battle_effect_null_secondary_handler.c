#include "fft/battle.h"
#include "fft/battle_effect.h"

/*
 * Secondary effect function 0x05: an erroneous
 * secondary animation that only steps its state so that a null animation can
 * finish cleanly.  Returns 1 to keep animating, 0 to stop.
 */
s32 battle_effect_null_secondary_handler(void) {
    s32 result;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch ((u32)g_battle_effect_current_secondary->phase) {
    case BATTLE_SECONDARY_EFFECT_INITIALIZING:
        g_battle_effect_current_secondary->phase = BATTLE_SECONDARY_EFFECT_EXECUTING;
        result = 1;
        break;
    case BATTLE_SECONDARY_EFFECT_EXECUTING:
        g_battle_effect_current_secondary->phase = BATTLE_SECONDARY_EFFECT_FINALIZING;
        result = 1;
        break;
    case BATTLE_SECONDARY_EFFECT_FINALIZING:
        result = 0;
        break;
    }
    return result;
}
