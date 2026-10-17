#include "psx/libgpu.h"

/* LIBGPU, retail 0x8002705c-0x80027308. Resolves all four indices without a count test. */
TMD_PRIM* ReadTMD(TMD_PRIM* primitive) {
    int size = psyq_gpu_decode_tmd_primitive(g_psyq_gpu_tmd_cursor, primitive);
    SVECTOR* vertices;
    SVECTOR* normals;
    if (size < 0)
        return 0;
    vertices = g_psyq_gpu_tmd_vertices;
    normals = g_psyq_gpu_tmd_normals;
    primitive->v_ofs = vertices;
    primitive->n_ofs = normals;
    copyVector(&primitive->n0, &normals[primitive->norm0]);
    copyVector(&primitive->n1, &normals[primitive->norm1]);
    copyVector(&primitive->n2, &normals[primitive->norm2]);
    g_psyq_gpu_tmd_cursor = (psyq_tmd_packet_t*)((u8*)g_psyq_gpu_tmd_cursor + size);
    copyVector(&primitive->n3, &normals[primitive->norm3]);
    copyVector(&primitive->x0, &vertices[primitive->vert0]);
    copyVector(&primitive->x1, &vertices[primitive->vert1]);
    copyVector(&primitive->x2, &vertices[primitive->vert2]);
    copyVector(&primitive->x3, &vertices[primitive->vert3]);
    return primitive;
}
