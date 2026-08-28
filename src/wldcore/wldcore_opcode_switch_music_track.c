#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_switch_music_track(void) {
    s16 track = g_wldcore_active_saved_record.music_selection;
    s32* state;
    s32 operand;

    if (track != 0) {
        state = &g_wldcore_audio_queue.music.flags;
        *state |= 1;
        operand = g_wldcore_active_saved_record.instruction.bytes.operand_0;
        main_sound_switch_music_track(track >> 8, 0, 0);
        main_sound_set_current_music_target(g_wldcore_audio_queue.volume, operand * 3);
    }
    g_wldcore_active_saved_record.state_flags |= 4;
}
