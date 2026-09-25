#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/gte_inline.h"
#include "psx/types.h"

/* Gouraud textured triangle (SDK POLY_GT3 layout). */
typedef struct battle_effect_model_gt3 {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u16 uv0;
    u16 clut;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u16 uv1;
    u16 tpage;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u16 uv2;
    u16 pad3;
} battle_effect_model_gt3_t;

/* Decoded model command from battle_effect_decode_next_model_command. */
typedef struct battle_effect_decoded {
    u8 unknown_00[3];
    u8 opcode;       /* 0x03 */
    u8 colors[4][4]; /* 0x04 */
    u16 uv[5];       /* 0x14 */
    u8 unknown_1e[0x4a];
    u16 vertices[4]; /* 0x68 */
} battle_effect_decoded_t;

typedef struct battle_effect_arrow_work {
    POLY_G3 g3[2][4];  /* 0x000 */
    POLY_G4 g4[2][13]; /* 0x0e0 */
} battle_effect_arrow_work_t;

typedef struct battle_effect_stone_work {
    POLY_G3 g3[2][12];
} battle_effect_stone_work_t;

typedef struct battle_effect_reflect_model_work {
    u16 clut[2][6];
    battle_effect_model_gt3_t gt3[2][24];
} battle_effect_reflect_model_work_t;

typedef struct battle_effect_model3_work {
    POLY_G3 g3[2][16];
} battle_effect_model3_work_t;

void battle_effect_decode_next_model_command(battle_effect_decoded_t* out);

/* Transformed model points live in the scratchpad as VECTORs, with the GTE
 * flag of each transform stored in `pad`. */
#define SCRATCH ((VECTOR*)0x1f800000)
#define PT(k)   (*(VECTOR*)((s32)scratch + (command.vertices[k] << 4)))

