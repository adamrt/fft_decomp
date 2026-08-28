#include "fft/event.h"
#include "fft/main_runtime.h"
#include "fft/world.h"
#include "psx/types.h"

u32 world_script_get_scenario_finish_operation_flags(void) {
    return (u32)(g_scenario_event_finish_operations[g_world_finish_operation_event_id] & 0xF300) >> 8;
}
