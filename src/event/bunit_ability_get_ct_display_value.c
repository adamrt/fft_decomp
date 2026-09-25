#include "fft/event_bunit.h"
#include "psx/types.h"

s32 bunit_ability_get_ct_display_value(s32 index) {
    s32 ability;
    s32 result;

    ability = g_bunit_ability_entries[index];
    result = 0x20000000;

    if ((ability & ABILITY_LIST_ENTRY_HIDE_DETAILS) == 0) {
        s32 quotient;
        s32 remainder;

        result = ((ability_secondary_data_t*)g_bunit_ability_selected_specific)->ct;
        quotient = 100 / result;
        remainder = 100 % result;
        result = quotient + (remainder != 0);
    }
    {
        /* The (s16) keeps the lh as one value; without it combine splits it into lhu/sll/sra and adds a frame. */
        s32 ability_type = (s16)ability >> ABILITY_LIST_ENTRY_DISABLED_SHIFT;

        if (ability_type != 0) {
            result |= 0x40000000;
        }
    }
    return result;
}
