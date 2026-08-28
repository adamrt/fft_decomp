#include "fft/main_sound.h"
#include "psx/api.h"
#include "psx/etc.h"
#include "psx/types.h"

/* Pushes a MUS record onto the head of the active music list with the
 * root-counter event disabled. */
void main_smd_insert_music(suzuki_music_t* music) {
    DisableEvent(g_main_root_counter_2_event);
    music->next = g_main_sound_active_music_list;
    g_main_sound_active_music_list = music;
    EnableEvent(g_main_root_counter_2_event);
}
