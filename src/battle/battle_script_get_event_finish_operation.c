#include "fft/battle.h"
#include "fft/event.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

u32 battle_script_get_event_finish_operation(void) {
    return (u32)(g_scenario_event_finish_operations[g_battle_next_event_id] & 0xF300) >> 8;
}
