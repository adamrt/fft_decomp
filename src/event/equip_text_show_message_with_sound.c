#include "fft/event_equip.h"
#include "psx/types.h"

/* Show message `message_id` in the text thread (dialog type 0x2B), play
 * `sound_id` at once with the queued sound suppressed, and lock the menu
 * input. */
void equip_text_show_message_with_sound(s32 message_id, s32 sound_id) {
    menu_text_start_thread_if_idle(THREAD_SCREEN_SLOT_TEXT, 0x2B, message_id, 0);
    g_equip_sound_suppress_queued = 1;
    main_sound_play_sfx(sound_id);
    g_equip_text_message_thread_active = 1;
    g_menu_input_lock_mode = 1;
}
