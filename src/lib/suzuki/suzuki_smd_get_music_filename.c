#include "fft/suzuki.h"

/* Returns the file name stored in the SMD data of a queued MUS record, or of
 * the most recently queued record when music is 0; 0 if it is not queued. */
char* suzuki_smd_get_music_filename(suzuki_music_t* music) {
    suzuki_music_t* entry = g_suzuki_sound_active_music_list;

    if (music != 0) {
        while (entry != 0) {
            if (entry == music) {
                break;
            }
            entry = entry->next;
        }
    }
    if (entry == 0) {
        return 0;
    }
    return (char*)entry->smd + entry->smd->filename_offset;
}
