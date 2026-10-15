#include "fft/battle_map.h"

/* 1 when the requested map state stands in the relation to a GNS row's state that
 * the row's comparison byte asks for: equal (1), requested less than the row's
 * (2) or requested greater (3). Any other comparison never matches. Every file
 * row of the retail GNS files uses 1. */
s16 map_is_gns_record_matching_state(s32 comparison, s16 record_state, s16 requested_state) {
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
