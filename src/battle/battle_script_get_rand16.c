#include "fft/battle.h"

/* The arithmetic runner consumes $v0 at 0x8014a078. Return the masked seed;
 * a byte-matching void definition alone did not establish this interface. */
s32 battle_script_get_rand16(void) {
    return g_battle_script_rand16_state = ((u32)g_battle_script_rand16_state * 0x6255U + 0x3619U) & 0xffffU;
}
