#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/world.h"
#include "psx/types.h"

/* Rectangle of packed nibbles inside a larger image: the first four fields are
 * the origin and extent in nibbles, the last the image's row stride. */
typedef world_gfx_4bpp_rect_t battle_nibble_region_t;

/* Mirrors the region horizontally, swapping each nibble with its opposite. */
void battle_text_mirror_packed_nibble_region_horizontal(u8* data, battle_nibble_region_t* region) {
    s32 row;
    s32 col;
    s32 value;
    s32 offset;

    for (row = 0; row < region->h; row++) {
        for (col = 0; col < region->w / 2; col++) {
            value = battle_text_read_packed_nibble(data, region->x + col, region->y + row, region->stride);
            offset = 1;
            battle_write_packed_nibble(data, region->x + col, region->y + row, region->stride,
                battle_text_read_packed_nibble(
                    data, region->x + region->w - (col + offset), region->y + row, region->stride));
            battle_write_packed_nibble(
                data, region->x + region->w - (col + offset), region->y + row, region->stride, value);
        }
    }
}
