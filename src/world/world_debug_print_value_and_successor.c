#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Target 0x800e4d50. */
void world_debug_print_value_and_successor(s32 value) {
    char* fmt = &g_world_text_decimal_format[0];

    FntPrint(fmt, value);
    value = value + 1;
    FntPrint(fmt, value);
}
