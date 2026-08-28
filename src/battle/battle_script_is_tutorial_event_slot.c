#include "fft/script_variables.h"
#include "psx/types.h"

s32 battle_script_is_tutorial_event_slot(void) {
    return (u32)(battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) - 0x19A) < 0x10U;
}
