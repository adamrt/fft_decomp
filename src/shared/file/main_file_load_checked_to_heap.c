#include "fft/main.h"

void* main_file_load_checked_to_heap(int sector, int size) {
    void* result = main_file_load_to_heap(sector, size);

    if (result == 0) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_FILE_LOAD);
    }
    return result;
}
