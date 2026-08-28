#include "fft/world.h"

typedef struct open_text_image_dimensions_t {
    s32 width;
    s32 height;
} open_text_image_dimensions_t;

void open_text_measure_menu_image(s32 combined_text_index, open_text_image_dimensions_t* dimensions) {
    s16 columns;
    s16 rows;
    const u8* text;

    text = world_text_find_entry(combined_text_index);
    world_text_measure(&columns, &rows, text);
    dimensions->width = ((columns * 10) + 24) & 0xfffc;
    dimensions->height = (rows << 4) + 16;
}
