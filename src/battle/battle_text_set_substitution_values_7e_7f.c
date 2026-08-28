#include "fft/battle.h"
#include "psx/types.h"

/* Stores four halfwords into text variables $7E and $7F.
 *
 * The target writes each word as two independent halfword stores. */
void battle_text_set_substitution_values_7e_7f(s32 x, s32 y, s32 width, s32 height) {
    ((s16*)&g_battle_text_substitution_values[0x7e])[0] = x;
    ((s16*)&g_battle_text_substitution_values[0x7e])[1] = y;
    ((s16*)&g_battle_text_substitution_values[0x7f])[0] = width;
    ((s16*)&g_battle_text_substitution_values[0x7f])[1] = height;
}
