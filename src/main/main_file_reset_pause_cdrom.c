#include "fft/main_file.h"
#include "psx/cd.h"

void main_file_reset_pause_cdrom(main_file_load_descriptor_t* state) {
    state->state = MAIN_FILE_LOAD_STATE_IDLE;
    CdFlush();
    while (CdControlb(9, 0, 0) == 0) { }
    VSync(3);
}
