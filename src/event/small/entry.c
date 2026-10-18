/* SMALL.OUT: binding the battle name text sections and an unreferenced return-zero stub. */

#include "fft/script_variable.h"
#include "fft/text.h"
#include "psx/types.h"

/* Indices in this name file's directory, distinct from the runtime text section slots. */
enum { NAME_TEXT_SECTION_ROSTER_UNIT_NAMES = 1, NAME_TEXT_SECTION_JOB_NAMES = 2 };

/* The 32-entry offset directory precedes g_text_name_data. Only the two name sections are bound here. */
extern const u32 g_text_name_section_offsets[TEXT_SECTION_COUNT];
extern u8 g_text_name_data[];

/* SMALL.OUT's entry point, run once the overlay is loaded: point the roster-name and
 * job-name text sections at its own name data, copy the skillset-names pointer and
 * clear the deployment unit limit. */
void small_entrypoint(void) {
    u8* string_data = g_text_name_data;

    g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES]
        = string_data + g_text_name_section_offsets[NAME_TEXT_SECTION_ROSTER_UNIT_NAMES];
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES]
        = string_data + g_text_name_section_offsets[NAME_TEXT_SECTION_JOB_NAMES];
    g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES_COPY] = g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES];
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT, 0);
}

/* Unreferenced return-zero stub; its original role is unknown. */
s32 overlay_return_zero(void) {
    return 0;
}
