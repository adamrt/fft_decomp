#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_play_sound(void) {
    main_sound_play_sfx_find_channel(g_wldcore_active_saved_record.instruction.bytes.operand_0);
    g_wldcore_active_saved_record.state_flags |= 4;
}
