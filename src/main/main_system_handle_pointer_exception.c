#include "fft/main_runtime.h"

void main_system_handle_pointer_exception(int error_code) {
    main_system_handle_malloc_exception(0, error_code);
    g_main_system_graphics_error = 0;
}
