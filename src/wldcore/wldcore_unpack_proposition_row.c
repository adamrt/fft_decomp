#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Expand one 23-byte proposition row into 16-bit fields, then remap the
 * two fee fields through the 16-bit table of record 1. */
void wldcore_unpack_proposition_row(wldcore_proposition_fields_t* out, s32 index) {
    u8* rows;
    u16* field_lookup;
    s32 i;
    u16* fields;

    rows = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_ROWS);
    fields = &out->category_index;
    for (i = 0; i < 23; i++) {
        fields[i] = rows[index * 23 + i];
    }
    field_lookup = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_FEES);
    out->fee_a = field_lookup[out->fee_a];
    out->fee_b = field_lookup[out->fee_b];
}
