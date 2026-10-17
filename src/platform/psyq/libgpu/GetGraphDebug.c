/* LIBGPU 800245cc-800245dc. */
#include "psx/libgpu.h"

int GetGraphDebug(void) {
    return g_psyq_gpu_environment.debug_level;
}
