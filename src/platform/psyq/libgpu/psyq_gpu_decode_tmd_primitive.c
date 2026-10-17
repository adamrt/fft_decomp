/* LIBGPU 800275c0-80028740. */
#include "psx/libgpu.h"

/* Unpack one TMD primitive packet into a zeroed TMD_PRIM (color, texture and vertex/normal
 * indexes; flat packets repeat their one color or normal). Returns the packet size in bytes, or -1
 * for an unsupported type. */
int psyq_gpu_decode_tmd_primitive(const psyq_tmd_packet_t* packet, TMD_PRIM* primitive) {
    memset(primitive, 0, sizeof(*primitive));
    primitive->id = packet->id;
    switch (primitive->id & ~PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_SEMI_TRANSPARENT)) {
    case PSYQ_TMD_PACKET_F3_LIT:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_f3_lit_name);
        setRGB0(primitive, packet->f3_lit.r0, packet->f3_lit.g0, packet->f3_lit.b0);
        setRGB1(primitive, packet->f3_lit.r0, packet->f3_lit.g0, packet->f3_lit.b0);
        setRGB2(primitive, packet->f3_lit.r0, packet->f3_lit.g0, packet->f3_lit.b0);
        primitive->vert0 = packet->f3_lit.v0;
        primitive->vert1 = packet->f3_lit.v1;
        primitive->vert2 = packet->f3_lit.v2;
        primitive->norm0 = packet->f3_lit.n0;
        primitive->norm1 = packet->f3_lit.n0;
        primitive->norm2 = packet->f3_lit.n0;
        return sizeof(packet->f3_lit);
    case PSYQ_TMD_PACKET_G3_LIT:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_g3_lit_name);
        setRGB0(primitive, packet->g3_lit.r0, packet->g3_lit.g0, packet->g3_lit.b0);
        setRGB1(primitive, packet->g3_lit.r0, packet->g3_lit.g0, packet->g3_lit.b0);
        setRGB2(primitive, packet->g3_lit.r0, packet->g3_lit.g0, packet->g3_lit.b0);
        primitive->vert0 = packet->g3_lit.v0;
        primitive->vert1 = packet->g3_lit.v1;
        primitive->vert2 = packet->g3_lit.v2;
        primitive->norm0 = packet->g3_lit.n0;
        primitive->norm1 = packet->g3_lit.n1;
        primitive->norm2 = packet->g3_lit.n2;
        return sizeof(packet->g3_lit);
    case PSYQ_TMD_PACKET_FT3_LIT:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_ft3_lit_name);
        primitive->tpage = packet->ft3_lit.tpage;
        primitive->clut = packet->ft3_lit.clut;
        setUV3(primitive, packet->ft3_lit.tu0, packet->ft3_lit.tv0, packet->ft3_lit.tu1, packet->ft3_lit.tv1,
            packet->ft3_lit.tu2, packet->ft3_lit.tv2);
        primitive->vert0 = packet->ft3_lit.v0;
        primitive->vert1 = packet->ft3_lit.v1;
        primitive->vert2 = packet->ft3_lit.v2;
        primitive->norm0 = packet->ft3_lit.n0;
        primitive->norm1 = packet->ft3_lit.n0;
        primitive->norm2 = packet->ft3_lit.n0;
        return sizeof(packet->ft3_lit);
    case PSYQ_TMD_PACKET_GT3_LIT:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_gt3_lit_name);
        primitive->tpage = packet->gt3_lit.tpage;
        primitive->clut = packet->gt3_lit.clut;
        setUV3(primitive, packet->gt3_lit.tu0, packet->gt3_lit.tv0, packet->gt3_lit.tu1, packet->gt3_lit.tv1,
            packet->gt3_lit.tu2, packet->gt3_lit.tv2);
        primitive->vert0 = packet->gt3_lit.v0;
        primitive->vert1 = packet->gt3_lit.v1;
        primitive->vert2 = packet->gt3_lit.v2;
        primitive->norm0 = packet->gt3_lit.n0;
        primitive->norm1 = packet->gt3_lit.n1;
        primitive->norm2 = packet->gt3_lit.n2;
        return sizeof(packet->gt3_lit);
    case PSYQ_TMD_PACKET_F3:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_f3_name);
        setRGB0(primitive, packet->f3.r0, packet->f3.g0, packet->f3.b0);
        setRGB1(primitive, packet->f3.r0, packet->f3.g0, packet->f3.b0);
        setRGB2(primitive, packet->f3.r0, packet->f3.g0, packet->f3.b0);
        primitive->vert0 = packet->f3.v0;
        primitive->vert1 = packet->f3.v1;
        primitive->vert2 = packet->f3.v2;
        return sizeof(packet->f3);
    case PSYQ_TMD_PACKET_G3:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_g3_name);
        setRGB0(primitive, packet->g3.r0, packet->g3.g0, packet->g3.b0);
        setRGB1(primitive, packet->g3.r1, packet->g3.g1, packet->g3.b1);
        setRGB2(primitive, packet->g3.r2, packet->g3.g2, packet->g3.b2);
        primitive->vert0 = packet->g3.v0;
        primitive->vert1 = packet->g3.v1;
        primitive->vert2 = packet->g3.v2;
        return sizeof(packet->g3);
    case PSYQ_TMD_PACKET_FT3:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_ft3_name);
        primitive->tpage = packet->ft3.tpage;
        primitive->clut = packet->ft3.clut;
        setUV3(primitive, packet->ft3.tu0, packet->ft3.tv0, packet->ft3.tu1, packet->ft3.tv1, packet->ft3.tu2,
            packet->ft3.tv2);
        setRGB0(primitive, packet->ft3.r0, packet->ft3.g0, packet->ft3.b0);
        setRGB1(primitive, packet->ft3.r0, packet->ft3.g0, packet->ft3.b0);
        setRGB2(primitive, packet->ft3.r0, packet->ft3.g0, packet->ft3.b0);
        primitive->vert0 = packet->ft3.v0;
        primitive->vert1 = packet->ft3.v1;
        primitive->vert2 = packet->ft3.v2;
        return sizeof(packet->ft3);
    case PSYQ_TMD_PACKET_GT3:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_gt3_name);
        primitive->tpage = packet->gt3.tpage;
        primitive->clut = packet->gt3.clut;
        setUV3(primitive, packet->gt3.tu0, packet->gt3.tv0, packet->gt3.tu1, packet->gt3.tv1, packet->gt3.tu2,
            packet->gt3.tv2);
        primitive->vert0 = packet->gt3.v0;
        primitive->vert1 = packet->gt3.v1;
        primitive->vert2 = packet->gt3.v2;
        setRGB0(primitive, packet->gt3.r0, packet->gt3.g0, packet->gt3.b0);
        setRGB1(primitive, packet->gt3.r1, packet->gt3.g1, packet->gt3.b1);
        setRGB2(primitive, packet->gt3.r2, packet->gt3.g2, packet->gt3.b2);
        return sizeof(packet->gt3);
    case PSYQ_TMD_PACKET_F4_LIT:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_f4_lit_name);
        setRGB0(primitive, packet->f4_lit.r0, packet->f4_lit.g0, packet->f4_lit.b0);
        setRGB1(primitive, packet->f4_lit.r0, packet->f4_lit.g0, packet->f4_lit.b0);
        setRGB2(primitive, packet->f4_lit.r0, packet->f4_lit.g0, packet->f4_lit.b0);
        setRGB3(primitive, packet->f4_lit.r0, packet->f4_lit.g0, packet->f4_lit.b0);
        primitive->vert0 = packet->f4_lit.v0;
        primitive->vert1 = packet->f4_lit.v1;
        primitive->vert2 = packet->f4_lit.v2;
        primitive->vert3 = packet->f4_lit.v3;
        primitive->norm0 = packet->f4_lit.n0;
        primitive->norm1 = packet->f4_lit.n0;
        primitive->norm2 = packet->f4_lit.n0;
        primitive->norm3 = packet->f4_lit.n0;
        return sizeof(packet->f4_lit);
    case PSYQ_TMD_PACKET_G4_LIT:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_g4_lit_name);
        setRGB0(primitive, packet->g4_lit.r0, packet->g4_lit.g0, packet->g4_lit.b0);
        setRGB1(primitive, packet->g4_lit.r0, packet->g4_lit.g0, packet->g4_lit.b0);
        setRGB2(primitive, packet->g4_lit.r0, packet->g4_lit.g0, packet->g4_lit.b0);
        setRGB3(primitive, packet->g4_lit.r0, packet->g4_lit.g0, packet->g4_lit.b0);
        primitive->vert0 = packet->g4_lit.v0;
        primitive->vert1 = packet->g4_lit.v1;
        primitive->vert2 = packet->g4_lit.v2;
        primitive->vert3 = packet->g4_lit.v3;
        primitive->norm0 = packet->g4_lit.n0;
        primitive->norm1 = packet->g4_lit.n1;
        primitive->norm2 = packet->g4_lit.n2;
        primitive->norm3 = packet->g4_lit.n3;
        return sizeof(packet->g4_lit);
    case PSYQ_TMD_PACKET_FT4_LIT:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_ft4_lit_name);
        primitive->tpage = packet->ft4_lit.tpage;
        primitive->clut = packet->ft4_lit.clut;
        setUV4(primitive, packet->ft4_lit.tu0, packet->ft4_lit.tv0, packet->ft4_lit.tu1, packet->ft4_lit.tv1,
            packet->ft4_lit.tu2, packet->ft4_lit.tv2, packet->ft4_lit.tu3, packet->ft4_lit.tv3);
        primitive->vert0 = packet->ft4_lit.v0;
        primitive->vert1 = packet->ft4_lit.v1;
        primitive->vert2 = packet->ft4_lit.v2;
        primitive->vert3 = packet->ft4_lit.v3;
        primitive->norm0 = packet->ft4_lit.n0;
        primitive->norm1 = packet->ft4_lit.n0;
        primitive->norm2 = packet->ft4_lit.n0;
        primitive->norm3 = packet->ft4_lit.n0;
        return sizeof(packet->ft4_lit);
    case PSYQ_TMD_PACKET_GT4_LIT:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_gt4_lit_name);
        primitive->tpage = packet->gt4_lit.tpage;
        primitive->clut = packet->gt4_lit.clut;
        setUV4(primitive, packet->gt4_lit.tu0, packet->gt4_lit.tv0, packet->gt4_lit.tu1, packet->gt4_lit.tv1,
            packet->gt4_lit.tu2, packet->gt4_lit.tv2, packet->gt4_lit.tu3, packet->gt4_lit.tv3);
        primitive->vert0 = packet->gt4_lit.v0;
        primitive->vert1 = packet->gt4_lit.v1;
        primitive->vert2 = packet->gt4_lit.v2;
        primitive->vert3 = packet->gt4_lit.v3;
        primitive->norm0 = packet->gt4_lit.n0;
        primitive->norm1 = packet->gt4_lit.n1;
        primitive->norm2 = packet->gt4_lit.n2;
        primitive->norm3 = packet->gt4_lit.n3;
        return sizeof(packet->gt4_lit);
    case PSYQ_TMD_PACKET_F4:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_f4_name);
        setRGB0(primitive, packet->f4.r0, packet->f4.g0, packet->f4.b0);
        setRGB1(primitive, packet->f4.r0, packet->f4.g0, packet->f4.b0);
        setRGB2(primitive, packet->f4.r0, packet->f4.g0, packet->f4.b0);
        setRGB3(primitive, packet->f4.r0, packet->f4.g0, packet->f4.b0);
        primitive->vert0 = packet->f4.v0;
        primitive->vert1 = packet->f4.v1;
        primitive->vert2 = packet->f4.v2;
        primitive->vert3 = packet->f4.v3;
        return sizeof(packet->f4);
    case PSYQ_TMD_PACKET_G4:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_g4_name);
        setRGB0(primitive, packet->g4.r0, packet->g4.g0, packet->g4.b0);
        setRGB1(primitive, packet->g4.r1, packet->g4.g1, packet->g4.b1);
        setRGB2(primitive, packet->g4.r2, packet->g4.g2, packet->g4.b2);
        setRGB3(primitive, packet->g4.r3, packet->g4.g3, packet->g4.b3);
        primitive->vert0 = packet->g4.v0;
        primitive->vert1 = packet->g4.v1;
        primitive->vert2 = packet->g4.v2;
        primitive->vert3 = packet->g4.v3;
        return sizeof(packet->g4);
    case PSYQ_TMD_PACKET_FT4:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_ft4_name);
        primitive->tpage = packet->ft4.tpage;
        primitive->clut = packet->ft4.clut;
        setUV4(primitive, packet->ft4.tu0, packet->ft4.tv0, packet->ft4.tu1, packet->ft4.tv1, packet->ft4.tu2,
            packet->ft4.tv2, packet->ft4.tu3, packet->ft4.tv3);
        setRGB0(primitive, packet->ft4.r0, packet->ft4.g0, packet->ft4.b0);
        setRGB1(primitive, packet->ft4.r0, packet->ft4.g0, packet->ft4.b0);
        setRGB2(primitive, packet->ft4.r0, packet->ft4.g0, packet->ft4.b0);
        setRGB3(primitive, packet->ft4.r0, packet->ft4.g0, packet->ft4.b0);
        primitive->vert0 = packet->ft4.v0;
        primitive->vert1 = packet->ft4.v1;
        primitive->vert2 = packet->ft4.v2;
        primitive->vert3 = packet->ft4.v3;
        return sizeof(packet->ft4);
    case PSYQ_TMD_PACKET_GT4:
        if (GetGraphDebug() == 2)
            printf(g_psyq_gpu_tmd_gt4_name);
        primitive->tpage = packet->gt4.tpage;
        primitive->clut = packet->gt4.clut;
        setUV4(primitive, packet->gt4.tu0, packet->gt4.tv0, packet->gt4.tu1, packet->gt4.tv1, packet->gt4.tu2,
            packet->gt4.tv2, packet->gt4.tu3, packet->gt4.tv3);
        primitive->vert0 = packet->gt4.v0;
        primitive->vert1 = packet->gt4.v1;
        primitive->vert2 = packet->gt4.v2;
        primitive->vert3 = packet->gt4.v3;
        setRGB0(primitive, packet->gt4.r0, packet->gt4.g0, packet->gt4.b0);
        setRGB1(primitive, packet->gt4.r1, packet->gt4.g1, packet->gt4.b1);
        setRGB2(primitive, packet->gt4.r2, packet->gt4.g2, packet->gt4.b2);
        setRGB3(primitive, packet->gt4.r3, packet->gt4.g3, packet->gt4.b3);
        return sizeof(packet->gt4);
    default:
        printf(
            g_psyq_gpu_unsupported_tmd_format, primitive->id & ~PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_SEMI_TRANSPARENT));
        return -1;
    }
}
