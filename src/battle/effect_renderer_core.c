/* Battle heap, effect render state, work records, emitters and interpolation. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/map.h"
#include "psx/gte_inline_split.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Turns buffer into a single free block that links to itself and empties the
 * per-owner lists; the effect stage calls this with the effect palette
 * buffer. */
void battle_heap_init(battle_heap_node_t* buffer, u32 buffer_size) {
    s32 owner;
    s16 block_count;

    block_count = (buffer_size >> 3) - 1;
    g_battle_heap_rover = buffer;
    g_battle_heap_base = buffer;
    buffer->size = block_count;
    g_battle_heap_block_count = block_count;
    buffer->next = buffer;
    for (owner = 16; owner >= 0; owner--) {
        g_battle_heap_owner_lists[owner].head = 0;
    }
}

/* Allocate `size` bytes from the battle heap for owner list `kind` (0-15) and
 * return the address past the block header.
 *
 * First fit along the circular free list, starting after the rover as in the K&R
 * malloc: the request is rounded up to 8-byte units plus one unit for the header,
 * an exact fit is unlinked and a larger block gives up its tail. The block goes
 * to the head of the owner's list. There is no failure exit: when no free block
 * is large enough the search goes round the list without end. */
void* battle_heap_alloc_block(u32 size, s32 kind) {
    battle_heap_node_t* prev;
    battle_heap_node_t* node;
    u32 blocks;

    blocks = (size + 15) >> 3;
    prev = g_battle_heap_rover;
    node = prev->next;
    for (;;) {
        if (node->size >= blocks) {
            if (node->size == blocks) {
                prev->next = node->next;
            } else {
                node->size = node->size - blocks;
                node = (battle_heap_node_t*)((u8*)node + node->size * 8);
                node->size = blocks;
            }
            node->owner = kind;
            node->next = g_battle_heap_owner_lists[kind].head;
            g_battle_heap_rover = prev;
            g_battle_heap_owner_lists[kind].head = node;
            return node + 1;
        } else {
            prev = node;
            node = node->next;
        }
    }
}

/*
 * Release a block from battle_heap_alloc_block.
 *
 * The block is unlinked from its owner's list, then returned to the circular
 * free list in address order and coalesced with both neighbors, as in the
 * K&R free().
 */
void battle_heap_free_block(void* ptr) {
    battle_heap_node_t* node;
    battle_heap_owner_list_t* list;
    battle_heap_node_t* owned;
    battle_heap_node_t* prev;
    battle_heap_node_t* free_node;

    node = (battle_heap_node_t*)ptr - 1;
    list = &g_battle_heap_owner_lists[node->owner];
    owned = list->head;
    if (owned == node) {
        list->head = node->next;
    } else {
        for (prev = owned; (owned = prev->next) != node; prev = owned) { }
        prev->next = node->next;
    }

    for (free_node = g_battle_heap_rover; !(node > free_node && node < free_node->next); free_node = free_node->next) {
        if (free_node >= free_node->next && (node > free_node || node < free_node->next)) {
            break;
        }
    }
    if (node + node->size == free_node->next) {
        node->size += free_node->next->size;
        node->next = free_node->next->next;
    } else {
        node->next = free_node->next;
    }
    if (free_node + free_node->size == node) {
        free_node->size += node->size;
        free_node->next = node->next;
    } else {
        free_node->next = node;
    }
    g_battle_heap_rover = free_node;
}

/*
 * Debug-print battle heap usage: total and largest free block on the free
 * ring, the low-water mark of the largest block, a "DP" value, and the bytes
 * held by each of the 16 allocation owners.
 *
 * The ring-end test rereads g_battle_heap_rover; GCC hoists that load, which
 * produces the target copy of the rover into a separate register.
 */
void battle_heap_print_stats(void) {
    battle_heap_node_t* free_node;
    battle_heap_node_t* node;
    s32 total;
    s32 largest;
    s32 owner;
    s32 used;
    s32 size;

    total = 0;
    largest = 0;
    free_node = g_battle_heap_rover->next;
    for (;;) {
        size = free_node->size;
        total += size;
        if (largest < size) {
            largest = size;
        }
        if (free_node == g_battle_heap_rover) {
            break;
        }
        free_node = free_node->next;
    }
    FntPrint(g_battle_text_heap_remain_format, total * 8, largest * 8);
    if (largest < g_battle_heap_min_largest_free) {
        g_battle_heap_min_largest_free = largest;
    }
    FntPrint(g_battle_text_min_heap_format, g_battle_heap_min_largest_free * 8);
    FntPrint(g_battle_text_heap_dp_format, g_battle_unknown_801bacc4 * 4 + g_battle_unknown_801bc0d8);
    for (owner = 0; owner < 16; owner++) {
        used = 0;
        for (node = g_battle_heap_owner_lists[owner].head; node != 0; node = node->next) {
            used += node->size;
        }
        if (used != 0) {
            FntPrint(g_battle_text_heap_task_format, owner, used * 8);
        }
    }
}

/* Initialize the effect renderer's transform, depth, and primitive state.
 *
 * The rotation terms form the identity matrix's third row and column. Every
 * corner starts at the same screen depth before an effect supplies geometry. */
void effect_init_render_state(void) {
    g_effect_matrix.m[2][2] = ONE;
    g_effect_frame_parity = 0;
    g_effect_matrix.m[2][1] = 0;
    g_effect_matrix.m[2][0] = 0;
    g_effect_matrix.m[1][2] = 0;
    g_effect_matrix.m[0][2] = 0;
    g_effect_matrix.t[2] = 0;
    g_effect_corner_bottom_right.vz = EFFECT_CORNER_Z_DEFAULT;
    g_effect_corner_bottom_left.vz = EFFECT_CORNER_Z_DEFAULT;
    g_effect_corner_top_right.vz = EFFECT_CORNER_Z_DEFAULT;
    g_effect_corner_top_left.vz = EFFECT_CORNER_Z_DEFAULT;
    g_effect_prim_buffer_offset = 0;
    g_effect_frame_start_prim_buffer_offset = 0;
    g_effect_prim_buffer = battle_heap_alloc_block(EFFECT_PRIMITIVE_BUFFER_SIZE, 0);
    g_effect_sprite_count_peak = 0;
    g_effect_sprite_count = 0;
}

/* The subset of effect_init_render_state that does not touch the
 * prim buffer or the sprite counters. */
void effect_reset_render_state(void) {
    g_effect_matrix.m[2][2] = ONE;
    g_effect_matrix.m[2][1] = 0;
    g_effect_matrix.m[2][0] = 0;
    g_effect_matrix.m[1][2] = 0;
    g_effect_matrix.m[0][2] = 0;
    g_effect_matrix.t[2] = 0;
    g_effect_corner_bottom_right.vz = EFFECT_CORNER_Z_DEFAULT;
    g_effect_corner_bottom_left.vz = EFFECT_CORNER_Z_DEFAULT;
    g_effect_corner_top_right.vz = EFFECT_CORNER_Z_DEFAULT;
    g_effect_corner_top_left.vz = EFFECT_CORNER_Z_DEFAULT;
}

/* Nothing in the game calls it. */
s32 effect_get_prim_buffer_offset(void) {
    return g_effect_prim_buffer_offset;
}

/* Allocate the variable-length sprite block for one effect frame.
 *
 * The eight-byte prefix is followed by one pointer per sprite part. The renderer tracks
 * both current and peak occupancy for the shared effect pool. */
effect_sprite_block_t* effect_alloc_sprite_block(u8 sprite_count) {
    effect_sprite_block_t* block;
    s32 total_count;

    block = battle_heap_alloc_block(sprite_count * sizeof(block->frames[0]) + sizeof(*block), 0);
    block->header.sprite_count = sprite_count;
    block->header.color.b = EFFECT_COLOR_NEUTRAL;
    block->header.color.g = EFFECT_COLOR_NEUTRAL;
    block->header.color.r = EFFECT_COLOR_NEUTRAL;
    total_count = g_effect_sprite_count + sprite_count;
    g_effect_sprite_count = total_count;
    if (g_effect_sprite_count_peak < total_count) {
        g_effect_sprite_count_peak = total_count;
    }
    return block;
}

