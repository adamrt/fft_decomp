#include "fft/equip.h"
#include "psx/types.h"

s32 equip_unit_has_two_swords(s16 unit_id) {
    return ((u8*)g_equip_unit_data[unit_id])[0x76] & 0x01;
}
