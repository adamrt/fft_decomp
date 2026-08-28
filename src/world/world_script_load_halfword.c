#include "fft/event.h"

s16 world_script_load_halfword(const u8* source) {
    return (s16)(source[0] | (source[1] << 8));
}
