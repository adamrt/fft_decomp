/* SCUS_942.21 0x8001b094..0x8001b427; tables 0x8001008c..0x800100cb. */
#include "psx/libspu.h"

void SpuSetCommonAttr(SpuCommonAttr* attr) {
    s16 left = 0;
    s16 right = 0;
    u32 mask = attr->mask;
    s32 all = mask == 0;
    u32 mode;
    u16 control;
    if (all || (mask & SPU_COMMON_MVOLL)) {
        if (all || (mask & SPU_COMMON_MVOLMODEL)) {
            switch (attr->mvolmode.left) {
            case SPU_VOICE_LINEARIncN:
                mode = 0x8000;
                break;
            case SPU_VOICE_LINEARIncR:
                mode = 0x9000;
                break;
            case SPU_VOICE_LINEARDecN:
                mode = 0xa000;
                break;
            case SPU_VOICE_LINEARDecR:
                mode = 0xb000;
                break;
            case SPU_VOICE_EXPIncN:
                mode = 0xc000;
                break;
            case SPU_VOICE_EXPIncR:
                mode = 0xd000;
                break;
            case SPU_VOICE_EXPDec:
                mode = 0xe000;
                break;
            case SPU_VOICE_DIRECT:
                left = attr->mvol.left;
                mode = 0;
                break;
            default:
                left = attr->mvol.left;
                mode = 0;
                break;
            }
        } else {
            left = attr->mvol.left;
            mode = 0;
        }
        if (mode != 0) {
            if (attr->mvol.left >= 128) {
                left = 127;
            } else if (attr->mvol.left < 0) {
                left = 0;
            } else {
                left = attr->mvol.left;
            }
        }
        _spu_RXX->rxx.main_volume_left = (left & 0x7fff) | mode;
    }
    if (all || (mask & SPU_COMMON_MVOLR)) {
        if (all || (mask & SPU_COMMON_MVOLMODER)) {
            switch (attr->mvolmode.right) {
            case SPU_VOICE_LINEARIncN:
                mode = 0x8000;
                break;
            case SPU_VOICE_LINEARIncR:
                mode = 0x9000;
                break;
            case SPU_VOICE_LINEARDecN:
                mode = 0xa000;
                break;
            case SPU_VOICE_LINEARDecR:
                mode = 0xb000;
                break;
            case SPU_VOICE_EXPIncN:
                mode = 0xc000;
                break;
            case SPU_VOICE_EXPIncR:
                mode = 0xd000;
                break;
            case SPU_VOICE_EXPDec:
                mode = 0xe000;
                break;
            case SPU_VOICE_DIRECT:
                right = attr->mvol.right;
                mode = 0;
                break;
            default:
                right = attr->mvol.right;
                mode = 0;
                break;
            }
        } else {
            right = attr->mvol.right;
            mode = 0;
        }
        if (mode != 0) {
            if (attr->mvol.right >= 128) {
                right = 127;
            } else if (attr->mvol.right < 0) {
                right = 0;
            } else {
                right = attr->mvol.right;
            }
        }
        _spu_RXX->rxx.main_volume_right = (right & 0x7fff) | mode;
    }
    if (all || (mask & SPU_COMMON_CDVOLL)) {
        _spu_RXX->rxx.cd_volume_left = attr->cd.volume.left;
    }
    if (all || (mask & SPU_COMMON_CDVOLR)) {
        _spu_RXX->rxx.cd_volume_right = attr->cd.volume.right;
    }
    if (all || (mask & SPU_COMMON_EXTVOLL)) {
        _spu_RXX->rxx.external_volume_left = attr->ext.volume.left;
    }
    if (all || (mask & SPU_COMMON_EXTVOLR)) {
        _spu_RXX->rxx.external_volume_right = attr->ext.volume.right;
    }
    if (all || (mask & SPU_COMMON_CDREV)) {
        if (attr->cd.reverb == SPU_OFF) {
            control = _spu_RXX->rxx.control;
            control &= 0xfffb;
            _spu_RXX->rxx.control = control;
        } else {
            control = _spu_RXX->rxx.control;
            control |= 4;
            _spu_RXX->rxx.control = control;
        }
    }
    if (all || (mask & SPU_COMMON_CDMIX)) {
        if (attr->cd.mix == SPU_OFF) {
            control = _spu_RXX->rxx.control;
            control &= 0xfffe;
            _spu_RXX->rxx.control = control;
        } else {
            control = _spu_RXX->rxx.control;
            control |= 1;
            _spu_RXX->rxx.control = control;
        }
    }
    if (all || (mask & SPU_COMMON_EXTREV)) {
        if (attr->ext.reverb == SPU_OFF) {
            control = _spu_RXX->rxx.control;
            control &= 0xfff7;
            _spu_RXX->rxx.control = control;
        } else {
            control = _spu_RXX->rxx.control;
            control |= 8;
            _spu_RXX->rxx.control = control;
        }
    }
    if (all || (mask & SPU_COMMON_EXTMIX)) {
        if (attr->ext.mix == SPU_OFF) {
            control = _spu_RXX->rxx.control;
            control &= 0xfffd;
            _spu_RXX->rxx.control = control;
        } else {
            control = _spu_RXX->rxx.control;
            control |= 2;
            _spu_RXX->rxx.control = control;
        }
    }
}
