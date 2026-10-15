#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

void* wldcore_proposition_get_data_pointer(wldcore_proposition_data_record_e record) {
    return (u8*)g_wldcore_proposition_data_base + g_wldcore_proposition_data_base[record];
}
