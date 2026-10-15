#include "fft/event_require.h"
#include "fft/main.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Load bonus image `image_id` (a 0x6800-byte file, 13 sectors apart from
 * sector 0x16c0) and upload it to VRAM.
 *
 * Waits until 0x8000 bytes can be reserved at the high overlay address, reads
 * the file there on the main stack, uploads the pixels and the palette that
 * follows them, and frees the buffer after one more frame.
 */
void require_reward_load_bonus_image(s32 image_id) {
    require_reward_bonus_image_file_t* allocation;
    s32 (*build_operation)(s32 sector, u32 size, void* destination);

    do {
        thread_yield();
        allocation = main_heap_reserve_at(g_main_heap_high_overlay_load_address, 0x8000);
    } while (allocation != g_main_heap_high_overlay_load_address);

    build_operation = main_file_call_build_header;
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))build_operation;
    } while (
        thread_call_on_main_stack(image_id * 13 + DISC_SECTOR_EVENT_BONUS_BIN, sizeof(*allocation), allocation) != 0);

    {
        int (*poll_operation)(void) = main_file_is_still_loading;
        do {
            thread_yield();
            g_thread_call_target = (void (*)(void))poll_operation;
        } while (thread_call_on_main_stack() != 0);
    }

    /* The 256-row upload includes the reserved suffix beyond the 0x6800-byte file. */
    LoadImage(&g_require_reward_bonus_image_rect, allocation->pixels[0]);
    LoadImage(&g_require_reward_bonus_palette_rect, (u32*)allocation->palettes);
    thread_yield();
    main_heap_free(allocation);
}
