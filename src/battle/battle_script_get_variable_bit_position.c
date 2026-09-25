#include "fft/battle.h"

s32 battle_script_get_variable_bit_position(s32 value) {
    s32 result;

    result = -1;
    if (value >= 0x80) {
        if (value < 0x360) {
            result = value & 0x1f;
        } else if (value < 0x400) {
            result = (value & 7) * 4;
        } else {
            battle_thread_exit_current();
        }
    }
    return result;
}
