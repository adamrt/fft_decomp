/* LIBGPU 80022eb0-80022eec. */
#include "psx/libgpu.h"

DISPENV* SetDefDispEnv(DISPENV* env, int x, int y, int w, int h) {
    setRECT(&env->disp, x, y, w, h);
    setRECT(&env->screen, 0, 0, 0, 0);
    env->isrgb24 = 0;
    env->isinter = 0;
    env->pad1 = 0;
    env->pad0 = 0;
    return env;
}
