#include "psx/types.h"

/* battle_effect_run_block stops the block on this result. */
enum { EFFECT_SCRIPT_END = 2 };

s32 battle_effect_code_script_04_end(void) {
    return EFFECT_SCRIPT_END;
}
