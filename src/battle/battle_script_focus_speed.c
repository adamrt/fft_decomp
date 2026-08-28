#include "fft/battle.h"
#include "psx/gte.h"
#include "psx/types.h"

/* FocusSpeed event instruction: derives the camera move duration from the
 * distance between the current and requested position/rotation and the speed.
 *
 * Twin of world_script_focus_speed without the division check. rotation_sum
 * is genuinely uninitialised in the target (s6 is never set). */
void battle_script_focus_speed(u8* parameters, s32* position, s32* rotation) {
    s32 position_delta[4]; /* four slots: the target spaces the arrays 0x10 apart */
    s32 rotation_delta[4];
    s32 speed;
    s32 i;
    s32 value;
    s32 position_sum;
    s32 rotation_sum;

    speed = battle_script_load_halfword(parameters);
    /* Skip the speed halfword and the byte after it. */
    parameters += 2;
    parameters++;
    position_sum = 0;
    for (i = 0; i < 3; i++) {
        value = battle_script_load_halfword(parameters + i * 2);
        if (value == 10000) {
            position_delta[i] = 0;
        } else {
            position_delta[i] = value - position[i] / 1024;
        }
        value = battle_script_load_halfword(parameters + i * 2 + 6);
        if (value == 10000) {
            rotation_delta[i] = 0;
        } else {
            rotation_delta[i] = value - rotation[i];
        }
        position_sum += position_delta[i] * position_delta[i];
        rotation_sum += rotation_delta[i] * rotation_delta[i];
    }
    if (position_sum * 2 < rotation_sum) {
        position_sum = rotation_sum / 2;
    }
    position_sum = SquareRoot0(position_sum) / speed;
    if (position_sum == 0) {
        position_sum = 1;
    }
    battle_script_store_halfword(parameters + 14, position_sum);
}
