#include "fft/open.h"
#include "psx/types.h"

void open_file_wait_for_pending(void) {
    u32* status = &g_main_file_still_loading;
    main_file_load_descriptor_t* header;

    if (*status == 0) {
        return;
    }

    /* The loading flag aliases the descriptor's state field. */
    header = (main_file_load_descriptor_t*)((u8*)status - sizeof(header->unknown_00));
    do {
        main_file_poll_load(header);
        VSync(0);
    } while (g_main_file_still_loading != 0);
}
