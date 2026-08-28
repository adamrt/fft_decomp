#include "fft/battle.h"
#include "psx/types.h"

/* The colour deltas are passed as sign-extended words; the u16 prototype
 * would zero-extend them, so this caller uses an all-s32 view.
 * The definition itself only matches with u16 colour parameters (s16 or s32
 * ones change its code), so the two views stay. */
#define MODIFY_PALETTE_WORDS ((void (*)(s32, s32, s32, s32, s32, s32, s32, s32))battle_map_modify_palette)

void battle_map_color_field(s32 first, s32 second, s16 red, s16 green, s16 blue) {
    MODIFY_PALETTE_WORDS(first, second, 0, 0, 1, red, green, blue);
}
