#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/require.h"
#include "psx/types.h"

void require_reward_load_bonus_image(s32 image_id) {
    void* allocation;
    s32 (*build_operation)();

    do {
        battle_thread_yield();
        allocation = main_heap_reserve_at(g_main_heap_high_overlay_load_address, 0x8000);
    } while (allocation != g_main_heap_high_overlay_load_address);

    build_operation = main_file_call_build_header;
    do {
        battle_thread_yield();
        g_battle_thread_call_target = (void (*)(void))build_operation;
    } while (battle_thread_call_on_main_stack(image_id * 13 + 0x16c0, 0x6800, allocation) != 0);

    {
        s32 (*poll_operation)() = main_file_is_still_loading;
        do {
            battle_thread_yield();
            g_battle_thread_call_target = (void (*)(void))poll_operation;
        } while (battle_thread_call_on_main_stack() != 0);
    }

    LoadImage((RECT*)g_require_reward_bonus_image_rect, (u32*)allocation);
    /* The file stores 200 rows of 64 halfwords (0x6400 bytes) of pixels, then the palette. */
    LoadImage(&g_require_reward_bonus_palette_rect, (u32*)((u8*)allocation + 0x6400));
    battle_thread_yield();
    main_heap_free(allocation);
}
