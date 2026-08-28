#include "fft/battle.h"

void battle_move_interpolate_height_toward_fixed_drop(battle_unit_misc_data_t* misc) {
    s32 step;
    s32 target;
    s32 height;

    step = misc->distortion_timer;
    target = misc->distortion_target;
    switch (misc->distortion_phase) {
    case 0:
        step = -0x24000 / step;
        target = misc->real.vy + -0x24000;
        misc->distortion_phase = 1;
    case 1:
        if (target < misc->real.vy) {
            misc->real.vy = step + misc->real.vy;
        } else {
            misc->real.vy = target;
            misc->distortion_animation_id = 0;
        }
        height = misc->real.vy;
        misc->screen.vy = height / ONE;
        break;
    }
    misc->distortion_timer = step;
    misc->distortion_target = target;
}