void battle_effect_draw_projectile_model(battle_effect_rotation_vector_t* spin, VECTOR* position,
    battle_effect_rotation_vector_t* angles, VECTOR* scale, s32 parameter) {
    battle_effect_decoded_t command;
    s32 unused[2];
    MATRIX matrix;
    SVECTOR point;
    battle_effect_resource_t* model;
    SVECTOR* vertices;
    VECTOR* scratch;
    s32 count;
    s32 i;
    s32 command_count;
    s32 gt3_index;
    s32 g4_index;
    s32 g3_index;
    s32 j;
    s32 z;
    u32* ot;

    /* Psy-Q RotMatrix returns MATRIX*; the project-wide void declaration
     * changes this call's schedule. */
    ((MATRIX * (*)(SVECTOR*, MATRIX*)) RotMatrix)((SVECTOR*)spin, &matrix);
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    model = g_battle_effect_model_data_ptrs[parameter & 0xff];
    scratch = SCRATCH;
    vertices = (SVECTOR*)((u8*)model + (model->words[0] + 0xc));
    count = model->words[1];
    for (i = 0; i < count; i++) {
        gte_ldv0(&vertices[i]);
        gte_rtv0tr();
        gte_stlvnl(&scratch[i]);
        gte_stflg(&scratch[i].pad);
    }
    RotMatrix((SVECTOR*)angles, &matrix);
    ScaleMatrixL(&matrix, scale);
    matrix.t[0] = position->vx;
    matrix.t[1] = position->vy;
    matrix.t[2] = position->vz;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    for (i = 0; i < count; i++) {
        point.vx = scratch[i].vx;
        point.vy = scratch[i].vy;
        point.vz = scratch[i].vz;
        gte_ldv0(&point);
        gte_rtv0tr();
        gte_stlvnl(&scratch[i]);
        gte_stflg(&scratch[i].pad);
    }
    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);
    for (i = 0; i < count; i++) {
        point.vx = scratch[i].vx;
        point.vy = scratch[i].vy;
        point.vz = scratch[i].vz;
        gte_ldv0(&point);
        gte_rtv0tr();
        gte_stlvnl(&scratch[i]);
        gte_stflg(&scratch[i].pad);
    }
    /* The target passes a second argument the one-parameter callee ignores. */
    command_count = ((s32 (*)(battle_effect_resource_t*, s32))battle_effect_init_resource_sections)(model, 0);
    gt3_index = 0;
    g4_index = 0;
    g3_index = 0;
    switch (parameter & 0xff) {
    case 0: {
        battle_effect_arrow_work_t* work;
        POLY_G4* quad;
        POLY_G3* tri;

        work = g_battle_effect_current_secondary->allocation;
        for (i = 0; i < command_count;) {
            battle_effect_decode_next_model_command(&command);
            if (command.opcode == 0x39) {
                quad = &work->g4[g_battle_effect_buffer_index][g4_index];
                quad->r0 = command.colors[0][0];
                quad->g0 = command.colors[0][1];
                quad->b0 = command.colors[0][2];
                quad->r1 = command.colors[1][0];
                quad->g1 = command.colors[1][1];
                quad->b1 = command.colors[1][2];
                quad->r2 = command.colors[2][0];
                quad->g2 = command.colors[2][1];
                quad->b2 = command.colors[2][2];
                quad->r3 = command.colors[3][0];
                quad->g3 = command.colors[3][1];
                quad->b3 = command.colors[3][2];
                quad->x0 = PT(0).vx;
                quad->y0 = PT(0).vy;
                z = PT(0).vz;
                quad->x1 = PT(1).vx;
                quad->y1 = PT(1).vy;
                z += PT(1).vz;
                quad->x2 = PT(2).vx;
                quad->y2 = PT(2).vy;
                z += PT(2).vz;
                quad->x3 = PT(3).vx;
                quad->y3 = PT(3).vy;
                g4_index++;
                z += PT(3).vz;
                z /= 16;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    addPrim(g_battle_effect_polygon_depth_data + z, quad);
                }
            }
            i++;
            if (command.opcode == 0x31) {
                tri = &work->g3[g_battle_effect_buffer_index][g3_index];
                tri->r0 = command.colors[0][0];
                tri->g0 = command.colors[0][1];
                tri->b0 = command.colors[0][2];
                tri->r1 = command.colors[1][0];
                tri->g1 = command.colors[1][1];
                tri->b1 = command.colors[1][2];
                tri->r2 = command.colors[2][0];
                tri->g2 = command.colors[2][1];
                tri->b2 = command.colors[2][2];
                tri->x0 = PT(0).vx;
                tri->y0 = PT(0).vy;
                z = PT(0).vz;
                tri->x1 = PT(1).vx;
                tri->y1 = PT(1).vy;
                z += PT(1).vz;
                tri->x2 = PT(2).vx;
                tri->y2 = PT(2).vy;
                z += PT(2).vz;
                z /= 12;
                g3_index++;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    addPrim(g_battle_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    case 1: {
        battle_effect_stone_work_t* work;
        POLY_G3* tri;

        work = g_battle_effect_current_secondary->allocation;
        j = 0;
        for (i = 0; i < command_count;) {
            battle_effect_decode_next_model_command(&command);
            i++;
            if (command.opcode == 0x31) {
                tri = &work->g3[g_battle_effect_buffer_index][j];
                tri->r0 = command.colors[0][0];
                tri->g0 = command.colors[0][1];
                tri->b0 = command.colors[0][2];
                tri->r1 = command.colors[1][0];
                tri->g1 = command.colors[1][1];
                tri->b1 = command.colors[1][2];
                tri->r2 = command.colors[2][0];
                tri->g2 = command.colors[2][1];
                tri->b2 = command.colors[2][2];
                tri->x0 = PT(0).vx;
                tri->y0 = PT(0).vy;
                z = PT(0).vz;
                tri->x1 = PT(1).vx;
                tri->y1 = PT(1).vy;
                z += PT(1).vz;
                tri->x2 = PT(2).vx;
                tri->y2 = PT(2).vy;
                z += PT(2).vz;
                z /= 12;
                j++;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    addPrim(g_battle_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    case 2: {
        battle_effect_reflect_model_work_t* work;
        battle_effect_model_gt3_t* tri;

        work = g_battle_effect_current_secondary->allocation;
        /* g4_index is reused here as the limit on decoded commands. */
        g4_index = g_battle_effect_current_secondary->timer * 3;
        if (command_count < g4_index) {
            g4_index = command_count;
        }
        for (i = 0; i < g4_index;) {
            battle_effect_decode_next_model_command(&command);
            i++;
            if (command.opcode == 0x37) {
                tri = &work->gt3[g_battle_effect_buffer_index][gt3_index];
                tri->tpage = 0x3f;
                tri->clut = g_battle_effect_current_secondary->own_slot_id + 0x7eff;
                tri->r0 = command.colors[0][0];
                tri->g0 = command.colors[0][1];
                tri->b0 = command.colors[0][2];
                tri->r1 = command.colors[1][0];
                tri->g1 = command.colors[1][1];
                tri->b1 = command.colors[1][2];
                tri->r2 = command.colors[2][0];
                tri->g2 = command.colors[2][1];
                tri->b2 = command.colors[2][2];
                tri->uv0 = command.uv[2];
                tri->uv1 = command.uv[3];
                tri->uv2 = command.uv[4];
                tri->x0 = PT(0).vx;
                tri->y0 = PT(0).vy;
                z = PT(0).vz;
                tri->x1 = PT(1).vx;
                tri->y1 = PT(1).vy;
                z = PT(1).vz;
                tri->x2 = PT(2).vx;
                tri->y2 = PT(2).vy;
                z = PT(2).vz;
                z /= 12;
                gt3_index++;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    ot = g_battle_effect_polygon_depth_data;
                    addPrim(ot, tri);
                }
            }
        }
        break;
    }
    case 3: {
        battle_effect_model3_work_t* work;
        POLY_G3* tri;

        work = g_battle_effect_current_secondary->allocation;
        j = 0;
        for (i = 0; i < command_count;) {
            battle_effect_decode_next_model_command(&command);
            i++;
            if (command.opcode == 0x31) {
                tri = &work->g3[g_battle_effect_buffer_index][j];
                tri->r0 = command.colors[0][0];
                tri->g0 = command.colors[0][1];
                tri->b0 = command.colors[0][2];
                tri->r1 = command.colors[1][0];
                tri->g1 = command.colors[1][1];
                tri->b1 = command.colors[1][2];
                tri->r2 = command.colors[2][0];
                tri->g2 = command.colors[2][1];
                tri->b2 = command.colors[2][2];
                tri->x0 = PT(0).vx;
                tri->y0 = PT(0).vy;
                z = PT(0).vz;
                tri->x1 = PT(1).vx;
                tri->y1 = PT(1).vy;
                z += PT(1).vz;
                tri->x2 = PT(2).vx;
                tri->y2 = PT(2).vy;
                z += PT(2).vz;
                z /= 12;
                j++;
                if (z > 0 && z < EFFECT_OT_DEPTH_MAX) {
                    addPrim(g_battle_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    }
}
