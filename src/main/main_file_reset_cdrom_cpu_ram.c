#include "fft/main.h"
#include "psx/types.h"

void main_file_reset_cdrom_cpu_ram(void) {
    SetMem(2);
    CdInit();
    CdSetDebug(0);
    main_file_reset_pause_cdrom(&g_main_file_cd_state);
    g_main_file_still_loading = 0;
}
