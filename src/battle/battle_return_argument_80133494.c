/* Two uncalled functions that return their argument. */

#include "psx/types.h"

/* Two identity functions back to back, at 0x80133494 and 0x8013349c. No code
 * calls either. */
s32 battle_return_argument_80133494(s32 value) {
    return value;
}

/* The second of the pair, also returning its argument. */
s32 battle_return_argument_8013349c(s32 value) {
    return value;
}
