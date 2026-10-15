#include "fft/menu.h"
#include "fft/wldcore.h"

/* Padded width and height, not position.
 * The mask is written 0xfffc (not ~3) to keep the target's `andi`. */
void wldcore_text_get_padded_dimensions(s32 value, size32_t* output) {
    s16 maximum_line_width;
    s16 line_count;

    text_measure_pixels(&maximum_line_width, &line_count, text_find_entry(value));
    output->width = (maximum_line_width + 24) & 0xfffc;
    output->height = line_count * 16 + 16;
}
