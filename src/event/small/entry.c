/* SMALL.OUT: binding the battle name text sections and an unreferenced return-zero stub. */

#include "fft/script_variable.h"
#include "fft/text.h"
#include "psx/types.h"

/* Offsets of the two name sections in g_text_name_data. */
typedef struct text_name_section_offsets {
    u32 unit_names;
    u32 job_names;
} text_name_section_offsets_t;

extern u8 g_text_name_data[];
extern const text_name_section_offsets_t g_text_name_section_offsets;

/* SMALL.OUT's entry point, run once the overlay is loaded: point the roster-name and
 * job-name text sections at its own name data, copy the skillset-names pointer and
 * clear the deployment unit limit. */
void small_entrypoint(void) {
    u8* string_data = g_text_name_data;

    g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES] = string_data + g_text_name_section_offsets.unit_names;
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES] = string_data + g_text_name_section_offsets.job_names;
    g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES_COPY] = g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES];
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT, 0);
}

/* Unreferenced return-zero stub; its original role is unknown. */
s32 overlay_return_zero(void) {
    return 0;
}
