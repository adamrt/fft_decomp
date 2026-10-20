/* Render the resident projectile models and thrown item sprite. */

#include "fft/camera.h"
#include "fft/effect_model.h"
#include "fft/effect_particle.h"
#include "fft/effect_secondary.h"
#include "fft/gfx_image.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern const psyq_tmd_header_t* g_effect_model_files[EFFECT_MODEL_COUNT];

#define PT(k) (*(VECTOR*)((u8*)scratch + (command.vertices.indices[k] << 4)))

/* Draw one of the models resident in BATTLE.BIN for the current
 * secondary effect: the low byte of parameter is an effect_model_e.
 *
 * The model's vertices go through three transforms in the scratchpad: the spin
 * rotation, then the angles rotation with scale and position, then the camera
 * matrix. The model's draw commands are then decoded one by one into the effect's
 * work buffer for the current frame buffer and linked into the effect ordering
 * table:
 * - ARROW: G4 and G3 commands, at a quarter of the corners' average z (their sum
 *   over 16 or 12).
 * - STONE and SHURIKEN: G3 commands only, at the same depth.
 * - REFLECT: only the first timer * 3 commands, so the shield grows by three
 *   triangles a frame. Its GT3 commands use texture page 0x3f and the CLUT at
 *   0x7eff plus the effect's own slot id. The depth is the last corner's z over
 *   12, and every triangle is linked at ordering table entry 0, not at its
 *   depth.
 *
 * A polygon whose depth is not between 1 and EFFECT_OT_DEPTH_MAX - 1 is built but
 * not drawn. */
