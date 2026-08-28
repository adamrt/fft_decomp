#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_fade_out_music(void) {
    s32* state;
    s32 operand;

    state = &g_wldcore_audio_queue.music.flags;
    *state &= ~1;
    operand = g_wldcore_active_saved_record.instruction.bytes.operand_0;
    main_sound_set_current_music_target(0, operand * 3);
    g_wldcore_active_saved_record.state_flags |= 4;
    g_wldcore_active_saved_record.music_selection &= 0xFF00;
}
