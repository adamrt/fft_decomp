#include "fft/map.h"

s32 battle_map_is_gns_record_matching_state(s32 comparison, s16 record_state, s16 requested_state) {
    s32 matches;

    comparison &= 0xff;
    matches = 0;
    switch (comparison) {
    case GNS_MAP_STATE_EQUAL: {
        if (requested_state == record_state) {
            matches = 1;
        }
        break;
    }
    case GNS_MAP_STATE_REQUESTED_LESS_THAN_RECORD: {
        if (requested_state < record_state) {
            matches = 1;
        }
        break;
    }
    case GNS_MAP_STATE_REQUESTED_GREATER_THAN_RECORD: {
        if (requested_state > record_state) {
            matches = 1;
        }
        break;
    }
    }
    return matches;
}
