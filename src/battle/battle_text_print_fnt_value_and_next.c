#include "fft/battle.h"
#include "psx/types.h"

/* Append value and value + 1 to the active Psy-Q font stream. */
void battle_text_print_fnt_value_and_next(s32 value) {
    FntPrint(g_battle_text_decimal_format, value);
    value++;
    FntPrint(g_battle_text_decimal_format, value);
}
