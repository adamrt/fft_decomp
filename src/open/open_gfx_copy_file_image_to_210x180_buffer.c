#include "fft/open.h"

#define OPEN_IMAGE_COPY_WIDTH  210
#define OPEN_IMAGE_COPY_HEIGHT 180

/* Provisional view of the file loaded at g_open_file_destination: the image
 * pixels, 256 per row, start at 0x5600. */
typedef struct {
    u8 unknown_00[0x5600];
    u16 pixels[1];
} open_image_file_t;

void open_gfx_copy_file_image_to_210x180_buffer(s32 file_index) {
    u16* copy_source;
    u16* copy_destination;
    u16* file_pixels;
    s32 row;
    s32 source_row;
    s32 column;
    s32 source_offset;
    s32 destination_index;
    s32 source_index;
    s32 destination_offset;
    u16* pixels;
    u16 value;

    copy_source = (u16*)g_open_work_buffer_0;
    copy_destination = (u16*)g_open_work_buffer_1;
    file_pixels = ((open_image_file_t*)g_open_file_destination)->pixels;
    g_open_gfx_image_copy_source_buffer = copy_source;
    g_open_gfx_image_copy_destination_buffer = copy_destination;
    g_open_gfx_image_copy_file_pixels = file_pixels;
    open_file_build_openbk_header(file_index + 2, g_open_work_buffer_0);

    row = 0;
    source_row = 0;
    do {
        column = 0;
        source_offset = row << 8;
        do {
            destination_index = source_row + column;
            source_index = source_offset + column;
            column++;
            destination_index <<= 1;
            pixels = g_open_gfx_image_copy_file_pixels;
            source_index <<= 1;
            /* index + (u32)pointer: the retail addu sums index first. */
            value = *(u16*)(source_index + (u32)pixels);
            *(u16*)(destination_index + (u32)g_open_gfx_image_copy_destination_buffer) = value;
        } while (column < OPEN_IMAGE_COPY_WIDTH);
        g_open_gfx_image_copy_row_flags[row + 2] = 0;
        row++;
        source_row += OPEN_IMAGE_COPY_WIDTH;
    } while (row < OPEN_IMAGE_COPY_HEIGHT);

    g_open_gfx_image_copy_row_flags[0] = 0;
    g_open_gfx_image_copy_row_flags[1] = 0;
}
