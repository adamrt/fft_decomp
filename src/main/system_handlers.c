/* Resident error hooks called by the file loader, heap and battle services. */

#include "fft/sys/system.h"

/* Cleared by the error hooks; its original purpose and any reader are unknown. */
static s32 s_system_error_hook_word = 0;

/* Empty report hook. Callers pass a source number (0 from system_handle_error, 1 the effect
 * code, 7 the menu's system functions, 0x11 the crystal pickup menu) and a code. */
void system_report(s32 category, s32 code) {
}

/* Empty error hook. Callers pass an id of the reporting subsystem and a code;
 * both are dropped. */
void system_report_error(s32 allocator_id, s32 error_code) {
}

/* The error hook of the file loads, the heap, the SMD loads and the battle sprite code. */
void system_handle_error(int error_code) {
    system_report(0, error_code);
    s_system_error_hook_word = 0;
}

/* Error hook BATTLE calls, mostly when a unit lookup by id fails: the code is
 * dropped (system_report_error is empty) and
 * g_system_error_hook_word is cleared. */
void system_handle_pointer_exception(int error_code) {
    system_report_error(0, error_code);
    s_system_error_hook_word = 0;
}
