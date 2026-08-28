#include "fft/bunit.h"
#include "psx/types.h"

/* Reaction abilities occupy category one. */
s32 bunit_ability_is_reaction_tab(void) {
    return g_bunit_ability_category == 1;
}
