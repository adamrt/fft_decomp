/* Battle text pointer init and a zero return. */

#include "fft/event_small.h"
#include "fft/menu.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "psx/types.h"

/* SMALL.OUT's entry point, run once the overlay is loaded: point the roster-name and
 * job-name text sections at its own name data, copy the skillset-names pointer and
 * clear the deployment unit limit. */
void small_text_init_battle_pointers(void) {
    u8* string_data = g_small_text_name_data;

    g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES]
        = string_data + g_small_text_name_section_offsets.unit_names;
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES] = string_data + g_small_text_name_section_offsets.job_names;
    g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES_COPY] = g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES];
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT, 0);
}

/* Returns 0; nothing on the disc calls it. */
s32 small_return_zero(void) {
    return 0;
}
