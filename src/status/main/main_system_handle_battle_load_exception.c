#include "fft/battle_status.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

void main_system_handle_battle_load_exception(s32 value) {
    main_system_handle_malloc_exception(3, value);
}
