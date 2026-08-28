#include "fft/battle.h"
#include "fft/battle_text.h"
#include "psx/types.h"

/* Rectangle of packed nibbles inside a larger image: the first four fields are
 * the origin and extent in nibbles, the last the image's row stride. */
typedef struct battle_nibble_region {
    s16 index;  /* 0x0 */
    s16 row;    /* 0x2 */
    s16 width;  /* 0x4 */
    s16 height; /* 0x6 */
    s32 stride; /* 0x8 */
} battle_nibble_region_t;

/* Mirrors the region horizontally, swapping each nibble with its opposite. */
void battle_text_mirror_packed_nibble_region_horizontal(u8* data, battle_nibble_region_t* region) {
    s32 row;
    s32 col;
    s32 value;
    s32 offset;

    for (row = 0; row < region->height; row++) {
        for (col = 0; col < region->width / 2; col++) {
            value = battle_text_read_packed_nibble(data, region->index + col, region->row + row, region->stride);
            offset = 1;
            battle_write_packed_nibble(data, region->index + col, region->row + row, region->stride,
                battle_text_read_packed_nibble(
                    data, region->index + region->width - (col + offset), region->row + row, region->stride));
            battle_write_packed_nibble(
                data, region->index + region->width - (col + offset), region->row + row, region->stride, value);
        }
    }
}
