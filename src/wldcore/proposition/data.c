/* Packed proposition records and the conditions that make a job available at a bar. */

#include "fft/calendar.h"
#include "fft/proposition.h"
#include "fft/script_variable.h"
#include "psx/types.h"

extern s32* g_proposition_data; /* byte-offset directory of the embedded proposition data */

/* Return the address of proposition data record `record`: the data block starts with a table of
 * byte offsets from its own start. */
void* proposition_data_get(proposition_data_record_e record) {
    return (u8*)g_proposition_data + g_proposition_data[record];
}

/* Expand one 23-byte proposition row into 16-bit fields, then remap the
 * two fee fields through the 16-bit table of record 1. */
void proposition_unpack_row(proposition_fields_t* out, s32 index) {
    u8* rows = proposition_data_get(PROPOSITION_DATA_ROWS);
    u16* field_lookup;
    s32 i;
    u16* fields = &out->category_index;

    for (i = 0; i < 23; i++) {
        fields[i] = rows[index * 23 + i];
    }
    field_lookup = proposition_data_get(PROPOSITION_DATA_FEES);
    out->fee_a = field_lookup[out->fee_a];
    out->fee_b = field_lookup[out->fee_b];
}

/* Whether proposition `index` is offered at `location`: the row's location
 * (0 for any), then each condition its flags select: a shop stock level, the
 * current zodiac sign, or another proposition (condition_value) completed. */
s32 proposition_is_available_at(s32 index, s32 location) {
    proposition_fields_t entry;
    s32 month;
    s32 day;

    proposition_unpack_row(&entry, index);
    if (entry.location != 0) {
        if (location != entry.location) {
            return 0;
        }
    }
    if (entry.condition_flags & PROPOSITION_CONDITION_SHOP_AVAILABILITY) {
        if (script_variable_get(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) < entry.required_shop_availability) {
            return 0;
        }
    }
    if (entry.condition_flags & PROPOSITION_CONDITION_ZODIAC_SIGN) {
        month = script_variable_get(EVENT_SCRIPT_VAR_MONTH);
        day = script_variable_get(EVENT_SCRIPT_VAR_DAY);
        calendar_date_to_zodiac_convert(&month, &day);
        if (month != entry.condition_value) {
            return 0;
        }
    }
    if (entry.condition_flags & PROPOSITION_CONDITION_STATUS) {
        if ((script_variable_get(entry.condition_value + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
                & PROPOSITION_VARIABLE_COMPLETED)
            == 0) {
            return 0;
        }
    }
    return 1;
}
