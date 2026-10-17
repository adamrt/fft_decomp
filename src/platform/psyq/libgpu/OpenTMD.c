#include "psx/libgpu.h"

/* LIBGPU, retail 0x8002701c-0x8002705c. */
int OpenTMD(u32* tmd, int object_index) {
    int count = psyq_gpu_open_tmd_object((psyq_tmd_header_t*)tmd, object_index, &g_psyq_gpu_tmd_cursor,
        &g_psyq_gpu_tmd_vertices, &g_psyq_gpu_tmd_normals);
    g_psyq_gpu_tmd_primitive_count = count;
    return count;
}
