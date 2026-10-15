#include "fft/main.h"
#include "psx/libcd.h"

void main_file_reset_pause_cdrom(main_file_load_descriptor_t* state) {
    state->state = MAIN_FILE_LOAD_STATE_IDLE;
    CdFlush();
    while (CdControlB(CdlPause, 0, 0) == 0) { }
    VSync(3);
}
