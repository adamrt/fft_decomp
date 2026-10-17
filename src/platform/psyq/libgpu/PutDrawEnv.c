/* LIBGPU 80024cac-80024d70. */
#include "psx/libgpu.h"
DRAWENV* PutDrawEnv(DRAWENV* env) {
    if (g_psyq_gpu_environment.debug_level >= 2)
        GPU_printf(g_psyq_gpu_put_draw_env_format, env);
    SetDrawEnv(&env->dr_env, env);
    termPrim(&env->dr_env);
    g_psyq_gpu_dispatch->enqueue_four(g_psyq_gpu_dispatch->ordering_table, &env->dr_env, sizeof(env->dr_env), 0);
    psyq_api_memcpy(&g_psyq_gpu_environment.draw, env, sizeof(*env));
    return env;
}
