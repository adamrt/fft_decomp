#include "fft/wldcore.h"

party_data_t* wldcore_get_party_data_pointer(s32 party_index) {
    return main_party_get_data_pointer(party_index);
}
