#include "fft/bunit.h"
#include "psx/types.h"

void bunit_job_calculate_current_jp(s32 index) {
    s32 generic_job;
    u8* unit;

    generic_job = bunit_job_get_generic_index(g_bunit_job_ids[index]);
    unit = (u8*)g_bunit_unit_data[g_bunit_unit_selected_index];
    g_bunit_job_current_jp = *(u16*)(unit + 0xbc + generic_job * 2);
}
