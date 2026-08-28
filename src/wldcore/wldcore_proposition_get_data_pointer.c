#include "fft/wldcore.h"

wldcore_proposition_data_t* wldcore_proposition_get_data_pointer(s32 proposition_index) {
    s32* table = g_wldcore_proposition_data_base;

    return (wldcore_proposition_data_t*)((u8*)table + table[proposition_index]);
}
