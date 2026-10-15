#include "fft/main.h"
#include "psx/libcd.h"

void main_file_reset_cd_subsystems(void) {
    CdReset(0);
}
