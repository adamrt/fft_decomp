#include "fft/menu.h"
#include "fft/open.h"

/* Padded pixel size of the window image for a text entry: 10 pixels per column
 * plus 24, rounded down to a multiple of 4, and 16 pixels per row plus 16. The
 * WLDCORE twin wldcore_text_get_padded_dimensions measures the text in pixels
 * instead. Nothing in OPEN.BIN calls it. */
void open_text_measure_menu_image(s32 combined_text_index, size32_t* dimensions) {
    s16 columns;
    s16 rows;

    text_measure(&columns, &rows, text_find_entry(combined_text_index));
    dimensions->width = ((columns * 10) + 24) & 0xfffc;
    dimensions->height = (rows << 4) + 16;
}
