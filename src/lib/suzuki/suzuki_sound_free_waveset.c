#include "fft/suzuki.h"

/* Unlinks a loaded waveset and frees its SPU RAM and heap header; does
 * nothing if it is not in g_suzuki_sound_waveset_list. */
void suzuki_sound_free_waveset(suzuki_waveset_t* waveset) {
    suzuki_waveset_t* node;
    suzuki_waveset_t* previous = 0;

    node = g_suzuki_sound_waveset_list;
    while (node != 0) {
        if (node == waveset) {
            break;
        }
        previous = node;
        node = node->next;
    }
    if (node != 0) {
        suzuki_sound_free_spu_ram(waveset->spu_address);
        if (previous != 0) {
            previous->next = waveset->next;
        } else {
            g_suzuki_sound_waveset_list = waveset->next;
        }
        suzuki_sound_free(waveset);
    }
}
