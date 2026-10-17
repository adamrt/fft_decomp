/* SCUS_942.21 0x8001a524..0x8001a9f3. */
#include "psx/libspu.h"

void _spu_setReverbAttr(psyq_spu_reverb_parameters_t* parameters) {
    u32 mask = parameters->mask;
    s32 all = mask == 0;
    if (all || (mask & 0x1)) {
        _spu_RXX->rxx.reverb.apf1_delay = parameters->registers.apf1_delay;
    }
    if (all || (mask & 0x2)) {
        _spu_RXX->rxx.reverb.apf2_delay = parameters->registers.apf2_delay;
    }
    if (all || (mask & 0x4)) {
        _spu_RXX->rxx.reverb.reflection_volume1 = parameters->registers.reflection_volume1;
    }
    if (all || (mask & 0x8)) {
        _spu_RXX->rxx.reverb.comb1_volume = parameters->registers.comb1_volume;
    }
    if (all || (mask & 0x10)) {
        _spu_RXX->rxx.reverb.comb2_volume = parameters->registers.comb2_volume;
    }
    if (all || (mask & 0x20)) {
        _spu_RXX->rxx.reverb.comb3_volume = parameters->registers.comb3_volume;
    }
    if (all || (mask & 0x40)) {
        _spu_RXX->rxx.reverb.comb4_volume = parameters->registers.comb4_volume;
    }
    if (all || (mask & 0x80)) {
        _spu_RXX->rxx.reverb.reflection_volume2 = parameters->registers.reflection_volume2;
    }
    if (all || (mask & 0x100)) {
        _spu_RXX->rxx.reverb.apf1_volume = parameters->registers.apf1_volume;
    }
    if (all || (mask & 0x200)) {
        _spu_RXX->rxx.reverb.apf2_volume = parameters->registers.apf2_volume;
    }
    if (all || (mask & 0x400)) {
        _spu_RXX->rxx.reverb.same_reflection1_left = parameters->registers.same_reflection1_left;
    }
    if (all || (mask & 0x800)) {
        _spu_RXX->rxx.reverb.same_reflection1_right = parameters->registers.same_reflection1_right;
    }
    if (all || (mask & 0x1000)) {
        _spu_RXX->rxx.reverb.comb1_left = parameters->registers.comb1_left;
    }
    if (all || (mask & 0x2000)) {
        _spu_RXX->rxx.reverb.comb1_right = parameters->registers.comb1_right;
    }
    if (all || (mask & 0x4000)) {
        _spu_RXX->rxx.reverb.comb2_left = parameters->registers.comb2_left;
    }
    if (all || (mask & 0x8000)) {
        _spu_RXX->rxx.reverb.comb2_right = parameters->registers.comb2_right;
    }
    if (all || (mask & 0x10000)) {
        _spu_RXX->rxx.reverb.same_reflection2_left = parameters->registers.same_reflection2_left;
    }
    if (all || (mask & 0x20000)) {
        _spu_RXX->rxx.reverb.same_reflection2_right = parameters->registers.same_reflection2_right;
    }
    if (all || (mask & 0x40000)) {
        _spu_RXX->rxx.reverb.different_reflection1_left = parameters->registers.different_reflection1_left;
    }
    if (all || (mask & 0x80000)) {
        _spu_RXX->rxx.reverb.different_reflection1_right = parameters->registers.different_reflection1_right;
    }
    if (all || (mask & 0x100000)) {
        _spu_RXX->rxx.reverb.comb3_left = parameters->registers.comb3_left;
    }
    if (all || (mask & 0x200000)) {
        _spu_RXX->rxx.reverb.comb3_right = parameters->registers.comb3_right;
    }
    if (all || (mask & 0x400000)) {
        _spu_RXX->rxx.reverb.comb4_left = parameters->registers.comb4_left;
    }
    if (all || (mask & 0x800000)) {
        _spu_RXX->rxx.reverb.comb4_right = parameters->registers.comb4_right;
    }
    if (all || (mask & 0x1000000)) {
        _spu_RXX->rxx.reverb.different_reflection2_left = parameters->registers.different_reflection2_left;
    }
    if (all || (mask & 0x2000000)) {
        _spu_RXX->rxx.reverb.different_reflection2_right = parameters->registers.different_reflection2_right;
    }
    if (all || (mask & 0x4000000)) {
        _spu_RXX->rxx.reverb.apf1_left = parameters->registers.apf1_left;
    }
    if (all || (mask & 0x8000000)) {
        _spu_RXX->rxx.reverb.apf1_right = parameters->registers.apf1_right;
    }
    if (all || (mask & 0x10000000)) {
        _spu_RXX->rxx.reverb.apf2_left = parameters->registers.apf2_left;
    }
    if (all || (mask & 0x20000000)) {
        _spu_RXX->rxx.reverb.apf2_right = parameters->registers.apf2_right;
    }
    if (all || (mask & 0x40000000)) {
        _spu_RXX->rxx.reverb.input_volume_left = parameters->registers.input_volume_left;
    }
    if (all || (mask & 0x80000000)) {
        _spu_RXX->rxx.reverb.input_volume_right = parameters->registers.input_volume_right;
    }
}