/* Release an effect-frame sprite block and update the live sprite count. */
void effect_free_sprite_block(effect_sprite_block_t* block) {
    g_effect_sprite_count -= block->header.sprite_count;
    battle_heap_free_block(block);
}

/* Begin one effect-rendering frame.
 *
 * The renderer alternates packet storage and remembers the arena offset so
 * the frame's transient primitives can be reclaimed together. */
void effect_start_render_frame(void) {
    SetGeomOffset(0, 0);
    g_effect_frame_parity = 1 - g_effect_frame_parity;
    g_effect_frame_start_prim_buffer_offset = g_effect_prim_buffer_offset;
}

/* Allocate one typed primitive from the effect renderer's circular arena.
 *
 * The original renderer wraps before an allocation that would cross the
 * 0x8000-byte arena boundary; callers consume the result immediately. */
void* effect_alloc_primitive(effect_primitive_type_e primitive_type) {
    /* These registers retain the arena offset and primitive size across the wrap test. */
    s32 offset;
    register s32 size __asm__("$4");
    register s32 size_byte_offset __asm__("$2");
    s32 remaining;

    offset = g_effect_prim_buffer_offset;
    /* Preserve the arena offset load before indexing the size table. */
    __asm__("" : "=r"(offset) : "0"(offset));
    size_byte_offset = primitive_type << 2;
    /* Byte indexing keeps the table address in $v0 before the size load. */
    size = *(s32*)((u8*)g_effect_primitive_sizes + size_byte_offset);
    /* Keep the size load ahead of the buffer-limit constant. */
    __asm__("" : "=r"(size) : "0"(size));
    remaining = EFFECT_PRIMITIVE_BUFFER_SIZE - offset;
    if (remaining < size) {
        g_effect_prim_buffer_offset = size;
        offset = 0;
    } else {
        remaining = size + offset;
        g_effect_prim_buffer_offset = remaining;
    }
    return g_effect_prim_buffer + offset;
}

/* Queues a multi-part effect sprite at a screen position: brackets the parts
 * with two draw-mode primitives, builds the 2D rotation (optionally scaled by
 * `zoom`) into the effect matrix, and projects each part's corners through the
 * GTE into a POLY_FT4 linked into `ot`. */
void effect_submit_sprite_to_ordering_table(
    effect_sprite_block_t* set, DVECTOR* position, s16 angle, VECTOR* zoom, u32* ot) {
    DR_MODE* draw_mode;
    POLY_FT4* quad;
    effect_sprite_part_t* frame;
    s32 i;
    s16 cosine;
    s16 sine;
    s32 value;
    s32 clut;
    s32 code;

    draw_mode = effect_alloc_primitive(EFFECT_PRIMITIVE_DR_MODE);
    SetDrawMode(draw_mode, 1, 1, 1, 0);
    addPrim(ot, draw_mode);
    g_effect_matrix.t[0] = position->vx;
    g_effect_matrix.t[1] = position->vy;
    cosine = rcos(angle);
    sine = rsin(angle);
    if (zoom != 0) {
        g_effect_matrix.m[0][0] = (cosine * zoom->vx) >> 12;
        g_effect_matrix.m[1][1] = (cosine * zoom->vy) >> 12;
        g_effect_matrix.m[0][1] = (-sine * zoom->vy) >> 12;
        g_effect_matrix.m[1][0] = (sine * zoom->vx) >> 12;
    } else {
        g_effect_matrix.m[1][0] = sine;
        g_effect_matrix.m[1][1] = cosine;
        g_effect_matrix.m[0][0] = cosine;
        g_effect_matrix.m[0][1] = -sine;
    }
    SetRotMatrix(&g_effect_matrix);
    SetTransMatrix(&g_effect_matrix);
    for (i = 0; i < set->header.sprite_count; i++) {
        frame = set->frames[i];
        code = 0x2c;
        /* Copy x/y while retaining the GTE input's z. */
        memcpy(&g_effect_corner_top_left, &frame->corners[0], sizeof(frame->corners[0]));
        memcpy(&g_effect_corner_top_right, &frame->corners[1], sizeof(frame->corners[1]));
        memcpy(&g_effect_corner_bottom_left, &frame->corners[2], sizeof(frame->corners[2]));
        gte_ldv3_split(&g_effect_corner_top_left, &g_effect_corner_top_right, &g_effect_corner_bottom_left);
        gte_rtpt_split();
        memcpy(&g_effect_corner_bottom_right, &frame->corners[3], sizeof(frame->corners[3]));
        quad = effect_alloc_primitive(EFFECT_PRIMITIVE_FT4);
        /* The fourth copied byte is the part count; code overwrites it. */
        memcpy(&quad->r0, &set->header, sizeof(set->header));
        setlen(quad, 9);
        quad->code = code;
        if (frame->flags & EFFECT_SPRITE_PART_FLAG_SEMI_TRANSPARENT) {
            quad->code = 0x2e;
        }
        gte_stsxy3_split(&quad->x0, &quad->x1, &quad->x2);
        gte_ldv0_split(&g_effect_corner_bottom_right);
        gte_rtps_split();
        quad->tpage = frame->tpage;
        value = frame->flags & EFFECT_SPRITE_PART_CLUT_MASK;
        if (frame->flags & EFFECT_SPRITE_PART_FLAG_SECOND_CLUT_ROW) {
            clut = value + MAP_CLUT(MAP_CLUT_ROW_EFFECT_SECOND, 0);
        } else {
            clut = value + MAP_CLUT(MAP_CLUT_ROW_EFFECT, 0);
        }
        quad->clut = clut;
        quad->u0 = quad->u2 = frame->u;
        quad->u1 = quad->u3 = quad->u0 + frame->width;
        quad->v0 = quad->v1 = frame->v;
        quad->v2 = quad->v3 = quad->v0 + frame->height;
        gte_stsxy_split(&quad->x3);
        addPrim(ot, quad);
    }
    draw_mode = effect_alloc_primitive(EFFECT_PRIMITIVE_DR_MODE);
    SetDrawMode(draw_mode, 1, 0, 1, 0);
    addPrim(ot, draw_mode);
}

/* Queues one effect slot's billboard sprite: projects the slot origin for its
 * ordering-table depth, builds the rotated, camera-scaled 2D matrix from the
 * slot angle, transforms the frame's four corners into the slot's
 * double-buffered POLY_FT4, and adds that quad between the slot's two draw-mode
 * primitives. */
