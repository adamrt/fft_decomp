#include "fft/battle.h"

u32* battle_script_get_controller_input_pointer(s32 use_current) {
    u32* result = (u32*)&g_battle_script_event_input;
    if (use_current != 0) {
        result = &g_battle_script_event_current_input;
    }
    return result;
}
