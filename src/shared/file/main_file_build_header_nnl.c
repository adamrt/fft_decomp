#include "fft/main.h"

/* main_file_build_header with loading display mode -1: the read shows no "Now
 * Loading" message. */
int main_file_build_header_nnl(main_file_load_descriptor_t* header, int sector, int sectors, void* destination) {
    return main_file_build_header(header, sector, sectors, destination, MAIN_FILE_LOADING_DISPLAY_OFF);
}
