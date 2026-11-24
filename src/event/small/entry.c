/* SMALL.OUT: binding the battle name text sections and an unreferenced return-zero stub. */

#include "fft/script/script_variable.h"
#include "fft/text/text.h"
#include "psx/types.h"

/* SMALL.OUT's entry point, run once the overlay is loaded: point the roster-name and
 * job-name text sections at its own name data, copy the skillset-names pointer and
 * clear the deployment unit limit. */
void small_entrypoint(void) {
    u8* text = g_name_text_bank.text;

    g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES]
        = text + g_name_text_bank.section_offsets[NAME_TEXT_SECTION_UNIT_NAMES];
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES]
        = text + g_name_text_bank.section_offsets[NAME_TEXT_SECTION_JOB_NAMES];
    g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES_COPY] = g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES];
    script_variable_set(SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT, 0);
}

/* Unreferenced return-zero stub; its original role is unknown. */
s32 overlay_return_zero(void) {
    return 0;
}
