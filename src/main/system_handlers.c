/* Resident error hooks called by the file loader, heap and battle services. */

#include "fft/system.h"

/* Cleared by the error hooks; its original purpose and any reader are unknown. */
static s32 g_system_error_hook_word = 0;

/* Empty report hook. Callers pass a source number (0 from system_error_handle, 1 the effect
 * code, 7 the menu's system functions, 0x11 the crystal pickup menu) and a code. */
void system_report_stub(s32 category, s32 code) {
}

/* Empty error hook. Callers pass an id of the reporting subsystem and a code;
 * both are dropped. */
void system_error_report(s32 allocator_id, s32 error_code) {
}

/* The error hook of the file loads, the heap, the SMD loads and the battle sprite code. */
void system_error_handle(int error_code) {
    system_report_stub(0, error_code);
    g_system_error_hook_word = 0;
}

/* Error hook BATTLE calls, mostly when a unit lookup by id fails: the code is
 * dropped (system_error_report is empty) and
 * g_system_error_hook_word is cleared. */
void system_pointer_exception_handle(int error_code) {
    system_error_report(0, error_code);
    g_system_error_hook_word = 0;
}
