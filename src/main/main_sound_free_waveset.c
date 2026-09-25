#include "fft/main.h"
#include "psx/types.h"

/* Unlinks a loaded waveset and frees its SPU RAM and heap header; does
 * nothing if it is not in g_main_sound_waveset_list. */
void main_sound_free_waveset(suzuki_waveset_t* waveset) {
    suzuki_waveset_t* node;
    suzuki_waveset_t* previous;

    previous = 0;
    node = g_main_sound_waveset_list;
    while (node != 0) {
        if (node == waveset) {
            break;
        }
        previous = node;
        node = node->next;
    }
    if (node != 0) {
        main_sound_free_spu_ram(waveset->spu_address);
        if (previous != 0) {
            previous->next = waveset->next;
        } else {
            g_main_sound_waveset_list = waveset->next;
        }
        main_sound_free(waveset);
    }
}
