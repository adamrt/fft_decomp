#include "fft/equip.h"
#include "psx/types.h"

s32 equip_unit_has_two_hands(s16 unit_id) {
    return g_equip_unit_data[unit_id]->support_sets_3 & 0x02;
}
