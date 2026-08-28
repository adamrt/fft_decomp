#include "fft/wldcore.h"

void wldcore_opcode_stop_music_and_play_sound(void) {
    char unused[8];
    s32 sound_id;

    sound_id = g_wldcore_active_saved_record.instruction.bytes.operand_0;
    if (g_wldcore_opcode_state.instruction.word & 0xFF0000) {
        g_wldcore_active_saved_record.sound_id = sound_id | 0x4000;
    }
    wldcore_sound_stop_music_and_play_sound(sound_id);
    g_wldcore_active_saved_record.state_flags |= 4;
}
