#include "fft/main.h"

void* main_file_load_checked_to_address(int sector, int size, void* destination) {
    void* result = main_file_load_to_address(sector, size, destination);

    if (result == 0) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_FILE_LOAD);
    }
    return result;
}
