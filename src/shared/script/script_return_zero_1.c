#include "fft/script.h"
#include "psx/types.h"

/* Returns 0 in BATTLE and WORLD; in BATTLE it sits between
 * gfx_set_draw_mode_from_origin and script_noop_10. MAIN calls BATTLE's copy
 * through main_overlay_call_battle_return_zero while script variable 0xff is
 * set. Nothing else references it; the name is a neutral placeholder until a
 * role turns up. */
s32 script_return_zero_1(void) {
    return 0;
}
