#include "fft/battle.h"
#include "psx/types.h"

void battle_script_apply_relative_camera(u8* parameters, s32* src) {
    u8* dst;
    s32 i;
    s32 delta;
    s32 val;

    dst = parameters + 0xF;
    if (parameters[0xE] == 0x38) {
        dst = parameters + 0x12;
    }
    for (i = 0; i < 7; i++) {
        delta = *src;
        if (i < 3) {
            delta = delta / 1024;
        }
        if (battle_script_load_halfword(parameters) == 10000) {
            val = 10000;
        } else {
            val = delta + battle_script_load_halfword(parameters);
        }
        battle_script_store_halfword(dst, val);
        parameters += 2;
        dst += 2;
        src++;
    }
}
