#include "fft/world.h"
#include "psx/types.h"

/* Show message `message_id` in the text thread (dialog type 0x2B), play
 * `sound_id` at once and mark a text task active. */
void world_text_show_message_and_play_sound(s32 message_id, s32 sound_id) {
    menu_text_start_thread_if_idle(THREAD_SCREEN_SLOT_TEXT, 0x2B, message_id, 0);
    g_world_sound_release_held_loop = 1;
    main_sound_play_sfx_find_channel(sound_id);
    g_menu_input_lock_mode = 1;
}
