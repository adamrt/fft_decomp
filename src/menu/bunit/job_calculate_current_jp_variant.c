#include "fft/event_bunit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Also returns the value: callers read it through g_menu_row_callbacks. */
s32 job_calculate_current_jp_variant(s32 index) {
    s32 generic_job;

    generic_job = job_get_generic_index(g_menu_job_ids[index]);
    return g_job_current_jp = g_bunit_unit_pointers[g_unit_selected_index]->job_points.current.values[generic_job];
}
