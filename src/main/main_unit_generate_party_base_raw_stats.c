#include "fft/main.h"

/* The target leaves a1 intact while advancing a0 to the raw-stat field. */
void main_unit_generate_party_base_raw_stats(party_data_t* party, s32 unit_type) {
    main_unit_generate_base_raw_stats(party->raw_stats, unit_type);
}
