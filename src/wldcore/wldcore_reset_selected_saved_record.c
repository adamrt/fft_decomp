#include "fft/wldcore.h"

void wldcore_reset_selected_saved_record(void) {
    wldcore_reset_saved_record_fields(&g_main_saved_records[g_wldcore_saved_record_index]);
}
