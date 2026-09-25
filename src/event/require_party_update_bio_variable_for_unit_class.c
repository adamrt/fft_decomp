#include "fft/event_require.h"
#include "psx/types.h"

/* Three unit ids whose presence selects the event variable at +6. */
typedef struct require_party_bio_update_record {
    u16 unit_ids[3];
    u16 variable;
} require_party_bio_update_record_t;

extern require_party_bio_update_record_t g_require_party_bio_update_records[11];

void require_party_update_bio_variable_for_unit_class(s32 unit_class, s32 value) {
    s32 i;
    s32 j;

    for (i = 0; i < 11; i++) {
        for (j = 0; j < 3; j++) {
            if (unit_class == g_require_party_bio_update_records[i].unit_ids[j]) {
                break;
            }
        }
        if (j != 3) {
            battle_script_set_variable(g_require_party_bio_update_records[i].variable, value);
        }
    }
}