void effect_draw_billboard_sprite(effect_slot_t* slot) {
    SVECTOR local;
    VECTOR position;
    s32 sxy;
    s32 flag;
    POLY_FT4* prim;
    effect_sprite_frameset_t* frame;
    u16 offset;
    effect_sprite_group_t* frame_group;
    s32 depth;
    s32 sine;
    s32 cosine;
    s32 zoom;
    s32 scale_y;
    u8 corner_u;
    u8 corner_v;
    u8 right;
    u8 bottom;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    setVector(&local, slot->motion.position.x >> 12, slot->motion.position.y >> 12, slot->motion.position.z >> 12);
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
    cosine = rcos(slot->angle);
    sine = rsin(slot->angle);
    zoom = g_camera_zoom.vx;
    scale_y = g_camera_zoom.vy;
    g_effect_matrix.m[0][0] = (cosine * zoom) >> 12;
    g_effect_matrix.m[1][1] = (cosine * scale_y) >> 12;
    g_effect_matrix.m[0][1] = (-sine * scale_y) >> 12;
    g_effect_matrix.m[1][0] = (sine * zoom) >> 12;
    SetRotMatrix(&g_effect_matrix);
    SetTransMatrix(&g_effect_matrix);

    frame_group = g_effect_trap_frame_data.groups;
    offset = *(u16*)((u8*)frame_group + (slot->frame * 2 + frame_group->frame_table_offset));
    prim = &slot->quads[g_effect_buffer_index];
    /* Copy RGB and the frame byte; the packet header overwrites that last byte. */
    memcpy(&prim->r0, &slot->color.r, sizeof(CVECTOR));
    setPolyFT4(prim);
    frame = (effect_sprite_frameset_t*)((u8*)frame_group + offset);
    setSemiTrans(prim, frame->flags & EFFECT_SPRITE_PART_FLAG_SEMI_TRANSPARENT);
    /* Copy x/y while retaining the GTE input's z. */
    memcpy(&g_effect_corner_top_left, &frame->parts[0].corners[0], sizeof(frame->parts[0].corners[0]));
    memcpy(&g_effect_corner_top_right, &frame->parts[0].corners[1], sizeof(frame->parts[0].corners[1]));
    memcpy(&g_effect_corner_bottom_left, &frame->parts[0].corners[2], sizeof(frame->parts[0].corners[2]));
    memcpy(&g_effect_corner_bottom_right, &frame->parts[0].corners[3], sizeof(frame->parts[0].corners[3]));
    /* The GTE returns each x/y pair as one packed 32-bit value. */
    RotTransPers3(&g_effect_corner_top_left, &g_effect_corner_top_right, &g_effect_corner_bottom_left, (s32*)&prim->x0,
        (s32*)&prim->x1, (s32*)&prim->x2, &sxy, &flag);
    RotTransPers(&g_effect_corner_bottom_right, (s32*)&prim->x3, &sxy, &flag);
    prim->tpage = (frame->flags & EFFECT_SPRITE_PART_TPAGE_MODE_MASK) | 8;
    prim->clut = slot->clut;
    corner_u = frame->parts[0].u;
    prim->u2 = corner_u;
    prim->u0 = corner_u;
    right = corner_u + frame->parts[0].width;
    prim->u3 = right;
    prim->u1 = right;
    corner_v = frame->parts[0].v;
    prim->v1 = corner_v;
    prim->v0 = corner_v;
    bottom = corner_v + frame->parts[0].height;
    prim->v3 = bottom;
    prim->v2 = bottom;
    SetDrawMode(&slot->modes[g_effect_buffer_index][0], 1, 1, 1, 0);
    AddPrim(&g_effect_polygon_depth_data[depth], &slot->modes[g_effect_buffer_index][0]);
    AddPrim(&g_effect_polygon_depth_data[depth], prim);
    SetDrawMode(&slot->modes[g_effect_buffer_index][1], 1, 0, 1, 0);
    AddPrim(&g_effect_polygon_depth_data[depth], &slot->modes[g_effect_buffer_index][1]);
}

/* Allocate the 256 effect work records and chain them all into the free list.
 * Also clears the translation of the particle heading and emitter matrices. */
void effect_init_record_chain(void) {
    s32 index;
    effect_work_record_t* records;

    g_effect_particle_heading_matrix.t[2] = 0;
    g_effect_particle_heading_matrix.t[1] = 0;
    g_effect_particle_heading_matrix.t[0] = 0;
    g_effect_emitter_matrix.t[2] = 0;
    g_effect_emitter_matrix.t[1] = 0;
    g_effect_emitter_matrix.t[0] = 0;
    records = battle_heap_alloc_block(EFFECT_WORK_RECORD_CAPACITY * sizeof(effect_work_record_t), 0);
    g_effect_work_records = records;
    g_effect_free_work_record_head = records;
    g_battle_unknown_801bc0c4 = 0;
    records[0].prev = 0;
    records[0].next = &records[1];
    for (index = 1; index < EFFECT_WORK_RECORD_CAPACITY - 1; index++) {
        g_effect_work_records[index].next = &g_effect_work_records[index + 1];
        g_effect_work_records[index].prev = &g_effect_work_records[index - 1];
    }
    g_effect_work_record_peak = 0;
    g_effect_work_record_count = 0;
    g_effect_work_records[index].prev = &g_effect_work_records[index - 1];
    g_effect_work_records[index].next = 0;
}

/*
 * Take a work record from the free chain and attach it to state record
 * record_index; the inverse of effect_free_work_record.
 *
 * Bits 12-15 of kind select the effect list node type allocated as the
 * record's data (0x0000-0x3000 map to list kinds 0, 2, 4 and 6); other kinds
 * get no data. g_effect_work_record_peak tracks the peak number of records in use.
 */
effect_work_record_t* effect_alloc_work_record(s16 record_index, s16 kind) {
    effect_work_record_t* work;
    effect_record_t* owner;

    work = g_effect_free_work_record_head;
    g_effect_work_record_count++;
    owner = &g_effect_state_records[record_index];
    if (g_effect_work_record_peak < g_effect_work_record_count) {
        g_effect_work_record_peak = g_effect_work_record_count;
    }
    if (work == 0) {
        main_system_report_stub(1, 0x3E);
    }
    g_effect_free_work_record_head = work->next;
    switch (kind & EFFECT_WORK_KIND_MASK) {
    case EFFECT_WORK_KIND_SPRITE_WORLD:
        work->data = effect_alloc_list_node(EFFECT_LIST_NODE_WORLD);
        break;
    case EFFECT_WORK_KIND_SPRITE_SCREEN_OFFSET:
        work->data = effect_alloc_list_node(EFFECT_LIST_NODE_SCREEN_OFFSET);
        break;
    case EFFECT_WORK_KIND_SPRITE_WORLD_UNSCALED:
        work->data = effect_alloc_list_node(EFFECT_LIST_NODE_WORLD_UNSCALED);
        break;
    case EFFECT_WORK_KIND_SPRITE_SCREEN:
        work->data = effect_alloc_list_node(EFFECT_LIST_NODE_SCREEN);
        break;
    }
    work->prev = 0;
    work->next = owner->work_head;
    if (owner->work_head != 0) {
        owner->work_head->prev = work;
    }
    owner->work_head = work;
    owner->work_count++;
    return work;
}

/* Unlinks work from state record record_index, releases its data according
 * to the kind in kind_flags, and returns it to the free chain. */
void effect_free_work_record(s16 record_index, effect_work_record_t* work) {
    effect_record_t* owner;

    owner = &g_effect_state_records[record_index];
    if (work->next != 0) {
        work->next->prev = work->prev;
    }
    if (work->prev != 0) {
        work->prev->next = work->next;
    } else {
        owner->work_head = work->next;
    }
    owner->work_count--;
    switch (work->kind_flags & EFFECT_WORK_KIND_MASK) {
    case EFFECT_WORK_KIND_SPRITE_WORLD:
    case EFFECT_WORK_KIND_SPRITE_SCREEN_OFFSET:
    case EFFECT_WORK_KIND_SPRITE_WORLD_UNSCALED:
    case EFFECT_WORK_KIND_SPRITE_SCREEN:
        effect_free_list_node(work->data);
        break;
    case EFFECT_WORK_KIND_HEAP_BLOCK:
        battle_heap_free_block(work->data);
        break;
    }
    work->next = g_effect_free_work_record_head;
    g_effect_free_work_record_head = work;
    g_effect_work_record_count--;
}

/* Copy a work record's position, heading, and color onto its list node.
 *
 * With kind_flags bit 1 the velocity is rotated into camera space and its
 * screen heading stored on the node. Color-curve records take each channel
 * from its animation curve at the record's curve frame; others use neutral
 * 0x80. */
