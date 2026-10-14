#include "fft/map.h"
#include "psx/types.h"

/* Returns 0 in BATTLE and WORLD; in BATTLE it sits between
 * menu_poll_small_load and map_data_load_complete_thread. Nothing on the disc
 * calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
s32 map_return_zero_1(void) {
    return 0;
}
