/* LIBGPU 80025a88-80025b44. */
#include "psx/libgpu.h"

/* Return the display start x for rect on the detected GPU type: halved on type 2, and mirrored
 * from 1024 when the display is reversed. */
int get_dx(RECT* rect) {
    /* The volatile view preserves separate graph-type address construction. */
    volatile psyq_gpu_environment_t* environment = &g_psyq_gpu_environment;
    switch (environment->graph_type) {
    case 1:
        return environment->reversed ? 1024 - rect->x - rect->w : rect->x;
    case 2:
        return environment->reversed ? 1024 - rect->x - rect->w / 2 : rect->x / 2;
    }
    return rect->x;
}
