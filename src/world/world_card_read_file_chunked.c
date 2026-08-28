/* `result` is pinned to $17 with a tied asm after the open call: the target
 * copies that result into s1 and tests s1, while plain C lets copy
 * propagation test $v0 directly and drop the `move s1,v0`.
 * The shared `fail:` tail reproduces the target's cross-jumped `j; li v0,-1`
 * block at 0x64, which all three -1 returns reach. */
#include "fft/card.h"
#include "fft/world.h"
#include "psx/api.h"
#include "psx/types.h"

s32 world_card_read_file_chunked(const char* filename, u8* destination, u32 size) {
    register s32 result __asm__("$17");
    u32 total;
    s32 i;

    result = world_card_open_file_with_retries(filename, 1);
    __asm__("" : "=r"(result) : "0"(result));
    /* Both failures share the -1 return below; separate returns get laid
       out as separate blocks. */
    if (result < 0) {
        goto fail;
    }
    if (FileGetError(g_world_card_open_descriptor) != 0) {
        world_card_close_file_with_retries(g_world_card_open_descriptor);
    fail:
        return -1;
    }
    total = 0;
    for (i = 0; i < 0x28; i++) {
        if (world_card_seek_file_with_retries(g_world_card_open_descriptor, total, 1) < 0) {
            break;
        }
        result = FileRead(g_world_card_open_descriptor, destination + total, size - total);
        if (result != -1) {
            total += result;
            if (total >= size) {
                break;
            }
        }
    }
    if (world_card_close_file_with_retries(g_world_card_open_descriptor) == 0) {
        goto fail;
    }
    return total;
}
