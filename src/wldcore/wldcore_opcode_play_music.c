#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_play_music(void) {
    wldcore_sound_change_scenario_music(g_wldcore_active_saved_record.instruction.bytes.operand_0);
    g_wldcore_active_saved_record.state_flags |= 4;
}
