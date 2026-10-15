#include "fft/event_small.h"
#include "fft/script.h"

/* Bind the roster and job name sections to the overlay's name data, alias
 * the skillset-name copy to the skillset names and zero the deployment unit
 * limit. */
void small_text_init_battle_pointers(void) {
    u8* string_data;

    string_data = g_small_text_name_data;
    g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES]
        = string_data + g_small_text_name_section_offsets.unit_names;
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES] = string_data + g_small_text_name_section_offsets.job_names;
    g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES_COPY] = g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES];
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT, 0);
}
