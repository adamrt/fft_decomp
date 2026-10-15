#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

/* Return the after-event code of the pending event (g_battle_next_event_id): bits
 * 8-9 and 12-15 of its 16-bit finish-operation value, shifted down by 8. Zero means the
 * event has none. */
u32 script_get_event_finish_operation(void) {
    return (g_main_scenario_event_finish_operations[g_script_next_event_id] & 0xF300) >> 8;
}
