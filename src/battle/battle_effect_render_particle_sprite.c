#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "psx/gte.h"
#include "psx/types.h"

typedef struct battle_effect_frame_record {
    s16 unknown_00;
    s16 count;
    battle_effect_sprite_part_t parts[1];
} battle_effect_frame_record_t;

/* Advances a list node's frame script, rebuilds its sprite-part table when the
 * frame changes, and queues the sprite at the node position.
 *
 * Script bytes: a value below 0x80 is a frame id followed by a hold count and a
 * depth mode; 0x81 restarts the script, 0x82 sets the 16-bit x/y offset and
 * 0x83 adds a signed 8-bit x/y delta. kind bit 0 marks the frame dirty; bits
 * 1-2 select camera projection or a screen-space position, with or without
 * the camera zoom. The oversized frame is reproduced with unused locals. */
void battle_effect_render_particle_sprite(effect_list_node_t* particle, s32 red, s32 green, s32 blue) {
    effect_list_node_t* node;
    SVECTOR input;
    SVECTOR unused_20;
    VECTOR output;
    s32 unused_38[6];
    s16 position[4];
    battle_effect_sprite_block_t* block;
    battle_effect_frame_record_t* frame;
    s32 op;
    s16 count;
    s16 i;
    s32 depth;

    node = particle;
    block = node->sprite_block;
    node->frame_timer -= 2;
    if (node->frame_timer <= 0) {
        for (;;) {
            op = node->sequence_data[(u16)node->sequence_offset];
            if (!(op & 0x80)) {
                if ((u8)node->sprite_frame_index != op) {
                    node->sprite_frame_index = op;
                    node->kind |= 1;
                }
                node->frame_timer = node->sequence_data[(u16)node->sequence_offset + 1];
                node->depth_mode = node->sequence_data[(u16)node->sequence_offset + 2];
                if (node->frame_timer != 0) {
                    node->sequence_offset += 3;
                }
                break;
            }
            switch (op & 0x7f) {
            case 1:
                node->sequence_offset = 0;
                break;
            case 2:
                node->sprite_offset_x = node->sequence_data[(u16)node->sequence_offset + 1]
                    + (node->sequence_data[(u16)node->sequence_offset + 2] << 8);
                node->sprite_offset_y = node->sequence_data[(u16)node->sequence_offset + 3]
                    + (node->sequence_data[(u16)node->sequence_offset + 4] << 8);
                node->sequence_offset += 5;
                break;
            case 3:
                node->sprite_offset_x += (s8)node->sequence_data[(u16)node->sequence_offset + 1];
                node->sprite_offset_y += (s8)node->sequence_data[(u16)node->sequence_offset + 2];
                node->sequence_offset += 3;
                break;
            }
        }
    }
    if (node->kind & 1) {
        frame = (battle_effect_frame_record_t*)(*(u16*)(g_battle_effect_frame_data
                                                    + ((u8)node->sprite_frame_index * 2
                                                        + *(s16*)(g_battle_effect_frame_data
                                                            + (u8)node->frame_group_index)))
            + g_battle_effect_frame_data);
        count = frame->count;
        if (block->sprite_count != count) {
            battle_effect_free_sprite_block(block);
            block = battle_effect_alloc_sprite_block(count);
            node->sprite_block = block;
        }
        for (i = 0; i < count; i++) {
            ((battle_effect_sprite_part_set_t*)block)->frames[i] = &frame->parts[i];
        }
        node->kind &= ~1;
    }
    block->red = red;
    block->green = green;
    block->blue = blue;
    switch (node->kind & 6) {
    case 0:
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        input.vx = node->x + node->sprite_offset_x;
        input.vy = node->y + node->sprite_offset_y;
        input.vz = node->z;
        RotTrans(&input, &output, (long*)&output.pad);
        depth = output.vz >> 2;
        switch (node->depth_mode) {
        case 1:
            depth -= 8;
            break;
        case 2:
            depth = 8;
            break;
        case 3:
            depth = 0x17e;
            break;
        case 4:
            depth = 0x10;
            break;
        case 5:
            depth -= 0x10;
            break;
        }
        if (depth < 0) {
            return;
        }
        if (depth > 0x17e) {
            depth = 0x17e;
        }
        position[0] = output.vx;
        position[1] = output.vy;
        battle_effect_submit_sprite_to_ordering_table((battle_effect_sprite_part_set_t*)node->sprite_block, position,
            node->screen_rotation_angle, &g_battle_camera_zoom, g_battle_effect_otag + depth);
        break;
    case 2:
        position[0] = node->x + node->sprite_offset_x + g_battle_camera_matrix.t[0];
        position[1] = node->y + node->sprite_offset_y + g_battle_camera_matrix.t[1];
        depth = node->z;
        if (depth < 0) {
            depth = 0;
        }
        if (depth > 0x17e) {
            depth = 0x17e;
        }
        battle_effect_submit_sprite_to_ordering_table((battle_effect_sprite_part_set_t*)node->sprite_block, position,
            node->screen_rotation_angle, &g_battle_camera_zoom, g_battle_effect_otag + depth);
        break;
    case 4:
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        input.vx = node->x + node->sprite_offset_x;
        input.vy = node->y + node->sprite_offset_y;
        input.vz = node->z;
        RotTrans(&input, &output, (long*)&output.pad);
        depth = output.vz >> 2;
        switch (node->depth_mode) {
        case 1:
            depth -= 8;
            break;
        case 2:
            depth = 8;
            break;
        case 3:
            depth = 0x17e;
            break;
        case 4:
            depth = 0x10;
            break;
        case 5:
            depth -= 0x10;
            break;
        }
        if (depth < 0) {
            return;
        }
        if (depth > 0x17e) {
            depth = 0x17e;
        }
        position[0] = output.vx;
        position[1] = output.vy;
        battle_effect_submit_sprite_to_ordering_table((battle_effect_sprite_part_set_t*)node->sprite_block, position,
            node->screen_rotation_angle, 0, g_battle_effect_otag + depth);
        break;
    case 6:
        position[0] = node->x + node->sprite_offset_x;
        position[1] = node->y + node->sprite_offset_y;
        depth = node->z;
        if (depth < 0) {
            depth = 0;
        }
        if (depth > 0x17e) {
            depth = 0x17e;
        }
        battle_effect_submit_sprite_to_ordering_table((battle_effect_sprite_part_set_t*)node->sprite_block, position,
            node->screen_rotation_angle, 0, g_battle_effect_otag + depth);
        break;
    }
}
