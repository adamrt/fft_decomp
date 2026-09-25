#include "fft/wldcore.h"
#include "psx/types.h"

/* Proposition record as copied out by wldcore_unpack_proposition_row.  Only the
 * fields this check reads are named:
 * 0x20 is compared against the current map location, 0x22 selects which of
 * the remaining conditions apply, 0x2c doubles as the required month (bit 1)
 * and the proposition status index (bit 2, script variable 0x360 + n). */
typedef struct wldcore_proposition_entry {
    u8 _unused_00[0x20];
    u16 location;
    u16 condition_flags;
    u8 _unused_24[6];
    u16 required_script_var_6f;
    u16 condition_value;
    u16 _unused_2e;
} wldcore_proposition_entry_t;

enum {
    WLDCORE_PROPOSITION_CONDITION_SCRIPT_VAR_6F = 1,
    WLDCORE_PROPOSITION_CONDITION_MONTH = 2,
    WLDCORE_PROPOSITION_CONDITION_STATUS = 4,
};

s32 wldcore_proposition_set_based_on_location(s32 index, s32 location) {
    wldcore_proposition_entry_t entry;
    s32 month;
    s32 day;

    wldcore_unpack_proposition_row((wldcore_proposition_fields_t*)&entry, index);
    if (entry.location != 0) {
        if (location != entry.location) {
            return 0;
        }
    }
    if (entry.condition_flags & WLDCORE_PROPOSITION_CONDITION_SCRIPT_VAR_6F) {
        if (world_script_get_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) < entry.required_script_var_6f) {
            return 0;
        }
    }
    if (entry.condition_flags & WLDCORE_PROPOSITION_CONDITION_MONTH) {
        month = world_script_get_variable(EVENT_SCRIPT_VAR_MONTH);
        day = world_script_get_variable(EVENT_SCRIPT_VAR_DAY);
        wldcore_convert_date_to_zodiac_date(&month, &day);
        if (month != entry.condition_value) {
            return 0;
        }
    }
    if (entry.condition_flags & WLDCORE_PROPOSITION_CONDITION_STATUS) {
        if ((world_script_get_variable(entry.condition_value + 0x360) & 4) == 0) {
            return 0;
        }
    }
    return 1;
}
