#include "fft/suzuki.h"

/* SPU transfer-complete callback: clears the driver's read, write and
 * transfer-busy flags. */
void suzuki_sound_spu_transfer_callback(void) {
    g_suzuki_sound_driver_flags
        &= ~(SUZUKI_SOUND_DRIVER_SPU_READ | SUZUKI_SOUND_DRIVER_SPU_WRITE | SUZUKI_SOUND_DRIVER_SPU_TRANSFER);
}
