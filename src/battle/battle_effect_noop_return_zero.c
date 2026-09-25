#include "fft/battle.h"

/* Effect-table no-op callback; the zero return selects the normal path. */
s32 battle_effect_noop_return_zero(void) {
    main_noop_800449f8(1, 0x320);
    return 0;
}
