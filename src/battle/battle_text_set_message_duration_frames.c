#include "fft/battle_text.h"
#include "psx/types.h"

/* Frames the battle message/dialogue wait loop will hold before it fakes a
 * confirm press. Callers pass 0x3C (1s) or 0xB4 (3s). */
void battle_text_set_message_duration_frames(u16 frames) {
    g_battle_text_message_duration_frames = frames;
}
