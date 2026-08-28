#include "fft/open.h"
#include "psx/types.h"

#define OPEN_IMAGE_WIDTH             210
#define OPEN_IMAGE_HEIGHT            180
#define OPEN_IMAGE_ROW_STRIDE        256
#define OPEN_IMAGE_BLEND_BAND_HEIGHT 17
#define OPEN_IMAGE_BLEND_END_ROW     (OPEN_IMAGE_HEIGHT + OPEN_IMAGE_BLEND_BAND_HEIGHT)

/*
 * Advance the bottom-up crossfade between the two 210x180 opening images by one row.
 *
 * Each call blends the 17-row band ending at the progress row: every row in the
 * band steps its own fade counter and writes the per-component mix of the source
 * and destination pixels into the file image.
 *
 * The reverse row is computed inside the row test, which makes it local to that
 * block. GCC 2.7.2's local-alloc orders exactly three block quantities with a
 * fixed-index comparison that would allocate the first half of the *210 multiply
 * first; with four it sorts by priority and the second half gets $v0, as in the
 * target. The s16 components are copied for the compare and the difference,
 * while the sums use the originals.
 */
void open_gfx_step_image_crossfade(void) {
    s32 row;
    s32 last_row;
    s32 reverse_row;
    s32 column;
    s32 source_row_start;
    s32 destination_row_start;
    u16 source_pixel;
    u16 destination_pixel;
    u16 pixel;
    s32 component;
    s16 source_component;
    s16 destination_component;
    s32 previous_components;
    s32 difference;
    u8 fade;

    if (g_open_gfx_image_copy_row_flags[0] != 0) {
        row = g_open_gfx_image_copy_progress - OPEN_IMAGE_BLEND_BAND_HEIGHT;
        if (row < g_open_gfx_image_copy_progress) {
            last_row = OPEN_IMAGE_HEIGHT - 1;
            do {
                if (row >= 0 && row < OPEN_IMAGE_HEIGHT) {
                    reverse_row = last_row - row;
                    column = 0;
                    source_row_start = reverse_row * OPEN_IMAGE_WIDTH;
                    destination_row_start = reverse_row * OPEN_IMAGE_ROW_STRIDE;
                    fade = g_open_gfx_image_copy_row_flags[row + 2]++;
                    do {
                        source_pixel = g_open_gfx_image_copy_source_buffer[source_row_start + column];
                        destination_pixel = g_open_gfx_image_copy_destination_buffer[source_row_start + column];
                        pixel = 0;
                        if (source_pixel != destination_pixel) {
                            for (component = 2; component >= 0; component--) {
                                previous_components = pixel >> 5;
                                source_component = source_pixel & 0x1F;
                                destination_component = destination_pixel & 0x1F;
                                if (source_component != destination_component) {
                                    difference = source_component - destination_component;
                                    pixel = previous_components
                                        | ((destination_component + (s16)((s16)(difference << 8) / 16 * fade) / 256)
                                            << 10);
                                } else {
                                    pixel = previous_components | (source_component << 10);
                                }
                                source_pixel >>= 5;
                                destination_pixel >>= 5;
                            }
                        } else {
                            pixel = source_pixel;
                        }
                        g_open_gfx_image_copy_file_pixels[destination_row_start + column]
                            = pixel | (destination_pixel & 0x8000);
                        column++;
                    } while (column < OPEN_IMAGE_WIDTH);
                }
                row++;
            } while (row < g_open_gfx_image_copy_progress);
        }
        {
            u8* progress = &g_open_gfx_image_copy_progress;

            if (++*progress >= OPEN_IMAGE_BLEND_END_ROW) {
                g_open_gfx_image_copy_row_flags[0] = 0;
            }
        }
    }
}
