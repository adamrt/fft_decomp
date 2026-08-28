#include "fft/equip.h"

s32 equip_bits_read_primary(s32 count) {
    s32 result;
    u8* cursor;
    s32 bit;
    u8 byte;

    if (count == 0) {
        return 0;
    }
    if (g_equip_bits_reader_1_reset != 0) {
        g_equip_bits_reader_1_index = 7;
        g_equip_bits_reader_1_reset = 0;
    }
    result = 0;
    for (count = count - 1; count != -1; count--) {
        cursor = g_equip_bits_reader_1_stream;
        byte = *cursor;
        bit = g_equip_bits_reader_1_index;
        g_equip_bits_reader_1_index = bit - 1;
        if ((byte >> bit) & 1) {
            result |= 1 << count;
        }
        if (g_equip_bits_reader_1_index < 0) {
            g_equip_bits_reader_1_index = 7;
            g_equip_bits_reader_1_stream = cursor + 1;
        }
    }
    return result;
}
