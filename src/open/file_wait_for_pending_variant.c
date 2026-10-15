#include "fft/open.h"
#include "psx/types.h"

void file_wait_for_pending_variant(void) {
    u32* status = &g_main_file_still_loading;
    main_file_load_descriptor_t* header;

    if (*status == 0) {
        return;
    }

    /* The flag aliases state; a separate descriptor symbol changes the load scheduling. */
    header = (main_file_load_descriptor_t*)((u8*)status - offsetof(main_file_load_descriptor_t, state));
    do {
        main_file_poll_load(header);
        VSync(0);
    } while (g_main_file_still_loading != 0);
}
