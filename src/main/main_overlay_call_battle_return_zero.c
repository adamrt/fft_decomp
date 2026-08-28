#include "fft/main_runtime.h"

int main_overlay_call_battle_return_zero(void) {
    if (g_battle_overlay_loaded == 0) {
        return 0;
    }
    return battle_return_zero();
}