void effect_model_draw(SVECTOR* spin, VECTOR* position, SVECTOR* angles, VECTOR* scale, s32 parameter) {
    effect_decoded_model_command_t command;
    s32 unused[2];
    MATRIX matrix;
    SVECTOR point;
    const psyq_tmd_header_t* model;
    const SVECTOR* vertices;
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

    RotMatrix(spin, &matrix);
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    model = g_effect_model_files[parameter & 0xff];
    scratch = (VECTOR*)getScratchAddr(0);
    vertices = (const SVECTOR*)((const u8*)model->objects + model->objects[0].vertex_offset);
    count = model->objects[0].vertex_count;
    for (i = 0; i < count; i++) {
        gte_RotTrans(&vertices[i], &scratch[i], &scratch[i].pad);
    }
    RotMatrix(angles, &matrix);
    ScaleMatrixL(&matrix, scale);
    matrix.t[0] = position->vx;
    matrix.t[1] = position->vy;
    matrix.t[2] = position->vz;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    for (i = 0; i < count; i++) {
        copyVector(&point, &scratch[i]);
        gte_RotTrans(&point, &scratch[i], &scratch[i].pad);
    }
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    for (i = 0; i < count; i++) {
        copyVector(&point, &scratch[i]);
        gte_RotTrans(&point, &scratch[i], &scratch[i].pad);
    }
    command_count = effect_model_sections_init(model, 0);
    gt3_index = 0;
    g4_index = 0;
    g3_index = 0;
    switch (parameter & 0xff) {
    case EFFECT_MODEL_ARROW: {
        effect_arrow_buffer_t* work;
        POLY_G4* quad;
        POLY_G3* tri;

        work = g_effect_secondary_current->allocation;
        for (i = 0; i < command_count;) {
            effect_model_command_decode(&command);
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G4) {
                quad = &work->g4[g_effect_buffer_index][g4_index];
                setRGB0(quad, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(quad, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(quad, command.colors[2].r, command.colors[2].g, command.colors[2].b);
                setRGB3(quad, command.colors[3].r, command.colors[3].g, command.colors[3].b);
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
                    addPrim(g_effect_polygon_depth_data + z, quad);
                }
            }
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G3) {
                tri = &work->g3[g_effect_buffer_index][g3_index];
                setRGB0(tri, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(tri, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(tri, command.colors[2].r, command.colors[2].g, command.colors[2].b);
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
                    addPrim(g_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    case EFFECT_MODEL_STONE: {
        effect_stone_buffer_t* work;
        POLY_G3* tri;

        work = g_effect_secondary_current->allocation;
        j = 0;
        for (i = 0; i < command_count;) {
            effect_model_command_decode(&command);
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G3) {
                tri = &work->g3[g_effect_buffer_index][j];
                setRGB0(tri, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(tri, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(tri, command.colors[2].r, command.colors[2].g, command.colors[2].b);
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
                    addPrim(g_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    case EFFECT_MODEL_REFLECT: {
        effect_reflect_buffer_t* work;
        POLY_GT3* tri;

        work = g_effect_secondary_current->allocation;
        /* g4_index is reused here as the limit on decoded commands. */
        g4_index = g_effect_secondary_current->timer * 3;
        if (command_count < g4_index) {
            g4_index = command_count;
        }
        for (i = 0; i < g4_index;) {
            effect_model_command_decode(&command);
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_GT3_SEMITRANSPARENT) {
                tri = &work->prims[g_effect_buffer_index][gt3_index];
                tri->tpage = 0x3f;
                tri->clut = g_effect_secondary_current->own_slot_id + 0x7eff;
                setRGB0(tri, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(tri, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(tri, command.colors[2].r, command.colors[2].g, command.colors[2].b);
                memcpy(&tri->u0, &command.texture.uv[0], sizeof(command.texture.uv[0]));
                memcpy(&tri->u1, &command.texture.uv[1], sizeof(command.texture.uv[1]));
                memcpy(&tri->u2, &command.texture.uv[2], sizeof(command.texture.uv[2]));
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
                    ot = g_effect_polygon_depth_data;
                    addPrim(ot, tri);
                }
            }
        }
        break;
    }
    case EFFECT_MODEL_SHURIKEN: {
        effect_shuriken_buffer_t* work;
        POLY_G3* tri;

        work = g_effect_secondary_current->allocation;
        j = 0;
        for (i = 0; i < command_count;) {
            effect_model_command_decode(&command);
            i++;
            if (command.header.opcode == EFFECT_MODEL_COMMAND_G3) {
                tri = &work->g3[g_effect_buffer_index][j];
                setRGB0(tri, command.colors[0].r, command.colors[0].g, command.colors[0].b);
                setRGB1(tri, command.colors[1].r, command.colors[1].g, command.colors[1].b);
                setRGB2(tri, command.colors[2].r, command.colors[2].g, command.colors[2].b);
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
                    addPrim(g_effect_polygon_depth_data + z, tri);
                }
            }
        }
        break;
    }
    }
}

/*
 * Queues the thrown item's icon for the current secondary effect: projects the
 * effect origin to find its OT depth, builds a rotated and camera-scaled 2D
 * matrix from the sprite angle, and transforms the four quad corners into the
 * slot's double-buffered primitive before adding it to the ordering table.
 */
void effect_thrown_item_sprite_draw(VECTOR* origin, s32 item_id, s32 angle) {
    SPRT sprite;
    SVECTOR local;
    VECTOR position;
    s32 depth_cue;
    s32 flag;
    effect_slot_t* slot = &g_effect_slots[g_effect_secondary_current->slot_ids[0]];
    POLY_FT4* prim;
    s32 depth;
    s32 sine;
    s32 cosine;
    s32 zoom;
    s32 scale_y;
    s32 half_w;
    s32 half_h;
    s16 right;
    s16 bottom;

    gfx_set_item_icon_sprite(&sprite, item_id);
    prim = &slot->quads[g_effect_buffer_index];
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    copyVector(&local, origin);
    RotTrans(&local, &position, &position.pad);
    depth = (position.vz >> 2) - 8;
    if (depth < EFFECT_OT_DEPTH_MIN) {
        depth = EFFECT_OT_DEPTH_MIN;
    }
    if (depth >= EFFECT_OT_DEPTH_LIMIT) {
        depth = EFFECT_OT_DEPTH_MAX;
    }
    g_effect_matrix.t[0] = position.vx;
    g_effect_matrix.t[1] = position.vy;
    cosine = rcos(angle);
    sine = rsin(angle);
    zoom = g_camera_zoom.vx;
    scale_y = g_camera_zoom.vy;
    g_effect_matrix.m[0][0] = (cosine * zoom) >> 12;
    g_effect_matrix.m[1][1] = (cosine * scale_y) >> 12;
    g_effect_matrix.m[0][1] = (-sine * scale_y) >> 12;
    g_effect_matrix.m[1][0] = (sine * zoom) >> 12;
    SetRotMatrix(&g_effect_matrix);
    SetTransMatrix(&g_effect_matrix);
    half_w = -sprite.w / 2;
    right = half_w + sprite.w;
    half_h = -sprite.h / 2;
    bottom = half_h + sprite.h;
    g_effect_corner_bottom_left.vx = half_w;
    g_effect_corner_top_left.vx = half_w;
    g_effect_corner_bottom_right.vx = right;
    g_effect_corner_top_right.vx = right;
    g_effect_corner_top_right.vy = half_h;
    g_effect_corner_top_left.vy = half_h;
    g_effect_corner_bottom_right.vy = bottom;
    g_effect_corner_bottom_left.vy = bottom;
    RotTransPers3(&g_effect_corner_top_left, &g_effect_corner_top_right, &g_effect_corner_bottom_left, (s32*)&prim->x0,
        (s32*)&prim->x1, (s32*)&prim->x2, &depth_cue, &flag);
    RotTransPers(&g_effect_corner_bottom_right, (s32*)&prim->x3, &depth_cue, &flag);
    AddPrim(g_effect_polygon_depth_data + depth, prim);
}
