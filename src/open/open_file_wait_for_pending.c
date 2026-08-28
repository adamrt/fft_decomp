#include "fft/main_file.h"
#include "fft/open.h"
#include "psx/types.h"

void open_file_wait_for_pending(void) {
    s32* status = &g_main_file_still_loading;
    void* header;

    if (*status == 0) {
        return;
    }

    header = (u8*)status - 4;
    do {
        main_file_poll_load(header);
        VSync(0);
    } while (g_main_file_still_loading != 0);
}
