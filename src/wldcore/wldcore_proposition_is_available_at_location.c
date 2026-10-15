#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Whether proposition `index` is offered at `location`: the row's location
 * (0 for any), then each condition its flags select. The status condition
 * tests bit 2 of script variable 0x360 + condition_value. */
s32 wldcore_proposition_is_available_at_location(s32 index, s32 location) {
    wldcore_proposition_fields_t entry;
    s32 month;
    s32 day;

    wldcore_unpack_proposition_row(&entry, index);
    if (entry.location != 0) {
        if (location != entry.location) {
            return 0;
        }
    }
    if (entry.condition_flags & WLDCORE_PROPOSITION_CONDITION_SCRIPT_VAR_6F) {
        if (script_get_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) < entry.required_script_var_6f) {
            return 0;
        }
    }
    if (entry.condition_flags & WLDCORE_PROPOSITION_CONDITION_MONTH) {
        month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
        day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
        wldcore_calendar_convert_date_to_zodiac(&month, &day);
        if (month != entry.condition_value) {
            return 0;
        }
    }
    if (entry.condition_flags & WLDCORE_PROPOSITION_CONDITION_STATUS) {
        if ((script_get_variable(entry.condition_value + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
                & WLDCORE_PROPOSITION_VARIABLE_COMPLETED)
            == 0) {
            return 0;
        }
    }
    return 1;
}
