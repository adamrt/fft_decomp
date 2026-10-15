#include "fft/open.h"

#define OPEN_IMAGE_COPY_WIDTH  210
#define OPEN_IMAGE_COPY_HEIGHT 180

/* Prepare the crossfade of the new-game prologue's picture to OPNBK image
 * file_index + 2.
 *
 * Points the crossfade at its three buffers, starts the asynchronous read of the
 * new image into g_open_work_buffer_0 (the crossfade's source) and copies the
 * 210x180 picture on screen out of the frame image into g_open_work_buffer_1,
 * packed 210 pixels per row. It clears the per-row fade steps, the running flag
 * (row_flags[0]) and the progress row (row_flags[1], the same byte as
 * g_open_gfx_image_copy_progress); image operation 0x20 of a later transition
 * entry waits for the read and sets the running flag. */
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
    u16* pixels;
    u16 value;
    u16(*file_image)[256];

    copy_source = (u16*)g_open_work_buffer_0;
    copy_destination = (u16*)g_open_work_buffer_1;
    file_image = g_open_file_destination;
    /* The 210x180 picture begins at row 43 of the 256-pixel frame image. */
    file_pixels = file_image[43];
    g_open_gfx_image_copy_source_buffer = copy_source;
    g_open_gfx_image_copy_destination_buffer = copy_destination;
    g_open_gfx_image_copy_file_pixels = file_pixels;
    open_file_build_openbk_header(file_index + 2, g_open_work_buffer_0);

    row = 0;
    source_row = 0;
    for (; row < OPEN_IMAGE_COPY_HEIGHT; row++) {
        column = 0;
        source_offset = row << 8;
        while (column < OPEN_IMAGE_COPY_WIDTH) {
            destination_index = source_row + column;
            source_index = source_offset + column++;
            destination_index <<= 1;
            pixels = g_open_gfx_image_copy_file_pixels;
            value = pixels[source_index];
            /* Destination indexing hoists pointer loads; byte-pointer arithmetic changes registers. */
            *(u16*)(destination_index + (u32)g_open_gfx_image_copy_destination_buffer) = value;
        }
        g_open_gfx_image_copy_row_flags[row + 2] = 0;
        source_row += OPEN_IMAGE_COPY_WIDTH;
    }

    g_open_gfx_image_copy_row_flags[0] = 0;
    g_open_gfx_image_copy_row_flags[1] = 0;
}
