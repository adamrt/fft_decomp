/* LIBGPU 80024d70-80024e4c. */
#include "psx/libgpu.h"
void DrawOTagEnv(u32* ot, DRAWENV* env) {
    if (g_psyq_gpu_environment.debug_level >= 2)
        GPU_printf(g_psyq_gpu_draw_otag_env_format, ot, env);
    SetDrawEnv(&env->dr_env, env);
    setaddr(&env->dr_env, ot);
    g_psyq_gpu_dispatch->enqueue_four(g_psyq_gpu_dispatch->ordering_table, &env->dr_env, sizeof(env->dr_env), 0);
    psyq_api_memcpy(&g_psyq_gpu_environment.draw, env, sizeof(*env));
}
