#include "fft/open.h"
#include "psx/types.h"

void open_bin_decdctoutcallback(void* callback) {
    DMACallback(1, callback);
}
