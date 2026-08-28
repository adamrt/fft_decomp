#include "fft/wldcore.h"

/* Target 0x8006abc8. */
void wldcore_map_apply_clamped_scroll(wldcore_point32_t* amount, wldcore_point32_t* position) {
    wldcore_map_apply_clamped_horizontal_scroll(&amount->x, &position->x);
    wldcore_map_apply_clamped_vertical_scroll(amount, position);
}
