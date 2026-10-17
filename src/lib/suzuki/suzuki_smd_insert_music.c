#include "fft/suzuki.h"
#include "psx/libapi.h"

/* Pushes a MUS record onto the head of the active music list with the
 * root-counter event disabled. */
void suzuki_smd_insert_music(suzuki_music_t* music) {
    DisableEvent(g_suzuki_sound_root_counter_2_event);
    music->next = g_suzuki_sound_active_music_list;
    g_suzuki_sound_active_music_list = music;
    EnableEvent(g_suzuki_sound_root_counter_2_event);
}
