#include "fft/wldcore.h"

/* Target 0x8006abc8. */
void wldcore_map_apply_clamped_scroll(point32_t* amount, VECTOR* position) {
    wldcore_map_apply_clamped_horizontal_scroll(&amount->x, &position->vx);
    wldcore_map_apply_clamped_vertical_scroll(amount, position);
}
