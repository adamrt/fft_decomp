/* SCUS_942.21 0x8001a014..0x8001a523. */
#include "psx/libspu.h"

/* Whole-record assignment replaces the retail byte copy with 32-bit copies. */
static inline void psyq_spu_copy_reverb_parameters(
    psyq_spu_reverb_parameters_t* destination, const psyq_spu_reverb_parameters_t* source) {
    u8* destination_bytes = (u8*)destination;
    const u8* source_bytes = (const u8*)source;
    u32 size = sizeof(*destination);
    while (size--)
        *destination_bytes++ = *source_bytes++;
}

s32 SpuSetReverbModeParam(SpuReverbAttr* attr) {
    psyq_spu_reverb_parameters_t parameters;
    s32 clear_work = 0;
    s32 was_enabled = 0;
    s32 mode_changed = 0;
    s32 delay_changed = 0;
    s32 feedback_changed = 0;
    u32 mask = attr->mask;
    s32 all = mask == 0;
    u32 mode;
    s32 delay;
    s32 delay_a;
    s32 delay_b;
    s32 feedback;
    u16 control;
    parameters.mask = 0;
    if (all || (mask & SPU_REV_MODE)) {
        mode = attr->mode;
        if (mode & SPU_REV_MODE_CLEAR_WA) {
            mode &= ~SPU_REV_MODE_CLEAR_WA;
            clear_work = 1;
        }
        if (mode >= PSYQ_SPU_REVERB_MODE_COUNT || _SpuIsInAllocateArea_(_spu_rev_startaddr[mode])) {
            return -1;
        }
        mode_changed = 1;
        _spu_rev_attr.mode = mode;
        _spu_rev_offsetaddr = _spu_rev_startaddr[_spu_rev_attr.mode];
        psyq_spu_copy_reverb_parameters(&parameters, &_spu_rev_param[_spu_rev_attr.mode]);
        switch (_spu_rev_attr.mode) {
        case SPU_REV_MODE_ECHO:
            _spu_rev_attr.feedback = 127;
            _spu_rev_attr.delay = 127;
            break;
        case SPU_REV_MODE_DELAY:
            _spu_rev_attr.feedback = 0;
            _spu_rev_attr.delay = 127;
            break;
        default:
            _spu_rev_attr.feedback = 0;
            _spu_rev_attr.delay = 0;
            break;
        }
    }
    if (all || (mask & SPU_REV_DELAYTIME)) {
        switch (_spu_rev_attr.mode) {
        case SPU_REV_MODE_ECHO:
        case SPU_REV_MODE_DELAY:
            delay_changed = 1;
            if (!mode_changed) {
                psyq_spu_copy_reverb_parameters(&parameters, &_spu_rev_param[_spu_rev_attr.mode]);
                parameters.mask = 0x0c011c00;
            }
            delay = attr->delay;
            _spu_rev_attr.delay = delay;
            delay_a = (delay << 13) / 127;
            delay_b = (delay << 12) / 127;
            parameters.registers.same_reflection1_left = delay_a - parameters.registers.apf1_delay;
            parameters.registers.same_reflection1_right = delay_b - parameters.registers.apf2_delay;
            parameters.registers.comb1_left = delay_b + parameters.registers.comb1_right;
            parameters.registers.same_reflection2_left = delay_b + parameters.registers.same_reflection2_right;
            parameters.registers.apf1_left = delay_b + parameters.registers.apf2_left;
            parameters.registers.apf1_right = delay_b + parameters.registers.apf2_right;
            break;
        default:
            _spu_rev_attr.delay = 0;
            break;
        }
    }
    if (all || (mask & SPU_REV_FEEDBACK)) {
        switch (_spu_rev_attr.mode) {
        case SPU_REV_MODE_ECHO:
        case SPU_REV_MODE_DELAY:
            feedback_changed = 1;
            if (!mode_changed) {
                if (!delay_changed) {
                    psyq_spu_copy_reverb_parameters(&parameters, &_spu_rev_param[_spu_rev_attr.mode]);
                    parameters.mask = 0x80;
                } else {
                    parameters.mask |= 0x80;
                }
            }
            feedback = attr->feedback;
            _spu_rev_attr.feedback = feedback;
            parameters.registers.reflection_volume2 = ((feedback * 129) << 8) / 127;
            break;
        default:
            _spu_rev_attr.feedback = 0;
            break;
        }
    }
    if (mode_changed) {
        was_enabled = (_spu_RXX->rxx.control >> 7) & 1;
        if (was_enabled) {
            control = _spu_RXX->rxx.control;
            control &= 0xff7f;
            _spu_RXX->rxx.control = control;
        }
    }
    if (!mode_changed) {
        if (all || (mask & SPU_REV_DEPTHL)) {
            _spu_RXX->rxx.reverb_depth_left = attr->depth.left;
            _spu_rev_attr.depth.left = attr->depth.left;
        }
        if (all || (mask & SPU_REV_DEPTHR)) {
            _spu_RXX->rxx.reverb_depth_right = attr->depth.right;
            _spu_rev_attr.depth.right = attr->depth.right;
        }
    } else {
        _spu_RXX->rxx.reverb_depth_left = 0;
        _spu_RXX->rxx.reverb_depth_right = 0;
        _spu_rev_attr.depth.left = 0;
        _spu_rev_attr.depth.right = 0;
    }
    if (mode_changed || delay_changed || feedback_changed) {
        _spu_setReverbAttr(&parameters);
    }
    if (clear_work) {
        SpuClearReverbWorkArea(_spu_rev_attr.mode);
    }
    if (mode_changed) {
        _spu_FsetRXX(PSYQ_SPU_REG_REVERB_START, _spu_rev_offsetaddr, PSYQ_SPU_REGISTER_RAW);
        if (was_enabled) {
            control = _spu_RXX->rxx.control;
            control |= 0x80;
            _spu_RXX->rxx.control = control;
        }
    }
    return 0;
}
