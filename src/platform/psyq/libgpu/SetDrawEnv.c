#include "psx/libgpu.h"

/* LIBGPU, retail 0x80025524-0x800257c8. Builds environment and optional background commands. */
void SetDrawEnv(DR_ENV* dr_env, DRAWENV* env) {
    extern volatile psyq_gpu_environment_t g_psyq_gpu_environment; /* An unqualified RAM view removes native 16-bit
                                                                      normalization in the bounds clamps. */
    /* Component packing replaces the retail 32-bit GPU loads with 16-bit loads. */
    RECT rectangle;
    int index;
    /* Code-member indexing reverses the ADDU operands of the variable-length tail. */
    u32* packet = (u32*)dr_env;
    dr_env->code[0] = get_cs(env->clip.x, env->clip.y);
    dr_env->code[1] = get_ce(env->clip.w + env->clip.x - 1, env->clip.y + env->clip.h - 1);
    dr_env->code[2] = get_ofs(env->ofs[0], env->ofs[1]);
    dr_env->code[3] = get_mode(env->dfe, env->dtd, env->tpage);
    dr_env->code[4] = get_tw(&env->tw);
    dr_env->code[5] = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_MASK);
    index = 7;
    if (env->isbg) {
        rectangle.x = env->clip.x;
        rectangle.y = env->clip.y;
        rectangle.w = env->clip.w;
        rectangle.h = env->clip.h;
        limitRange(rectangle.w, 0, g_psyq_gpu_environment.vram_width - 1);
        limitRange(rectangle.h, 0, g_psyq_gpu_environment.vram_height - 1);
        if ((rectangle.x & 63) || (rectangle.w & 63)) {
            rectangle.x -= env->ofs[0];
            rectangle.y -= env->ofs[1];
            packet[index++] = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_TILE) | (env->b0 << 16) | (env->g0 << 8) | env->r0;
            packet[index++] = *(u32*)&rectangle.x;
            packet[index++] = *(u32*)&rectangle.w;
            rectangle.x += env->ofs[0];
            rectangle.y += env->ofs[1];
        } else {
            packet[index++]
                = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_BLOCK_FILL) | (env->b0 << 16) | (env->g0 << 8) | env->r0;
            packet[index++] = *(u32*)&rectangle.x;
            packet[index++] = *(u32*)&rectangle.w;
        }
    }
    setlen(packet, index - 1);
}
