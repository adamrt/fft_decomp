#include "psx/libgpu.h"

/* LIBGPU, retail 0x80027428-0x800275c0. Offsets are relative to the object table. */
int psyq_gpu_open_tmd_object(
    psyq_tmd_header_t* header, int object_index, psyq_tmd_packet_t** primitive, SVECTOR** vertices, SVECTOR** normals) {
    psyq_tmd_object_t* objects = header->objects;
    if (GetGraphDebug() == 2)
        printf(g_psyq_gpu_tmd_decode_message);
    if (GetGraphDebug() == 2)
        printf(g_psyq_gpu_tmd_object_format, header->id, header->flags, header->object_count, object_index);
    if (GetGraphDebug() == 2)
        printf(g_psyq_gpu_tmd_vertices_format, objects[object_index].vertex_offset, objects[object_index].vertex_count);
    if (GetGraphDebug() == 2)
        printf(g_psyq_gpu_tmd_normals_format, objects[object_index].normal_offset, objects[object_index].normal_count);
    if (GetGraphDebug() == 2)
        printf(g_psyq_gpu_tmd_primitives_format, objects[object_index].primitive_offset,
            objects[object_index].primitive_count);
    *vertices = (SVECTOR*)((u8*)objects + objects[object_index].vertex_offset);
    *normals = (SVECTOR*)((u8*)objects + objects[object_index].normal_offset);
    *primitive = (psyq_tmd_packet_t*)((u8*)objects + objects[object_index].primitive_offset);
    return objects[object_index].primitive_count;
}
