#include "fft/battle.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_script_apply_input_overrides(u32* input) {
    u32 high_buttons;

    if (*input == (PSX_PAD_START | PSX_PAD_SELECT | PSX_PAD_L1 | PSX_PAD_R1)) {
        main_system_reset_game();
    }

    high_buttons = *input & 0xffff0000;
    *input &= battle_script_get_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) ^ 0xffff;
    *input = high_buttons | (battle_script_get_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS) | *input);
}
