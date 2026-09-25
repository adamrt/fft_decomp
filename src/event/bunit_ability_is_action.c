#include "fft/event_bunit.h"
#include "psx/types.h"

/* Action abilities occupy type zero in the ability-pointer classification. */
s32 bunit_ability_is_action(void) {
    return g_bunit_ability_type == ABILITY_TYPE_DEFAULT;
}
