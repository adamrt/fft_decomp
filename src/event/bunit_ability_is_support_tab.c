#include "fft/bunit.h"
#include "psx/types.h"

/* Support abilities occupy category two. */
s32 bunit_ability_is_support_tab(void) {
    return g_bunit_ability_category == 2;
}
