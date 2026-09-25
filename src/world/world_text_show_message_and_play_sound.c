#include "fft/world.h"
#include "psx/types.h"

void world_text_show_message_and_play_sound(s32 message_id, s32 sound_id) {
    world_text_start_character_thread_if_idle(1, 0x2B, message_id, 0);
    g_world_sound_release_held_loop = 1;
    main_sound_play_sfx_find_channel(sound_id);
    g_world_thread_task_active = 1;
}
