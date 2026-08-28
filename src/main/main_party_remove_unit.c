#include "fft/data.h"
#include "psx/types.h"

void main_party_remove_unit(u32 index) {
    g_main_party_data[index].party_id = PARTY_ID_NONE;
}
