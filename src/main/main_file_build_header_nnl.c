#include "fft/main.h"

int main_file_build_header_nnl(main_file_load_descriptor_t* header, int sector, int sectors, void* destination) {
    return main_file_build_header(header, sector, sectors, destination, -1);
}
