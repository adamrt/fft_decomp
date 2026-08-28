#include "fft/jobstts.h"

void jobstts_menu_clear_selection_record(s32 index) {
    memset(&g_jobstts_menu_selection_records[index], 0, sizeof(jobstts_ability_selection_state_t));
}
