/* Sound-novel completion counts the saved read-section flags using the retail completion divisor. */

#include "fft/bits.h"
#include "fft/sound_novel.h"
#include "psx/types.h"

/* Count the set bits among the first 0x1000 read-section bits and return that
 * count as a percentage of 76, the number retail treats as complete. */
s32 sound_novel_read_percent_calculate(void) {
    s32 count = 0;
    s32 i;

    for (i = 0; i < 0x1000; i++) {
        if (bits_test(g_sound_novel_read_section_bits, i) != 0) {
            count++;
        }
    }
    return count * 100 / 76;
}
