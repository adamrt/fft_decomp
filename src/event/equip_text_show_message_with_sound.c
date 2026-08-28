#include "fft/battle.h"
#include "fft/equip.h"
#include "psx/types.h"

void equip_text_show_message_with_sound(s32 message_id, s32 sound_id) {
    equip_thread_start_if_idle(1, 0x2B, message_id, 0);
    g_equip_sound_suppress_queued = 1;
    main_sound_play_sfx(sound_id);
    g_equip_text_message_thread_active = 1;
    g_event_mode = 1;
}
