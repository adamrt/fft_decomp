/* LIBGPU 80024e84-80025334. */
#include "psx/libgpu.h"

DISPENV* PutDispEnv(DISPENV* env) {
    extern volatile psyq_gpu_environment_t g_psyq_gpu_environment; /* An unqualified RAM view folds the native 16-bit
                                                                      address and conversion sequences. */
    int h_start;
    int v_start;
    int h_end;
    int v_end;
    u32 mode = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_DISPLAY_MODE);
    if (g_psyq_gpu_environment.debug_level >= 2)
        GPU_printf(g_psyq_gpu_put_disp_env_format, env);
    g_psyq_gpu_dispatch->control((g_psyq_gpu_environment.graph_type == 1 || g_psyq_gpu_environment.graph_type == 2)
            ? ((env->disp.y & 0xfff) << 12) | (get_dx(&env->disp) & 0xfff)
                | PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_DISPLAY_START)
            : ((env->disp.y & 0x3ff) << 10) | (env->disp.x & 0x3ff)
                | PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_DISPLAY_START));
    if (g_psyq_gpu_environment.display.screen.x != env->screen.x
        || g_psyq_gpu_environment.display.screen.y != env->screen.y
        || g_psyq_gpu_environment.display.screen.w != env->screen.w
        || g_psyq_gpu_environment.display.screen.h != env->screen.h) {
        env->pad0 = GetVideoMode(); /* pad0 caches the video mode. */
        h_start = env->screen.x * 10 + 608;
        v_start = env->screen.y + (env->pad0 ? 19 : 16);
        h_end = h_start + (env->screen.w ? env->screen.w * 10 : 2560);
        v_end = v_start + (env->screen.h ? env->screen.h : 240);
        limitRange(h_start, 500, 3290);
        limitRange(h_end, h_start + 80, 3290);
        limitRange(v_start, 16, env->pad0 ? 310 : 256);
        limitRange(v_end, v_start + 2, env->pad0 ? 312 : 258);
        g_psyq_gpu_dispatch->control(
            PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_HORIZONTAL_RANGE) | ((h_end & 0xfff) << 12) | (h_start & 0xfff));
        g_psyq_gpu_dispatch->control(
            PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_VERTICAL_RANGE) | ((v_end & 0x3ff) << 10) | (v_start & 0x3ff));
    }
    /* isinter, isrgb24 and the two pad bytes are compared as one 32-bit value. */
    if (*(u32*)&g_psyq_gpu_environment.display.isinter != *(u32*)&env->isinter
        || g_psyq_gpu_environment.display.disp.x != env->disp.x || g_psyq_gpu_environment.display.disp.y != env->disp.y
        || g_psyq_gpu_environment.display.disp.w != env->disp.w
        || g_psyq_gpu_environment.display.disp.h != env->disp.h) {
        env->pad0 = GetVideoMode();
        if (env->pad0 == MODE_PAL)
            mode |= PSYQ_GPU_DISPLAY_PAL;
        if (env->isrgb24)
            mode |= PSYQ_GPU_DISPLAY_RGB24;
        if (env->isinter)
            mode |= PSYQ_GPU_DISPLAY_INTERLACE;
        if (g_psyq_gpu_environment.reversed)
            mode |= PSYQ_GPU_DISPLAY_REVERSE;
        if (env->disp.w <= 280)
            ;
        else if (env->disp.w <= 352)
            mode |= PSYQ_GPU_DISPLAY_WIDTH_320;
        else if (env->disp.w <= 400)
            mode |= PSYQ_GPU_DISPLAY_WIDTH_368;
        else if (env->disp.w <= 560)
            mode |= PSYQ_GPU_DISPLAY_WIDTH_512;
        else
            mode |= PSYQ_GPU_DISPLAY_WIDTH_640;
        if (env->disp.h <= (env->pad0 ? 288 : 256))
            ;
        else
            mode |= PSYQ_GPU_DISPLAY_HEIGHT_480 | PSYQ_GPU_DISPLAY_INTERLACE;
        g_psyq_gpu_dispatch->control(mode);
    }
    psyq_api_memcpy(&g_psyq_gpu_environment.display, env, sizeof(DISPENV));
    return env;
}
