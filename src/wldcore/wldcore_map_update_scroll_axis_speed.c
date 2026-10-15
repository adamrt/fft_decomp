#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

void wldcore_map_update_scroll_axis_speed(wldcore_axis_state_t* axis, s32 direction) {
    switch (direction) {
    case 1:
        if (axis->value < axis->limit) {
            axis->value = axis->value + axis->step;
        }
        if (axis->value > axis->limit) {
            axis->value = axis->limit;
        }
        axis->output = axis->value >> 8;
        break;
    case 0:
        if (axis->value == 0) {
            break;
        }
        if (axis->value > 0) {
            axis->value = axis->value - axis->decay;
            if (axis->value <= 0) {
                axis->value = 0;
            }
            axis->output = axis->value >> 8;
        } else {
            axis->value = axis->value + axis->decay;
            if (axis->value >= 0) {
                axis->value = 0;
            }
            axis->output = -((-axis->value) >> 8);
        }
        break;
    case -1:
        if (-axis->value < axis->limit) {
            axis->value = axis->value - axis->step;
        }
        if (-axis->value > axis->limit) {
            axis->value = -axis->limit;
        }
        axis->output = -((-axis->value) >> 8);
        break;
    }
}
