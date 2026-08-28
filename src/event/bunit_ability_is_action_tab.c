#include "fft/bunit.h"
#include "psx/types.h"

/* The action-ability tab is category zero. */
s32 bunit_ability_is_action_tab(void) {
    return g_bunit_ability_category == 0;
}
