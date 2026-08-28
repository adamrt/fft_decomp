#include "fft/open.h"
#include "psx/types.h"

void open_bin_decdctincallback(void* callback) {
    DMACallback(0, callback);
}
