#include "fft/main.h"
#include "psx/types.h"

/* Allocates a game-heap buffer for a TIM of `size` bytes at disc `sector`,
 * queues the read and blocks until the file has landed.  Twin of
 * main_file_get_smd, which draws from the SMD allocator instead. */
void* main_file_get_tim(s32 sector, s32 size) {
    void* destination;
    main_file_load_descriptor_t* header;

    destination = main_heap_alloc(size);
    if (destination == 0) {
        main_system_handle_animation_exception(1);
    }
    while (main_file_build_header_nnl(&g_main_file_cd_state, sector, (s32)((u32)size >> 11), destination) != 0) {
        VSync(0);
        main_noop_800449ec();
        main_file_poll_load(&g_main_file_cd_state);
    }
    if (g_main_file_still_loading != 0) {
        header = &g_main_file_cd_state;
        do {
            VSync(0);
            main_noop_800449ec();
            main_file_poll_load(header);
        } while (g_main_file_still_loading != 0);
    }
    return destination;
}
