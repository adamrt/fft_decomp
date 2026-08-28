#include "fft/battle.h"

s16 battle_script_load_halfword(const u8* data) {
    return (s16)(data[0] | (data[1] << 8));
}