void effect_update_node_from_work(s16 record_index, effect_work_record_t* work) {
    SVECTOR direction;
    VECTOR rotated;

    work->data->x = work->motion.position.x >> 12;
    work->data->y = work->motion.position.y >> 12;
    work->data->z = work->motion.position.z >> 12;
    if (work->kind_flags & EFFECT_EMITTER_ORIENT_TO_VELOCITY) {
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_effect_particle_heading_matrix);
        setVector(&direction, work->motion.velocity.x >> 3, work->motion.velocity.y >> 3, work->motion.velocity.z >> 3);
        /* The 32-bit flag value lands in the output vector's 32-bit pad field; the target frame
         * has no separate flag slot. */
        RotTrans(&direction, &rotated, &rotated.pad);
        work->data->screen_rotation_angle = ratan2(rotated.vy, rotated.vx);
    }
    if (work->flags & EFFECT_EMITTER_FLAG_COLOR_CURVES) {
        effect_render_particle_sprite(work->data,
            g_effect_animation_curve_data->curves[work->color_curves.r][work->curve_frame],
            g_effect_animation_curve_data->curves[work->color_curves.g][work->curve_frame],
            g_effect_animation_curve_data->curves[work->color_curves.b][work->curve_frame]);
    } else {
        effect_render_particle_sprite(work->data, 0x80, 0x80, 0x80);
    }
}

void battle_noop_801a6010(void) {
}

void battle_noop_801a6018(void) {
}

/* Returns 0 unless the record has a work list and requested_index is inside
 * it.  Note the retail loop walks work_count links, not requested_index, so
 * the returned node is always the same for a given record. */
effect_work_record_t* effect_get_list_node(s16 record_index, s16 requested_index) {
    effect_record_t* record = &g_effect_state_records[record_index];
    effect_work_record_t* node = 0;
    s16 node_count;
    s16 i;

    if ((record->flags & EFFECT_RECORD_FLAG_ALLOCATED) != 0) {
        if (requested_index >= 0) {
            node_count = record->work_count;
            if (requested_index < node_count) {
                node = record->work_head;
                for (i = 0; i < node_count; i++)
                    node = node->next;
            }
        }
    }
    return node;
}

/* Spawns an emitter's particles for this frame and initializes their motion.
 *
 * Emitter emitter_index of the effect's particle-system section spawns every
 * spawn_interval frames of `frame`, which also picks the column of the
 * animation curves, modulo EFFECT_ANIMATION_CURVE_FRAMES. Each start/end pair
 * is lerped by its motion-curve binding, and the
 * per-particle values are drawn between the resulting minimum and maximum.
 * The particles belong to state record record_index and are placed for its
 * target; parent is the particle that the PARENT_PARTICLE origin spawns from.
 *
 * Positions have 12 fractional bits; initial speed values scale into velocity by eight.
 * The lerps of the pair at 0x4c and the target's facing quadrant are discarded. */
