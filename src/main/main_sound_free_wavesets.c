#include "fft/main.h"
#include "psx/types.h"

/* Frees every loaded waveset whose id is 0x20 or above, keeping the
 * resident low-id sets. */
void main_sound_free_wavesets(void) {
    suzuki_waveset_t* waveset;
    suzuki_waveset_t* node;

    node = g_main_sound_waveset_list;
    while (node != 0) {
        waveset = node;
        node = node->next;
        if (waveset->id >= 0x20) {
            main_sound_free_waveset(waveset);
        }
    }
}
