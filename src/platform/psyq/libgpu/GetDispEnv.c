/* LIBGPU 80025334-8002536c. */
#include "psx/libgpu.h"

DISPENV* GetDispEnv(DISPENV* env) {
    psyq_api_memcpy(env, &g_psyq_gpu_environment.display, sizeof(DISPENV));
    return env;
}