s32 effect_spawn_emitter_particles(s16 record_index, s32 frame, s32 emitter_index, effect_work_record_t* parent) {
    SVECTOR rotation;
    VECTOR out;
    SVECTOR caster;
    MATRIX matrix;
    SVECTOR target;
    VECTOR spawn;
    VECTOR spread;
    VECTOR angle;
    VECTOR angle_spread;
    SVECTOR accel_min;
    SVECTOR accel_max;
    SVECTOR accel_step_min;
    SVECTOR accel_step_max;
    SVECTOR target_offset;
    SVECTOR map_max;
    SVECTOR tile_anchor;
    const effect_geometry_record_t* entry;
    effect_work_record_t* particle;
    SVECTOR* unit_coords;
    s32 time;
    s32 selector;
    u16 inertia_binding;
    u8 weight_binding;
    s32 discarded_selector;
    s32 curve;
    s32 factor;
    s32 i;
    u8 target_index;
    s16 scale;
    s16 speed_min;
    s16 speed_max;
    s16 speed;
    s16 facing;
    u32 motion_curve_selectors;
    s32 count;
    effect_work_record_t* head;
    u16 placement_flags;
    u16 behavior_flags;
    s32 rotate;
    s16 inertia_min;
    s16 inertia_max;
    s16 weight_min;
    s16 weight_max;
    s16 lifetime_min;
    s16 lifetime_max;
    s16 homing_min;
    s16 homing_max;
    u16 homing_curve_selector;
    u8 color_r;
    u8 color_g;
    u8 color_b;
    map_tile_t* tile;

    entry = &g_effect_geometry_table->entries[emitter_index];
    time = frame;
    frame = frame % EFFECT_ANIMATION_CURVE_FRAMES;
    placement_flags = entry->emitter.placement_flags;
    behavior_flags = entry->emitter.flags;
    /* Keep the binding word packed: the first lookup loads its last byte,
     * later lookups and the homing setup load the complete word. */
    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_SPAWN_INTERVAL_SHIFT & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    if (time % battle_lerp_fixed8(entry->emitter.spawn_interval_start, entry->emitter.spawn_interval_end, factor)
        != 0) {
        return;
    }

    SetTransMatrix(&g_effect_emitter_matrix);
    target_index = g_effect_state_records[record_index].timeline.target_index;
    rotate = (behavior_flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING;
    effect_resolve_target_world_coords(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
        EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &caster);
    if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
        facing = ((battle_unit_get_facing_quadrant_by_runtime_id(
                       g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id)
                      + 1)
                     & 3)
            << 10;
        rotation.vz = 0;
        rotation.vx = 0;
        rotation.vy = facing;
        RotMatrix(&rotation, &matrix);
    }
    effect_resolve_target_world_coords(
        g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, target_index, &g_effect_context, &target);
    if (g_effect_context.targets[target_index].target_type == EFFECT_TARGET_TYPE_UNIT) {
        battle_unit_get_facing_quadrant_by_runtime_id(g_effect_context.targets[target_index].data.unit.runtime_id);
        unit_coords
            = battle_unit_get_event_offset_by_runtime_id(g_effect_context.targets[target_index].data.unit.runtime_id);
    }
    tile = map_get_tile_data_pointer(
        g_effect_target_tile.data.tile.vx, g_effect_target_tile.data.tile.vz, g_effect_target_tile.data.tile.vy);

    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_PARTICLE_COUNT_SHIFT & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    count = battle_lerp_fixed8(entry->emitter.particle_count_start, entry->emitter.particle_count_end, factor);
    for (i = 0; i < count; i++) {
        particle = effect_alloc_work_record(record_index, (placement_flags & EFFECT_WORK_KIND_MASK));
        particle->kind_flags = placement_flags;
        particle->flags = behavior_flags;
        particle->curve_frame = 0;
        effect_init_animation_sequence(
            particle->data, entry->emitter.animation_frame_group, entry->emitter.animation_index);
    }
    head = particle;

    curve = entry->emitter.spawn_bindings.position;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    effect_interpolate_emitter_spawn_position(entry, factor, &spawn);
    if (rotate) {
        SetRotMatrix(&matrix);
        copyVector(&rotation, &spawn);
        RotTrans(&rotation, &spawn, &out.pad);
    }
    switch (placement_flags & EFFECT_EMITTER_ORIGIN_MASK) {
    case EFFECT_EMITTER_ORIGIN_WORLD:
        break;
    case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
        spawn.vx += g_effect_target_tile.data.tile.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        spawn.vz += g_effect_target_tile.data.tile.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        spawn.vy -= tile->height * 12;
        break;
    case EFFECT_EMITTER_ORIGIN_CASTER:
        effect_add_svector_to_vector(&caster, &spawn, &spawn);
        break;
    case EFFECT_EMITTER_ORIGIN_TARGET:
        effect_add_svector_to_vector(&target, &spawn, &spawn);
        break;
    case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
        if (parent != 0) {
            spawn.vx += parent->motion.position.x >> 12;
            spawn.vy += parent->motion.position.y >> 12;
            spawn.vz += parent->motion.position.z >> 12;
        }
        break;
    case EFFECT_EMITTER_ORIGIN_MAP_CENTER:
        map_store_max_coordinates(&map_max);
        spawn.vx += map_max.vx * (MAP_TILE_SIZE / 2);
        spawn.vz += map_max.vz * (MAP_TILE_SIZE / 2);
        break;
    case EFFECT_EMITTER_ORIGIN_TRACKED_ENTITY:
        spawn.vx += target.vx + unit_coords->vx;
        spawn.vy += target.vy + unit_coords->vy;
        spawn.vz += target.vz + unit_coords->vz;
        break;
    }

    selector = entry->emitter.spawn_bindings.spread;
    curve = selector;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    effect_interpolate_emitter_spawn_spread(entry, factor, &spread);
    if (rotate) {
        SetRotMatrix(&matrix);
        copyVector(&rotation, &spread);
        RotTrans(&rotation, &spread, &out.pad);
    }
    particle = head;
    if (placement_flags & EFFECT_EMITTER_BOX_SPREAD) {
        for (i = 0; i < count; i++) {
            particle->motion.position.x = (spawn.vx + RANDOM_BETWEEN(spread.vx, -spread.vx)) * ONE;
            particle->motion.position.y = (spawn.vy + RANDOM_BETWEEN(spread.vy, -spread.vy)) * ONE;
            particle->motion.position.z = (spawn.vz + RANDOM_BETWEEN(spread.vz, -spread.vz)) * ONE;
            particle = particle->next;
        }
    } else {
        scale = effect_select_largest_magnitude_s16(spread.vx, spread.vy, spread.vz);
        for (i = 0; i < count; i++) {
            if (scale != 0) {
                setVector(&rotation, rand(), rand(), rand());
                RotMatrix(&rotation, &g_effect_emitter_matrix);
                SetRotMatrix(&g_effect_emitter_matrix);
                rotation.vx = scale;
                rotation.vz = 0;
                rotation.vy = 0;
                RotTrans(&rotation, &out, &out.pad);
                particle->motion.position.x = ((out.vx * spread.vx) / scale + spawn.vx) * ONE;
                particle->motion.position.y = ((out.vy * spread.vy) / scale + spawn.vy) * ONE;
                particle->motion.position.z = ((out.vz * spread.vz) / scale + spawn.vz) * ONE;
            } else {
                particle->motion.position.x = spawn.vx * ONE;
                particle->motion.position.y = spawn.vy * ONE;
                particle->motion.position.z = spawn.vz * ONE;
            }
            particle = particle->next;
        }
    }

    curve = entry->emitter.spawn_bindings.radial_speed;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
        speed_min
            = battle_lerp_fixed8(entry->emitter.radial_speed_start.min, entry->emitter.radial_speed_end.min, factor);
        speed_max
            = battle_lerp_fixed8(entry->emitter.radial_speed_start.max, entry->emitter.radial_speed_end.max, factor);
    } else {
        speed_min = entry->emitter.radial_speed_start.min;
        speed_max = entry->emitter.radial_speed_start.max;
    }
    selector = entry->emitter.spawn_bindings.velocity_spread;
    curve = selector;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
        angle_spread.vx = battle_lerp_fixed8(
            entry->emitter.spawn.velocity_spread_start.x, entry->emitter.spawn.velocity_spread_end.x, factor);
        angle_spread.vy = battle_lerp_fixed8(
            entry->emitter.spawn.velocity_spread_start.y, entry->emitter.spawn.velocity_spread_end.y, factor);
        angle_spread.vz = battle_lerp_fixed8(
            entry->emitter.spawn.velocity_spread_start.z, entry->emitter.spawn.velocity_spread_end.z, factor);
    } else {
        setVector(&angle_spread, entry->emitter.spawn.velocity_spread_start.x / 2,
            entry->emitter.spawn.velocity_spread_start.y / 2, entry->emitter.spawn.velocity_spread_start.z / 2);
    }
    selector = entry->emitter.spawn_bindings.velocity_angle;
    curve = selector;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    effect_interpolate_emitter_velocity_base_angles(entry, factor, &angle);

    particle = head;
    switch (behavior_flags & EFFECT_EMITTER_VELOCITY_MASK) {
    case EFFECT_EMITTER_VELOCITY_OUTWARD:
        for (i = 0; i < count; i++) {
            setVector(&rotation, angle.vx + RANDOM_BETWEEN(angle_spread.vx, -angle_spread.vx),
                angle.vy + RANDOM_BETWEEN(angle_spread.vy, -angle_spread.vy),
                angle.vz + RANDOM_BETWEEN(angle_spread.vz, -angle_spread.vz));
            RotMatrix(&rotation, &g_effect_emitter_matrix);
            SetRotMatrix(&g_effect_emitter_matrix);
            rotation.vz = 0;
            rotation.vx = 0;
            rotation.vy = RANDOM_BETWEEN(speed_min, speed_max);
            RotTrans(&rotation, &out, &out.pad);
            particle->motion.velocity.x = out.vx * 8;
            particle->motion.velocity.y = out.vy * 8;
            particle->motion.velocity.z = out.vz * 8;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_VELOCITY_CASTER_FACING:
        for (i = 0; i < count; i++) {
            setVector(&rotation, angle.vx + RANDOM_BETWEEN(angle_spread.vx, -angle_spread.vx),
                angle.vy + RANDOM_BETWEEN(angle_spread.vy, -angle_spread.vy),
                angle.vz + RANDOM_BETWEEN(angle_spread.vz, -angle_spread.vz));
            RotMatrix(&rotation, &g_effect_emitter_matrix);
            SetRotMatrix(&g_effect_emitter_matrix);
            rotation.vz = 0;
            rotation.vx = 0;
            rotation.vy = RANDOM_BETWEEN(speed_min, speed_max);
            RotTrans(&rotation, &out, &out.pad);
            SetRotMatrix(&matrix);
            copyVector(&rotation, &out);
            RotTrans(&rotation, &out, &out.pad);
            particle->motion.velocity.x = out.vx * 8;
            particle->motion.velocity.y = out.vy * 8;
            particle->motion.velocity.z = out.vz * 8;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_VELOCITY_TOWARD_TARGET:
        break;
    case EFFECT_EMITTER_VELOCITY_INWARD:
        for (i = 0; i < count; i++) {
            setVector(&out, spawn.vx - (particle->motion.position.x >> 12),
                spawn.vy - (particle->motion.position.y >> 12), spawn.vz - (particle->motion.position.z >> 12));
            speed = RANDOM_BETWEEN(speed_min, speed_max);
            if (out.vx != 0 || out.vy != 0 || out.vz != 0) {
                VectorNormal(&out, &out);
                particle->motion.velocity.x = (speed * out.vx) >> 9;
                particle->motion.velocity.y = (speed * out.vy) >> 9;
                particle->motion.velocity.z = (speed * out.vz) >> 9;
            } else {
                particle->motion.velocity.y = speed * 8;
                particle->motion.velocity.z = 0;
                particle->motion.velocity.x = 0;
            }
            particle = particle->next;
        }
        break;
    }

    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_ACCELERATION_SHIFT & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    accel_min.vx
        = battle_lerp_fixed8(entry->emitter.acceleration_start.x.min, entry->emitter.acceleration_end.x.min, factor);
    accel_max.vx
        = battle_lerp_fixed8(entry->emitter.acceleration_start.x.max, entry->emitter.acceleration_end.x.max, factor);
    accel_min.vy
        = battle_lerp_fixed8(entry->emitter.acceleration_start.y.min, entry->emitter.acceleration_end.y.min, factor);
    accel_max.vy
        = battle_lerp_fixed8(entry->emitter.acceleration_start.y.max, entry->emitter.acceleration_end.y.max, factor);
    accel_min.vz
        = battle_lerp_fixed8(entry->emitter.acceleration_start.z.min, entry->emitter.acceleration_end.z.min, factor);
    accel_max.vz
        = battle_lerp_fixed8(entry->emitter.acceleration_start.z.max, entry->emitter.acceleration_end.z.max, factor);
    if (rotate) {
        SetRotMatrix(&matrix);
        RotTrans(&accel_min, &out, &out.pad);
        copyVector(&accel_min, &out);
        RotTrans(&accel_max, &out, &out.pad);
        copyVector(&accel_max, &out);
    }
    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_ACCELERATION_STEP_SHIFT & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    accel_step_min.vx = battle_lerp_fixed8(
        entry->emitter.acceleration_step_start.x.min, entry->emitter.acceleration_step_end.x.min, factor);
    accel_step_max.vx = battle_lerp_fixed8(
        entry->emitter.acceleration_step_start.x.max, entry->emitter.acceleration_step_end.x.max, factor);
    accel_step_min.vy = battle_lerp_fixed8(
        entry->emitter.acceleration_step_start.y.min, entry->emitter.acceleration_step_end.y.min, factor);
    accel_step_max.vy = battle_lerp_fixed8(
        entry->emitter.acceleration_step_start.y.max, entry->emitter.acceleration_step_end.y.max, factor);
    accel_step_min.vz = battle_lerp_fixed8(
        entry->emitter.acceleration_step_start.z.min, entry->emitter.acceleration_step_end.z.min, factor);
    accel_step_max.vz = battle_lerp_fixed8(
        entry->emitter.acceleration_step_start.z.max, entry->emitter.acceleration_step_end.z.max, factor);
    if (rotate) {
        RotTrans(&accel_step_min, &out, &out.pad);
        copyVector(&accel_step_min, &out);
        RotTrans(&accel_step_max, &out, &out.pad);
        copyVector(&accel_step_max, &out);
    }
    particle = head;
    for (i = 0; i < count; i++) {
        particle->motion.acceleration.x = RANDOM_BETWEEN(accel_min.vx, accel_max.vx);
        particle->motion.acceleration.y = RANDOM_BETWEEN(accel_min.vy, accel_max.vy);
        particle->motion.acceleration.z = RANDOM_BETWEEN(accel_min.vz, accel_max.vz);
        particle->motion.acceleration_step.x = RANDOM_BETWEEN(accel_step_min.vx, accel_step_max.vx);
        particle->motion.acceleration_step.y = RANDOM_BETWEEN(accel_step_min.vy, accel_step_max.vy);
        particle->motion.acceleration_step.z = RANDOM_BETWEEN(accel_step_min.vz, accel_step_max.vz);
        particle = particle->next;
    }

    inertia_binding = entry->emitter.spawn_bindings.inertia;
    curve = inertia_binding;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    inertia_min
        = battle_lerp_fixed8(entry->emitter.spawn.inertia_start.min, entry->emitter.spawn.inertia_end.min, factor);
    inertia_max
        = battle_lerp_fixed8(entry->emitter.spawn.inertia_start.max, entry->emitter.spawn.inertia_end.max, factor);
    discarded_selector = entry->emitter.spawn_bindings._unknown_02;
    curve = discarded_selector;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    battle_lerp_fixed8(entry->emitter._unknown_4c[0], entry->emitter._unknown_50[0], factor);
    battle_lerp_fixed8(entry->emitter._unknown_4c[1], entry->emitter._unknown_50[1], factor);
    weight_binding = entry->emitter.spawn_bindings.weight;
    curve = weight_binding;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    weight_min = battle_lerp_fixed8(entry->emitter.weight_start.min, entry->emitter.weight_end.min, factor);
    weight_max = battle_lerp_fixed8(entry->emitter.weight_start.max, entry->emitter.weight_end.max, factor);
    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_LIFETIME_SHIFT & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    lifetime_min = battle_lerp_fixed8(entry->emitter.lifetime_start.min, entry->emitter.lifetime_end.min, factor);
    lifetime_max = battle_lerp_fixed8(entry->emitter.lifetime_start.max, entry->emitter.lifetime_end.max, factor);
    particle = head;
    for (i = 0; i < count; i++) {
        particle->motion.inertia = RANDOM_BETWEEN(inertia_min, inertia_max);
        particle->motion.weight = RANDOM_BETWEEN(weight_min, weight_max);
        particle->lifetime = RANDOM_BETWEEN(lifetime_min, lifetime_max);
        particle = particle->next;
    }

    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_TARGET_OFFSET_SHIFT & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    target_offset.vx
        = battle_lerp_fixed8(entry->emitter.target_offset_start.x, entry->emitter.target_offset_end.x, factor);
    target_offset.vy
        = battle_lerp_fixed8(entry->emitter.target_offset_start.y, entry->emitter.target_offset_end.y, factor);
    target_offset.vz
        = battle_lerp_fixed8(entry->emitter.target_offset_start.z, entry->emitter.target_offset_end.z, factor);
    particle = head;
    switch (placement_flags & EFFECT_EMITTER_HOMING_MASK) {
    case EFFECT_EMITTER_HOMING_OFFSET:
    case EFFECT_EMITTER_HOMING_OFFSET_ALT:
        for (i = 0; i < count; i++) {
            particle->target.x = target_offset.vx;
            particle->target.y = target_offset.vy;
            particle->target.z = target_offset.vz;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_HOMING_MAP_CENTER:
        map_store_max_coordinates(&map_max);
        for (i = 0; i < count; i++) {
            particle->target.x = map_max.vx * (MAP_TILE_SIZE / 2) + target_offset.vx;
            particle->target.z = map_max.vz * (MAP_TILE_SIZE / 2) + target_offset.vz;
            particle->target.y = target_offset.vy;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_HOMING_CASTER:
        for (i = 0; i < count; i++) {
            particle->target.x = caster.vx + target_offset.vx;
            particle->target.y = caster.vy + target_offset.vy;
            particle->target.z = caster.vz + target_offset.vz;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_HOMING_TARGET:
        for (i = 0; i < count; i++) {
            particle->target.x = target.vx + target_offset.vx;
            particle->target.y = target.vy + target_offset.vy;
            particle->target.z = target.vz + target_offset.vz;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_HOMING_CURSOR_TILE:
        tile_anchor.vx = g_effect_target_tile.data.tile.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        tile_anchor.vz = g_effect_target_tile.data.tile.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        tile_anchor.vy = -(tile->height * 12);
        for (i = 0; i < count; i++) {
            particle->target.x = tile_anchor.vx + target_offset.vx;
            particle->target.y = tile_anchor.vy + target_offset.vy;
            particle->target.z = tile_anchor.vz + target_offset.vz;
            particle = particle->next;
        }
        break;
    }

    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_HOMING_STRENGTH_SHIFT & 3;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    /* The lerp results pass through curve: that reassignment keeps CSE from
     * sign-extending the call results instead of homing_min/max. */
    curve
        = battle_lerp_fixed8(entry->emitter.homing_strength_start.min, entry->emitter.homing_strength_end.min, factor);
    homing_min = curve;
    curve
        = battle_lerp_fixed8(entry->emitter.homing_strength_start.max, entry->emitter.homing_strength_end.max, factor);
    homing_max = curve;
    /* This otherwise-unused test keeps the packed selectors live through the loop setup. */
    motion_curve_selectors = entry->emitter.motion_curve_selectors;
    homing_curve_selector = motion_curve_selectors >> EFFECT_EMITTER_CURVE_HOMING_MOTION_SHIFT;
    if (motion_curve_selectors != 0) {
        curve = motion_curve_selectors >> EFFECT_EMITTER_CURVE_HOMING_STRENGTH_SHIFT & 3;
    }
    color_r = entry->emitter.color_curves & 0xf;
    color_g = entry->emitter.color_curves >> 4 & 0xf;
    color_b = entry->emitter.color_curves >> 8 & 0xf;
    particle = head;
    for (i = 0; i < count; i++) {
        particle->homing_strength = RANDOM_BETWEEN(homing_min, homing_max);
        particle->homing_curve_selector = homing_curve_selector;
        particle->color_curves.r = color_r;
        particle->color_curves.g = color_g;
        particle->color_curves.b = color_b;
        if (behavior_flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK) {
            particle->child_emitter_on_death = entry->emitter.child_emitter_on_death;
        }
        if (behavior_flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) {
            particle->child_emitter_mid_life = entry->emitter.child_emitter_mid_life;
        }
        particle = particle->next;
    }
}

/* Spawns one particle's motion block from the interpolated emitter values and
 * returns its random lifetime.
 *
 * Position is the emitter offset plus a random point in the spread box, both
 * optionally rotated to face the caster or the target line (motion_flags 0x410
 * / 0x400) and anchored at the caster or target (anchor_flags 0x400 / 0x600).
 * Velocity is a random speed along the angle cone, or aimed back at the
 * position when motion_flags is 0x10. The caller stores the result as the
 * particle lifetime.
 *
 * The caster and target blocks go through a secondary-init record, as in the
 * secondary initializers; resolve_target_render_coords reads the same layout. */
s32 effect_spawn_particle_motion(effect_emitter_values_t* values, effect_motion_t* motion) {
    MATRIX anchor_matrix;
    MATRIX rotation;
    SVECTOR anchor_angle;
    SVECTOR vector;
    SVECTOR origin;
    SVECTOR target;
    VECTOR direction;
    VECTOR position;
    VECTOR spread;
    VECTOR angle;
    VECTOR half_spread;
    u8 _unused_c0[0x28]; /* reserved by the target frame, never referenced */
    effect_secondary_init_t init;
    s32 oriented;
    s16 scale;
    s16 speed_min;
    s16 speed_max;
    s16 speed;

    init.targets[EFFECT_CASTER_ENTRY] = g_effect_current_secondary->caster;
    effect_resolve_target_world_coords(0, EFFECT_TARGET_INDEX_CASTER, &init, &origin);
    init.targets[0] = g_effect_current_secondary->target.record;
    effect_resolve_target_world_coords(0, 0, &init, &target);
    oriented = 0;
    SetTransMatrix(&g_effect_projectile_matrix);
    if ((values->motion_flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING) {
        oriented = 1;
        anchor_angle.vy
            = ((battle_unit_get_facing_quadrant_by_runtime_id(g_effect_current_secondary->caster.data.unit.runtime_id)
                   + 1)
                  & 3)
            << 10;
        anchor_angle.vz = 0;
        anchor_angle.vx = 0;
        RotMatrix(&anchor_angle, &anchor_matrix);
    } else if ((values->motion_flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_TOWARD_TARGET) {
        direction.vz = target.vz - origin.vz;
        direction.vx = target.vx - origin.vx;
        direction.vy = target.vy - origin.vy;
        oriented = 1;
        anchor_angle.vy = ratan2(-direction.vz, direction.vx);
        anchor_angle.vz
            = ratan2(direction.vy, SquareRoot0(direction.vx * direction.vx + direction.vz * direction.vz)) + 0x400;
        anchor_angle.vx = 0;
        RotMatrix(&anchor_angle, &anchor_matrix);
    }

    setVector(&position, values->position.x, values->position.y, values->position.z);
    if (oriented) {
        SetRotMatrix(&anchor_matrix);
        copyVector(&vector, &position);
        RotTrans(&vector, &position, &direction.pad);
    }
    switch (values->anchor_flags & EFFECT_EMITTER_ORIGIN_MASK) {
    case EFFECT_EMITTER_ORIGIN_CASTER:
        position.vx += origin.vx;
        position.vy += origin.vy;
        position.vz += origin.vz;
        break;
    case EFFECT_EMITTER_ORIGIN_TARGET:
        position.vx += target.vx;
        position.vy += target.vy;
        position.vz += target.vz;
        break;
    }

    setVector(&spread, values->spread.x, values->spread.y, values->spread.z);
    if (oriented) {
        copyVector(&vector, &spread);
        RotTrans(&vector, &spread, &direction.pad);
    }
    scale = effect_select_largest_magnitude_s16(spread.vx, spread.vy, spread.vz);
    setVector(&vector, rand(), rand(), rand());
    RotMatrix(&vector, &rotation);
    SetRotMatrix(&rotation);
    vector.vx = scale;
    vector.vz = 0;
    vector.vy = 0;
    RotTrans(&vector, &direction, &direction.pad);
    motion->position.x = ((direction.vx * spread.vx) / scale + position.vx) * ONE;
    motion->position.y = ((direction.vy * spread.vy) / scale + position.vy) * ONE;
    motion->position.z = ((direction.vz * spread.vz) / scale + position.vz) * ONE;

    speed_min = values->speed_min;
    speed_max = values->speed_max;
    setVector(&angle, values->angle.x, values->angle.y, values->angle.z);
    setVector(&half_spread, values->angle_spread.x / 2, values->angle_spread.y / 2, values->angle_spread.z / 2);
    switch (values->motion_flags & EFFECT_EMITTER_VELOCITY_MASK) {
    case EFFECT_EMITTER_VELOCITY_OUTWARD:
    case EFFECT_EMITTER_VELOCITY_TOWARD_TARGET:
    case EFFECT_EMITTER_VELOCITY_CASTER_FACING:
        setVector(&vector, angle.vx + effect_get_random_between(half_spread.vx, -half_spread.vx),
            angle.vy + effect_get_random_between(half_spread.vy, -half_spread.vy),
            angle.vz + effect_get_random_between(half_spread.vz, -half_spread.vz));
        RotMatrix(&vector, &rotation);
        SetRotMatrix(&rotation);
        vector.vz = 0;
        vector.vx = 0;
        vector.vy = effect_get_random_between(speed_min, speed_max);
        RotTrans(&vector, &direction, &direction.pad);
        if (oriented) {
            copyVector(&vector, &direction);
            SetRotMatrix(&anchor_matrix);
            RotTrans(&vector, &direction, &direction.pad);
        }
        motion->velocity.x = direction.vx << 3;
        motion->velocity.y = direction.vy << 3;
        motion->velocity.z = direction.vz << 3;
        break;
    case EFFECT_EMITTER_VELOCITY_INWARD:
        setVector(&direction, position.vx - (motion->position.x >> 12), position.vy - (motion->position.y >> 12),
            position.vz - (motion->position.z >> 12));
        speed = effect_get_random_between(speed_min, speed_max);
        if (direction.vx != 0 || direction.vy != 0 || direction.vz != 0) {
            VectorNormal(&direction, &direction);
            motion->velocity.x = (speed * direction.vx) >> 9;
            motion->velocity.y = (speed * direction.vy) >> 9;
            motion->velocity.z = (speed * direction.vz) >> 9;
        } else {
            motion->velocity.y = speed << 3;
            motion->velocity.z = 0;
            motion->velocity.x = 0;
        }
        break;
    }
    motion->acceleration.x = motion->acceleration.y = motion->acceleration.z = motion->acceleration_step.x
        = motion->acceleration_step.y = motion->acceleration_step.z = 0;
    motion->inertia = ONE;
    motion->weight = effect_get_random_between(values->weight.min, values->weight.max);
    return effect_get_random_between(values->lifetime_min, values->lifetime_max);
}

/* Calls the empty main_noop_800449f8 with (1, 0x50). No code calls it. */
void battle_noop_801a866c(void) {
    main_system_report_stub(1, 0x50);
}

/* Return a signed 16-bit value in the half-open interval between two bounds. */
s16 effect_get_random_between_s16(s16 minimum, s16 maximum) {
    s16 result;

    if (minimum == maximum) {
        result = minimum;
    } else if (maximum < minimum) {
        result = maximum + rand() % (minimum - maximum);
    } else {
        result = minimum + rand() % (maximum - minimum);
    }

    return result;
}

/* Return the original component with the largest signed-16 magnitude.
 *
 * Strict comparisons make earlier arguments win ties. */
s16 effect_select_largest_magnitude_s16(s16 first, s16 second, s16 third) {
    s16 first_magnitude;
    s16 second_magnitude;
    s16 third_magnitude;
    s16 result;

    first_magnitude = first < 0 ? -first : first;
    second_magnitude = second < 0 ? -second : second;
    third_magnitude = third < 0 ? -third : third;

    if (first_magnitude < second_magnitude) {
        result = second;
        if (second_magnitude < third_magnitude) {
            result = third;
        }
    } else {
        result = first;
        if (first_magnitude < third_magnitude) {
            result = third;
        }
    }
    return result;
}

/*
 * Return the original component with the larger signed-16 magnitude.
 *
 * The strict comparison makes the first argument win ties. Negating -32768
 * wraps back to -32768 when stored as s16, matching the target's edge case.
 */
s16 effect_select_larger_magnitude_s16(s16 first, s16 second) {
    s16 first_magnitude;
    s16 second_magnitude;
    s16 result;

    first_magnitude = first < 0 ? -first : first;
    second_magnitude = second < 0 ? -second : second;

    if (first_magnitude < second_magnitude) {
        result = second;
    } else {
        result = first;
    }
    return result;
}

/* Cosine-eased interpolation from start to end at current/duration; the same
 * curve as effect_interpolate_svector_cos, on a single s32. */
s32 effect_interpolate_s32_cos(s32 start, s32 end, s32 duration, s32 current) {
    s32 difference;
    s32 scaled_difference;

    difference = end - start;
    scaled_difference = difference * (ONE - rcos((current << 11) / duration));
    return start + scaled_difference / (2 * ONE);
}

/* t ranges from 0 at from to 2 * ONE at to. */
void effect_interpolate_s16(s16 from, s16 to, s32 t, s16* out) {
    *out = from + (((to - from) * t) / (2 * ONE));
}

void effect_interpolate_svector(const SVECTOR* from, const SVECTOR* to, s32 t, SVECTOR* out) {
    effect_interpolate_s16(from->vx, to->vx, t, &out->vx);
    effect_interpolate_s16(from->vy, to->vy, t, &out->vy);
    effect_interpolate_s16(from->vz, to->vz, t, &out->vz);
}

/* Eased interpolation of three 16-bit values: the factor follows half a cosine wave
 * over duration, so the value leaves from slowly, and arrives at to slowly when
 * current reaches duration. */
void effect_interpolate_svector_cos(const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out) {
    effect_interpolate_svector(from, to, ONE - rcos((current << 11) / duration), out);
}

/* Out-and-back interpolation of three 16-bit values: the factor follows a whole
 * cosine wave over duration, so the value reaches to at half the duration and is
 * back at from when current reaches duration. */
void effect_interpolate_svector_cos_q12(
    const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out) {
    effect_interpolate_svector(from, to, ONE - rcos((current * ONE) / duration), out);
}

/* Linear counterpart of effect_interpolate_svector_cos: t = current/duration in q13. */
void effect_interpolate_svector_linear(
    const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out) {
    effect_interpolate_svector(from, to, (current << 13) / duration, out);
}

void effect_lerp_component_q13(s32 from, s32 to, s32 t, s32* out) {
    *out = from + (to - from) * t / (2 * ONE);
}

void effect_interpolate_vector_q13(const VECTOR* from, const VECTOR* to, s32 t, VECTOR* out) {
    effect_lerp_component_q13(from->vx, to->vx, t, &out->vx);
    effect_lerp_component_q13(from->vy, to->vy, t, &out->vy);
    effect_lerp_component_q13(from->vz, to->vz, t, &out->vz);
}

/* Eased interpolation of three 32-bit components: the factor follows half a cosine wave over
 * duration, so the value leaves from slowly, and arrives at to slowly when
 * current reaches duration. */
void effect_interpolate_vector_q13_cos(const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out) {
    effect_interpolate_vector_q13(from, to, ONE - rcos((current << 11) / duration), out);
}

/* Out-and-back interpolation of three 32-bit components: the factor follows a whole cosine
 * wave over duration, so the value reaches to at half the duration and is back at
 * from when current reaches duration. */
void effect_interpolate_vector_q13_cos_q12(
    const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out) {
    effect_interpolate_vector_q13(from, to, ONE - rcos((current * ONE) / duration), out);
}

/* Linear counterpart of effect_interpolate_vector_q13_cos: t = current/duration in q13. */
void effect_interpolate_vector_q13_linear(
    const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out) {
    effect_interpolate_vector_q13(from, to, (current << 13) / duration, out);
}

/* Linear interpolation with an eight-bit fractional factor (256 = 1.0).
 * The arithmetic right shift rounds negative products down. */
s32 battle_lerp_fixed8(s32 start, s32 end, s32 factor) {
    return ((end - start) * factor >> 8) + start;
}

/* Store and return the interpolation with an eight-bit fractional factor.
 * The arithmetic right shift rounds negative products down. */
s32 battle_lerp_fixed8_and_store(s32 start, s32 end, s32 factor, s32* out) {
    s32 result;

    result = ((end - start) * factor >> 8) + start;
    *out = result;
    return result;
}

void effect_interpolate_emitter_spawn_position(const effect_geometry_record_t* source, s32 factor, VECTOR* out) {
    const effect_geometry_entry_t* entry = &source->emitter;

    battle_lerp_fixed8_and_store(
        entry->spawn.spawn_position_start.x, entry->spawn.spawn_position_end.x, factor, &out->vx);
    battle_lerp_fixed8_and_store(
        entry->spawn.spawn_position_start.y, entry->spawn.spawn_position_end.y, factor, &out->vy);
    battle_lerp_fixed8_and_store(
        entry->spawn.spawn_position_start.z, entry->spawn.spawn_position_end.z, factor, &out->vz);
}

void effect_interpolate_emitter_spawn_spread(const effect_geometry_record_t* source, s32 factor, VECTOR* out) {
    const effect_geometry_entry_t* entry = &source->emitter;

    battle_lerp_fixed8_and_store(entry->spawn.spawn_spread_start.x, entry->spawn.spawn_spread_end.x, factor, &out->vx);
    battle_lerp_fixed8_and_store(entry->spawn.spawn_spread_start.y, entry->spawn.spawn_spread_end.y, factor, &out->vy);
    battle_lerp_fixed8_and_store(entry->spawn.spawn_spread_start.z, entry->spawn.spawn_spread_end.z, factor, &out->vz);
}

void effect_interpolate_emitter_velocity_base_angles(const effect_geometry_record_t* source, s32 factor, VECTOR* out) {
    const effect_geometry_entry_t* entry = &source->emitter;

    battle_lerp_fixed8_and_store(
        entry->spawn.velocity_base_angle_start.x, entry->spawn.velocity_base_angle_end.x, factor, &out->vx);
    battle_lerp_fixed8_and_store(
        entry->spawn.velocity_base_angle_start.y, entry->spawn.velocity_base_angle_end.y, factor, &out->vy);
    battle_lerp_fixed8_and_store(
        entry->spawn.velocity_base_angle_start.z, entry->spawn.velocity_base_angle_end.z, factor, &out->vz);
}

void effect_interpolate_emitter_velocity_direction_spread(
    const effect_geometry_record_t* source, s32 factor, VECTOR* out) {
    const effect_geometry_entry_t* entry = &source->emitter;

    battle_lerp_fixed8_and_store(
        entry->spawn.velocity_spread_start.x, entry->spawn.velocity_spread_end.x, factor, &out->vx);
    battle_lerp_fixed8_and_store(
        entry->spawn.velocity_spread_start.y, entry->spawn.velocity_spread_end.y, factor, &out->vy);
    battle_lerp_fixed8_and_store(
        entry->spawn.velocity_spread_start.z, entry->spawn.velocity_spread_end.z, factor, &out->vz);
}

/* Random integer between the two arguments, given in either order: at least the
 * smaller and less than the larger. Equal arguments return that value without
 * drawing a random number. */
s32 effect_get_random_between(s32 value_a, s32 value_b) {
    s32 remainder;
    s32 random_value;
    s32 result;

    if (value_a == value_b) {
        return value_a;
    }
    if (value_b < value_a) {
        random_value = rand();
        remainder = random_value % (value_a - value_b);
        result = remainder + value_b;
    } else {
        random_value = rand();
        remainder = random_value % (value_b - value_a);
        result = remainder + value_a;
    }
    return result;
}
