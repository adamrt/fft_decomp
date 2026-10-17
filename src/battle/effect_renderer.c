/* The effect renderer, including the heap, sprite blocks, primitives, particle emitters, interpolation, models and
 * vector arithmetic. */

#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/effect.h"
#include "fft/effect_file.h"
#include "fft/effect_geometry.h"
#include "fft/effect_heap.h"
#include "fft/effect_particle.h"
#include "fft/effect_projectile.h"
#include "fft/effect_script.h"
#include "fft/effect_secondary.h"
#include "fft/effect_timeline.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/system.h"
#include "fft/unit_runtime.h"
#include "psx/gte_inline_split.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Particle transforms, gravity and inertia used by the emitter integrator. */
extern MATRIX g_effect_emitter_matrix;
extern MATRIX g_effect_particle_heading_matrix;

/* Particle gravity and inertia threshold, and their saved copies: the secondary handlers save both while their slots
 * run under a gravity of (0, ONE, 0) and EFFECT_SECONDARY_INERTIA_THRESHOLD. */
extern vec3s32_t g_effect_gravity;
extern vec3s32_t g_effect_saved_gravity;
extern s32 g_effect_inertia_threshold;
extern s32 g_effect_saved_inertia_threshold;

/* Effect work pool: g_effect_work_records is the 256-record allocation, g_effect_free_work_record_head
 * the free-chain head, g_effect_work_record_count the in-use count and g_effect_work_record_peak its
 * peak. */
extern effect_work_record_t* g_effect_work_records;
extern effect_work_record_t* g_effect_free_work_record_head;
extern s32 g_effect_work_record_count;
extern s32 g_effect_work_record_peak;
extern s32 g_effect_work_records_unread; /* cleared when the effect work-record chain is built; nothing reads it */

/* Sprite-node pool: the nodes (node 0 the sentinel), the active and free list heads, and the sprite parts held in
 * the nodes' sprite blocks with their peak. */
extern effect_list_node_t g_effect_list_nodes[EFFECT_SPRITE_NODE_CAPACITY + 1];
extern u16 g_effect_active_list_node_head;
extern u16 g_effect_free_list_node_head;
extern s32 g_effect_sprite_count;
extern s32 g_effect_sprite_count_peak;

/* Primitive arena and the frame markers maintained by this renderer. */
extern s32 g_effect_frame_parity;
extern u8* g_effect_prim_buffer;
extern s32 g_effect_prim_buffer_offset;
extern s32 g_effect_frame_start_prim_buffer_offset;

/* The third 32-byte primitive kind has no named caller. */
static s32 g_effect_primitive_sizes[3] = { sizeof(POLY_FT4), sizeof(DR_MODE), 32 };

/* Private cursor state for the resident model decoder. */
extern psyq_tmd_object_t* g_effect_model_object;
extern SVECTOR* g_effect_model_vertices;
extern SVECTOR* g_effect_model_normals;
extern psyq_tmd_packet_t* g_effect_model_commands;
extern s32 g_effect_model_command_word_offset;

static s32 effect_particle_update_node_from_work(effect_record_t* unused_record, effect_work_record_t* work);
static void effect_vector_add_svector_to_vector(const SVECTOR* first, const VECTOR* second, VECTOR* out);
static effect_list_node_t* effect_particle_alloc_list_node(s16 kind);
static void effect_particle_free_list_node(effect_list_node_t* node);
static s32 effect_get_random_between(s32 value_a, s32 value_b);
static void effect_particle_init_animation_sequence(effect_list_node_t* state, s32 frame_group, s32 animation_index);
static void effect_particle_render_sprite(effect_list_node_t* particle, s32 red, s32 green, s32 blue);

/* The four sprite work kinds share one updater; heap-block work has no entry. */
s32 (*g_effect_particle_render_handlers[4])(effect_record_t* record, effect_work_record_t* work)
    = { effect_particle_update_node_from_work, effect_particle_update_node_from_work,
          effect_particle_update_node_from_work, effect_particle_update_node_from_work };

/* Make `buffer` the battle heap: a single free block of buffer_size / 8 - 1 eight-byte
 * blocks that forms the free ring by itself, with owner lists 0 to 16 emptied. */
void effect_heap_init(effect_heap_node_t* buffer, u32 buffer_size) {
    s32 owner;
    s16 block_count = (buffer_size >> 3) - 1;

    g_effect_heap_rover = buffer;
    g_effect_heap_base = buffer;
    buffer->size = block_count;
    g_effect_heap_block_count = block_count;
    buffer->next = buffer;
    for (owner = EFFECT_HEAP_OWNER_COUNT - 1; owner >= 0; owner--) {
        g_effect_heap_owner_lists[owner].head = 0;
    }
}

/* Allocate `size` bytes from the battle heap for owner list `owner` (0-16) and
 * return the address past the block header.
 *
 * First fit along the circular free list, starting after the rover as in the K&R
 * malloc: the request is rounded up to 8-byte units plus one unit for the header,
 * an exact fit is unlinked and a larger block gives up its tail. The block goes
 * to the head of the owner's list. There is no failure exit: when no free block
 * is large enough the search goes round the list without end. */
void* effect_heap_alloc_block(u32 size, s32 owner) {
    effect_heap_node_t* prev;
    effect_heap_node_t* node;
    u32 blocks = (size + 15) >> 3;

    prev = g_effect_heap_rover;
    node = prev->next;
    for (;;) {
        if (node->size >= blocks) {
            if (node->size == blocks) {
                prev->next = node->next;
            } else {
                node->size = node->size - blocks;
                node = (effect_heap_node_t*)((u8*)node + node->size * 8);
                node->size = blocks;
            }
            node->owner = owner;
            node->next = g_effect_heap_owner_lists[owner].head;
            g_effect_heap_rover = prev;
            g_effect_heap_owner_lists[owner].head = node;
            return node + 1;
        } else {
            prev = node;
            node = node->next;
        }
    }
}

/*
 * Release a block from effect_heap_alloc_block.
 *
 * The block is unlinked from its owner's list, then returned to the circular
 * free list in address order and coalesced with both neighbors, as in the
 * K&R free().
 */
void effect_heap_free_block(void* ptr) {
    effect_heap_node_t* node = (effect_heap_node_t*)ptr - 1;
    effect_heap_owner_list_t* list = &g_effect_heap_owner_lists[node->owner];
    effect_heap_node_t* owned = list->head;
    effect_heap_node_t* prev;
    effect_heap_node_t* free_node;

    if (owned == node) {
        list->head = node->next;
    } else {
        for (prev = owned; (owned = prev->next) != node; prev = owned) { }
        prev->next = node->next;
    }

    for (free_node = g_effect_heap_rover; !(node > free_node && node < free_node->next); free_node = free_node->next) {
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
    g_effect_heap_rover = free_node;
}

/*
 * Debug-print battle heap usage: total and largest free block on the free
 * ring, the low-water mark of the largest block, a "DP" value, and the bytes
 * held by owners 0 through 15. The original diagnostic omits owner 16.
 *
 * The ring-end test rereads g_effect_heap_rover; GCC hoists that load, which
 * produces the target copy of the rover into a separate register.
 */
void effect_heap_print_stats(void) {
    effect_heap_node_t* free_node;
    effect_heap_node_t* node;
    s32 total = 0;
    s32 largest = 0;
    s32 owner;
    s32 used;
    s32 size;

    free_node = g_effect_heap_rover->next;
    for (;;) {
        size = free_node->size;
        total += size;
        if (largest < size) {
            largest = size;
        }
        if (free_node == g_effect_heap_rover) {
            break;
        }
        free_node = free_node->next;
    }
    FntPrint(g_effect_heap_remain_format, total * 8, largest * 8);
    if (largest < g_effect_heap_min_largest_free) {
        g_effect_heap_min_largest_free = largest;
    }
    FntPrint(g_effect_heap_min_format, g_effect_heap_min_largest_free * 8);
    FntPrint(g_effect_heap_dp_format, g_effect_heap_debug_dp_words * 4 + g_effect_heap_debug_dp_bytes);
    for (owner = 0; owner < 16; owner++) {
        used = 0;
        for (node = g_effect_heap_owner_lists[owner].head; node != 0; node = node->next) {
            used += node->size;
        }
        if (used != 0) {
            FntPrint(g_effect_heap_task_format, owner, used * 8);
        }
    }
}

/* Initialize the effect renderer's transform, depth, and primitive state.
 *
 * The rotation terms form the identity matrix's third row and column. Every
 * corner starts at the same screen depth before an effect supplies geometry. */
static void effect_render_init_state(void) {
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
    g_effect_prim_buffer = effect_heap_alloc_block(EFFECT_PRIMITIVE_BUFFER_SIZE, 0);
    g_effect_sprite_count_peak = 0;
    g_effect_sprite_count = 0;
}

/* The subset of effect_render_init_state that does not touch the
 * prim buffer or the sprite counters. */
void effect_render_reset_state(void) {
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
s32 effect_render_get_prim_buffer_offset(void) {
    return g_effect_prim_buffer_offset;
}

/* Allocate the variable-length sprite block for one effect frame.
 *
 * The eight-byte prefix is followed by one pointer per sprite part. The renderer tracks
 * both current and peak occupancy for the shared effect pool. */
static effect_sprite_block_t* effect_render_alloc_sprite_block(u8 sprite_count) {
    effect_sprite_block_t* block;
    s32 total_count;

    block = effect_heap_alloc_block(sprite_count * sizeof(block->frames[0]) + sizeof(*block), 0);
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
static void effect_render_free_sprite_block(effect_sprite_block_t* block) {
    g_effect_sprite_count -= block->header.sprite_count;
    effect_heap_free_block(block);
}

/* Reset the projection center and store frame parity and the arena's current offset.
 * The two stored values have no identified reader; allocation wraps the arena independently. */
void effect_render_start_frame(void) {
    SetGeomOffset(0, 0);
    g_effect_frame_parity = 1 - g_effect_frame_parity;
    g_effect_frame_start_prim_buffer_offset = g_effect_prim_buffer_offset;
}

/* Allocate one typed primitive from the effect renderer's circular arena.
 *
 * The original renderer wraps before an allocation that would cross the
 * 0x8000-byte arena boundary; callers consume the result immediately. */
static void* effect_render_alloc_primitive(effect_primitive_type_e primitive_type) {
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
static void effect_render_submit_sprite_to_ordering_table(
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

    draw_mode = effect_render_alloc_primitive(EFFECT_PRIMITIVE_DR_MODE);
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
        quad = effect_render_alloc_primitive(EFFECT_PRIMITIVE_FT4);
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
            clut = value + BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_EFFECT_SECOND, 0);
        } else {
            clut = value + BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_EFFECT, 0);
        }
        quad->clut = clut;
        quad->u0 = quad->u2 = frame->u;
        quad->u1 = quad->u3 = quad->u0 + frame->width;
        quad->v0 = quad->v1 = frame->v;
        quad->v2 = quad->v3 = quad->v0 + frame->height;
        gte_stsxy_split(&quad->x3);
        addPrim(ot, quad);
    }
    draw_mode = effect_render_alloc_primitive(EFFECT_PRIMITIVE_DR_MODE);
    SetDrawMode(draw_mode, 1, 0, 1, 0);
    addPrim(ot, draw_mode);
}

/* Queues one effect slot's billboard sprite: projects the slot origin for its
 * ordering-table depth, builds the rotated, camera-scaled 2D matrix from the
 * slot angle, transforms the frame's four corners into the slot's
 * double-buffered POLY_FT4, and adds that quad between the slot's two draw-mode
 * primitives. */
static void effect_slot_draw_billboard_sprite(effect_slot_t* slot) {
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

    frame_group = g_effect_secondary_particle_frame_data.groups;
    /* The group's frame table starts frame_table_offset bytes into it, one halfword per frame. */
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
static void effect_particle_init_record_chain(void) {
    s32 index;
    effect_work_record_t* records;

    g_effect_particle_heading_matrix.t[2] = 0;
    g_effect_particle_heading_matrix.t[1] = 0;
    g_effect_particle_heading_matrix.t[0] = 0;
    g_effect_emitter_matrix.t[2] = 0;
    g_effect_emitter_matrix.t[1] = 0;
    g_effect_emitter_matrix.t[0] = 0;
    records = effect_heap_alloc_block(EFFECT_WORK_RECORD_CAPACITY * sizeof(effect_work_record_t), 0);
    g_effect_work_records = records;
    g_effect_free_work_record_head = records;
    g_effect_work_records_unread = 0;
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
 * record_index; the inverse of effect_particle_free_work_record.
 *
 * Bits 12-15 of kind select the effect list node type allocated as the
 * record's data (0x0000-0x3000 map to list kinds 0, 2, 4 and 6); other kinds
 * get no data. g_effect_work_record_peak tracks the peak number of records in use.
 * With the free chain empty it only passes code 0x3e to the empty report hook
 * and then reads the next link through the null head.
 */
static effect_work_record_t* effect_particle_alloc_work_record(s16 record_index, s16 kind) {
    effect_work_record_t* work = g_effect_free_work_record_head;
    effect_record_t* owner;

    g_effect_work_record_count++;
    owner = &g_effect_state_records[record_index];
    if (g_effect_work_record_peak < g_effect_work_record_count) {
        g_effect_work_record_peak = g_effect_work_record_count;
    }
    if (work == 0) {
        system_report_stub(1, 0x3E);
    }
    g_effect_free_work_record_head = work->next;
    switch (kind & EFFECT_WORK_KIND_MASK) {
    case EFFECT_WORK_KIND_SPRITE_WORLD:
        work->data = effect_particle_alloc_list_node(EFFECT_LIST_NODE_WORLD);
        break;
    case EFFECT_WORK_KIND_SPRITE_SCREEN_OFFSET:
        work->data = effect_particle_alloc_list_node(EFFECT_LIST_NODE_SCREEN_OFFSET);
        break;
    case EFFECT_WORK_KIND_SPRITE_WORLD_UNSCALED:
        work->data = effect_particle_alloc_list_node(EFFECT_LIST_NODE_WORLD_UNSCALED);
        break;
    case EFFECT_WORK_KIND_SPRITE_SCREEN:
        work->data = effect_particle_alloc_list_node(EFFECT_LIST_NODE_SCREEN);
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
void effect_particle_free_work_record(s16 record_index, effect_work_record_t* work) {
    effect_record_t* owner = &g_effect_state_records[record_index];

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
        effect_particle_free_list_node(work->data);
        break;
    case EFFECT_WORK_KIND_HEAP_BLOCK:
        effect_heap_free_block(work->data);
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
static s32 effect_particle_update_node_from_work(effect_record_t* unused_record, effect_work_record_t* work) {
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
        effect_particle_render_sprite(work->data,
            g_effect_animation_curve_data->curves[work->color_curves.r][work->curve_frame],
            g_effect_animation_curve_data->curves[work->color_curves.g][work->curve_frame],
            g_effect_animation_curve_data->curves[work->color_curves.b][work->curve_frame]);
    } else {
        effect_particle_render_sprite(work->data, 0x80, 0x80, 0x80);
    }
}

/* Empty. Nothing in the game calls it or effect_noop_2. */
void effect_noop_1(void) {
}

/* Empty and uncalled, like effect_noop_1 before it. */
void effect_noop_2(void) {
}

/* Meant to return the requested_index-th work record of an allocated state
 * record (0 when out of range). The loop follows work_count links from the
 * head instead of requested_index, which runs off the end of the
 * null-terminated list, so it always returns 0. Nothing in the game calls it. */
effect_work_record_t* effect_particle_get_work_record(s16 record_index, s16 requested_index) {
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
s32 effect_particle_spawn_from_emitter(s16 record_index, s32 frame, s32 emitter_index, effect_work_record_t* parent) {
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
    battle_map_tile_t* tile;

    entry = &g_effect_geometry_table->entries[emitter_index];
    time = frame;
    frame = frame % EFFECT_ANIMATION_CURVE_FRAMES;
    placement_flags = entry->emitter.placement_flags;
    behavior_flags = entry->emitter.flags;
    /* Keep the binding word packed: the first lookup loads its last byte,
     * later lookups and the homing setup load the complete word. */
    EFFECT_READ_CURVE(curve, factor,
        (entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_SPAWN_INTERVAL_SHIFT) & 0xf, frame);
    if (time % effect_lerp_q8(entry->emitter.spawn_interval_start, entry->emitter.spawn_interval_end, factor) != 0) {
        return;
    }

    SetTransMatrix(&g_effect_emitter_matrix);
    target_index = g_effect_state_records[record_index].timeline.target_index;
    rotate = (behavior_flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING;
    effect_resolve_target_world_coords(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
        EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &caster);
    if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
        facing = ((unit_facing_get_quadrant_by_runtime_id(
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
        unit_facing_get_quadrant_by_runtime_id(g_effect_context.targets[target_index].data.unit.runtime_id);
        unit_coords = unit_get_event_offset_by_runtime_id(g_effect_context.targets[target_index].data.unit.runtime_id);
    }
    tile = battle_map_tile_get_pointer(
        g_effect_context.target_tile.vx, g_effect_context.target_tile.vz, g_effect_context.target_tile.vy);

    EFFECT_READ_CURVE(curve, factor,
        (entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_PARTICLE_COUNT_SHIFT) & 0xf, frame);
    count = effect_lerp_q8(entry->emitter.particle_count_start, entry->emitter.particle_count_end, factor);
    for (i = 0; i < count; i++) {
        particle = effect_particle_alloc_work_record(record_index, (placement_flags & EFFECT_WORK_KIND_MASK));
        particle->kind_flags = placement_flags;
        particle->flags = behavior_flags;
        particle->curve_frame = 0;
        effect_particle_init_animation_sequence(
            particle->data, entry->emitter.animation_frame_group, entry->emitter.animation_index);
    }
    /* Each record is pushed at the head of the owner's list, so the last one
     * allocated leads the batch and `next` walks the rest of it. */
    head = particle;

    EFFECT_READ_CURVE(curve, factor, entry->emitter.spawn_bindings.position, frame);
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
        spawn.vx += g_effect_context.target_tile.vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        spawn.vz += g_effect_context.target_tile.vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        spawn.vy -= tile->height * BATTLE_MAP_HEIGHT_STEP;
        break;
    case EFFECT_EMITTER_ORIGIN_CASTER:
        effect_vector_add_svector_to_vector(&caster, &spawn, &spawn);
        break;
    case EFFECT_EMITTER_ORIGIN_TARGET:
        effect_vector_add_svector_to_vector(&target, &spawn, &spawn);
        break;
    case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
        if (parent != 0) {
            spawn.vx += parent->motion.position.x >> 12;
            spawn.vy += parent->motion.position.y >> 12;
            spawn.vz += parent->motion.position.z >> 12;
        }
        break;
    case EFFECT_EMITTER_ORIGIN_MAP_CENTER:
        battle_map_store_dimensions(&map_max);
        spawn.vx += map_max.vx * (BATTLE_MAP_TILE_SIZE / 2);
        spawn.vz += map_max.vz * (BATTLE_MAP_TILE_SIZE / 2);
        break;
    case EFFECT_EMITTER_ORIGIN_TRACKED_ENTITY:
        spawn.vx += target.vx + unit_coords->vx;
        spawn.vy += target.vy + unit_coords->vy;
        spawn.vz += target.vz + unit_coords->vz;
        break;
    }

    selector = entry->emitter.spawn_bindings.spread;
    EFFECT_READ_CURVE(curve, factor, selector, frame);
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
        speed_min = effect_lerp_q8(entry->emitter.radial_speed_start.min, entry->emitter.radial_speed_end.min, factor);
        speed_max = effect_lerp_q8(entry->emitter.radial_speed_start.max, entry->emitter.radial_speed_end.max, factor);
    } else {
        speed_min = entry->emitter.radial_speed_start.min;
        speed_max = entry->emitter.radial_speed_start.max;
    }
    selector = entry->emitter.spawn_bindings.velocity_spread;
    curve = selector;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
        angle_spread.vx = effect_lerp_q8(
            entry->emitter.spawn.velocity_spread_start.x, entry->emitter.spawn.velocity_spread_end.x, factor);
        angle_spread.vy = effect_lerp_q8(
            entry->emitter.spawn.velocity_spread_start.y, entry->emitter.spawn.velocity_spread_end.y, factor);
        angle_spread.vz = effect_lerp_q8(
            entry->emitter.spawn.velocity_spread_start.z, entry->emitter.spawn.velocity_spread_end.z, factor);
    } else {
        /* Without a curve the start spread is halved; the curve path uses it whole. */
        setVector(&angle_spread, entry->emitter.spawn.velocity_spread_start.x / 2,
            entry->emitter.spawn.velocity_spread_start.y / 2, entry->emitter.spawn.velocity_spread_start.z / 2);
    }
    selector = entry->emitter.spawn_bindings.velocity_angle;
    EFFECT_READ_CURVE(curve, factor, selector, frame);
    effect_interpolate_emitter_velocity_base_angles(entry, factor, &angle);

    particle = head;
    switch (behavior_flags & EFFECT_EMITTER_VELOCITY_MASK) {
    case EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED:
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

    EFFECT_READ_CURVE(
        curve, factor, (entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_ACCELERATION_SHIFT) & 0xf, frame);
    accel_min.vx
        = effect_lerp_q8(entry->emitter.acceleration_start.x.min, entry->emitter.acceleration_end.x.min, factor);
    accel_max.vx
        = effect_lerp_q8(entry->emitter.acceleration_start.x.max, entry->emitter.acceleration_end.x.max, factor);
    accel_min.vy
        = effect_lerp_q8(entry->emitter.acceleration_start.y.min, entry->emitter.acceleration_end.y.min, factor);
    accel_max.vy
        = effect_lerp_q8(entry->emitter.acceleration_start.y.max, entry->emitter.acceleration_end.y.max, factor);
    accel_min.vz
        = effect_lerp_q8(entry->emitter.acceleration_start.z.min, entry->emitter.acceleration_end.z.min, factor);
    accel_max.vz
        = effect_lerp_q8(entry->emitter.acceleration_start.z.max, entry->emitter.acceleration_end.z.max, factor);
    if (rotate) {
        SetRotMatrix(&matrix);
        RotTrans(&accel_min, &out, &out.pad);
        copyVector(&accel_min, &out);
        RotTrans(&accel_max, &out, &out.pad);
        copyVector(&accel_max, &out);
    }
    EFFECT_READ_CURVE(curve, factor,
        (entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_ACCELERATION_STEP_SHIFT) & 0xf, frame);
    accel_step_min.vx = effect_lerp_q8(
        entry->emitter.acceleration_step_start.x.min, entry->emitter.acceleration_step_end.x.min, factor);
    accel_step_max.vx = effect_lerp_q8(
        entry->emitter.acceleration_step_start.x.max, entry->emitter.acceleration_step_end.x.max, factor);
    accel_step_min.vy = effect_lerp_q8(
        entry->emitter.acceleration_step_start.y.min, entry->emitter.acceleration_step_end.y.min, factor);
    accel_step_max.vy = effect_lerp_q8(
        entry->emitter.acceleration_step_start.y.max, entry->emitter.acceleration_step_end.y.max, factor);
    accel_step_min.vz = effect_lerp_q8(
        entry->emitter.acceleration_step_start.z.min, entry->emitter.acceleration_step_end.z.min, factor);
    accel_step_max.vz = effect_lerp_q8(
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
    EFFECT_READ_CURVE(curve, factor, inertia_binding, frame);
    inertia_min = effect_lerp_q8(entry->emitter.spawn.inertia_start.min, entry->emitter.spawn.inertia_end.min, factor);
    inertia_max = effect_lerp_q8(entry->emitter.spawn.inertia_start.max, entry->emitter.spawn.inertia_end.max, factor);
    discarded_selector = entry->emitter.spawn_bindings._unknown_02;
    EFFECT_READ_CURVE(curve, factor, discarded_selector, frame);
    effect_lerp_q8(entry->emitter._unknown_4c[0], entry->emitter._unknown_50[0], factor);
    effect_lerp_q8(entry->emitter._unknown_4c[1], entry->emitter._unknown_50[1], factor);
    weight_binding = entry->emitter.spawn_bindings.weight;
    EFFECT_READ_CURVE(curve, factor, weight_binding, frame);
    weight_min = effect_lerp_q8(entry->emitter.weight_start.min, entry->emitter.weight_end.min, factor);
    weight_max = effect_lerp_q8(entry->emitter.weight_start.max, entry->emitter.weight_end.max, factor);
    EFFECT_READ_CURVE(
        curve, factor, (entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_LIFETIME_SHIFT) & 0xf, frame);
    lifetime_min = effect_lerp_q8(entry->emitter.lifetime_start.min, entry->emitter.lifetime_end.min, factor);
    lifetime_max = effect_lerp_q8(entry->emitter.lifetime_start.max, entry->emitter.lifetime_end.max, factor);
    particle = head;
    for (i = 0; i < count; i++) {
        particle->motion.inertia = RANDOM_BETWEEN(inertia_min, inertia_max);
        particle->motion.weight = RANDOM_BETWEEN(weight_min, weight_max);
        particle->lifetime = RANDOM_BETWEEN(lifetime_min, lifetime_max);
        particle = particle->next;
    }

    EFFECT_READ_CURVE(curve, factor,
        (entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_TARGET_OFFSET_SHIFT) & 0xf, frame);
    target_offset.vx = effect_lerp_q8(entry->emitter.target_offset_start.x, entry->emitter.target_offset_end.x, factor);
    target_offset.vy = effect_lerp_q8(entry->emitter.target_offset_start.y, entry->emitter.target_offset_end.y, factor);
    target_offset.vz = effect_lerp_q8(entry->emitter.target_offset_start.z, entry->emitter.target_offset_end.z, factor);
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
        battle_map_store_dimensions(&map_max);
        for (i = 0; i < count; i++) {
            particle->target.x = map_max.vx * (BATTLE_MAP_TILE_SIZE / 2) + target_offset.vx;
            particle->target.z = map_max.vz * (BATTLE_MAP_TILE_SIZE / 2) + target_offset.vz;
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
        tile_anchor.vx = g_effect_context.target_tile.vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        tile_anchor.vz = g_effect_context.target_tile.vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        tile_anchor.vy = -(tile->height * BATTLE_MAP_HEIGHT_STEP);
        for (i = 0; i < count; i++) {
            particle->target.x = tile_anchor.vx + target_offset.vx;
            particle->target.y = tile_anchor.vy + target_offset.vy;
            particle->target.z = tile_anchor.vz + target_offset.vz;
            particle = particle->next;
        }
        break;
    }

    EFFECT_READ_CURVE(curve, factor,
        (entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_HOMING_STRENGTH_SHIFT) & 3, frame);
    /* The lerp results pass through curve: that reassignment keeps CSE from
     * sign-extending the call results instead of homing_min/max. */
    curve = effect_lerp_q8(entry->emitter.homing_strength_start.min, entry->emitter.homing_strength_end.min, factor);
    homing_min = curve;
    curve = effect_lerp_q8(entry->emitter.homing_strength_start.max, entry->emitter.homing_strength_end.max, factor);
    homing_max = curve;
    /* This otherwise-unused test keeps the packed selectors live through the loop setup. */
    motion_curve_selectors = entry->emitter.motion_curve_selectors;
    homing_curve_selector = motion_curve_selectors >> EFFECT_EMITTER_CURVE_HOMING_MOTION_SHIFT;
    if (motion_curve_selectors != 0) {
        curve = (motion_curve_selectors >> EFFECT_EMITTER_CURVE_HOMING_STRENGTH_SHIFT) & 3;
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
 * Position is the emitter offset plus a random point on its spread ellipsoid, both
 * optionally rotated to the caster's facing or the caster-to-target line
 * (motion_flags VELOCITY_CASTER_FACING or VELOCITY_TOWARD_TARGET) and anchored
 * at the caster or target (anchor_flags ORIGIN_CASTER or ORIGIN_TARGET).
 * Velocity is a random speed along the angle cone, or aimed back at the
 * position for VELOCITY_INWARD. The caller stores the result as the
 * particle lifetime.
 *
 * The caster and target blocks use an effect context record, as in the
 * secondary initializers; effect_resolve_target_world_coords reads the same layout. */
s32 effect_slot_spawn_motion(effect_emitter_values_t* values, effect_motion_t* motion) {
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
    effect_context_t init;
    s32 oriented;
    s16 scale;
    s16 speed_min;
    s16 speed_max;
    s16 speed;

    init.targets[EFFECT_CASTER_ENTRY] = g_effect_secondary_current->caster;
    effect_resolve_target_world_coords(0, EFFECT_TARGET_INDEX_CASTER, &init, &origin);
    init.targets[0] = g_effect_secondary_current->target.record;
    effect_resolve_target_world_coords(0, 0, &init, &target);
    oriented = 0;
    SetTransMatrix(&g_effect_projectile_matrix);
    if ((values->motion_flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING) {
        oriented = 1;
        anchor_angle.vy
            = ((unit_facing_get_quadrant_by_runtime_id(g_effect_secondary_current->caster.data.unit.runtime_id) + 1)
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
    case EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED:
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

/* Calls the empty system_report_stub with (1, 0x50). No code calls it. */
void effect_report_0x50(void) {
    system_report_stub(1, 0x50);
}

/* Return a signed 16-bit value in the half-open interval between two bounds. */
static s16 effect_get_random_between_s16(s16 minimum, s16 maximum) {
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
    s16 first_magnitude = first < 0 ? -first : first;
    s16 second_magnitude = second < 0 ? -second : second;
    s16 third_magnitude = third < 0 ? -third : third;
    s16 result;

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
    s16 first_magnitude = first < 0 ? -first : first;
    s16 second_magnitude = second < 0 ? -second : second;
    s16 result;

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
    s32 difference = end - start;
    s32 scaled_difference = difference * (ONE - rcos((current << 11) / duration));

    return start + scaled_difference / (2 * ONE);
}

/* t ranges from 0 at from to 2 * ONE at to. */
static void effect_interpolate_s16(s16 from, s16 to, s32 t, s16* out) {
    *out = from + (((to - from) * t) / (2 * ONE));
}

/* Interpolate each component with effect_interpolate_s16: t runs from 0 at from to 2 * ONE
 * at to. */
static void effect_interpolate_svector(const SVECTOR* from, const SVECTOR* to, s32 t, SVECTOR* out) {
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
void effect_interpolate_svector_cos_out_and_back(
    const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out) {
    effect_interpolate_svector(from, to, ONE - rcos((current * ONE) / duration), out);
}

/* Linear counterpart of effect_interpolate_svector_cos: t = current/duration in q13. */
void effect_interpolate_svector_linear(
    const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out) {
    effect_interpolate_svector(from, to, (current << 13) / duration, out);
}

/* The 32-bit counterpart of effect_interpolate_s16: t runs from 0 at from to 2 * ONE at to. */
static void effect_lerp_component_q13(s32 from, s32 to, s32 t, s32* out) {
    *out = from + (to - from) * t / (2 * ONE);
}

/* Interpolate each VECTOR component with effect_lerp_component_q13. */
static void effect_interpolate_vector_q13(const VECTOR* from, const VECTOR* to, s32 t, VECTOR* out) {
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
void effect_interpolate_vector_q13_cos_out_and_back(
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
s32 effect_lerp_q8(s32 start, s32 end, s32 factor) {
    return ((end - start) * factor >> 8) + start;
}

/* Store and return the interpolation with an eight-bit fractional factor.
 * The arithmetic right shift rounds negative products down. */
static s32 effect_lerp_q8_and_store(s32 start, s32 end, s32 factor, s32* out) {
    s32 result = ((end - start) * factor >> 8) + start;

    *out = result;
    return result;
}

/* Lerp the emitter's spawn position from its start to its end triple by an animation-curve factor
 * (256 = end) into out. */
void effect_interpolate_emitter_spawn_position(const effect_geometry_record_t* source, s32 factor, VECTOR* out) {
    const effect_geometry_entry_t* entry = &source->emitter;

    effect_lerp_q8_and_store(entry->spawn.spawn_position_start.x, entry->spawn.spawn_position_end.x, factor, &out->vx);
    effect_lerp_q8_and_store(entry->spawn.spawn_position_start.y, entry->spawn.spawn_position_end.y, factor, &out->vy);
    effect_lerp_q8_and_store(entry->spawn.spawn_position_start.z, entry->spawn.spawn_position_end.z, factor, &out->vz);
}

/* Lerp the emitter's spawn spread (the random offset range around the spawn position) from its
 * start to its end triple by an animation-curve factor (256 = end) into out. */
void effect_interpolate_emitter_spawn_spread(const effect_geometry_record_t* source, s32 factor, VECTOR* out) {
    const effect_geometry_entry_t* entry = &source->emitter;

    effect_lerp_q8_and_store(entry->spawn.spawn_spread_start.x, entry->spawn.spawn_spread_end.x, factor, &out->vx);
    effect_lerp_q8_and_store(entry->spawn.spawn_spread_start.y, entry->spawn.spawn_spread_end.y, factor, &out->vy);
    effect_lerp_q8_and_store(entry->spawn.spawn_spread_start.z, entry->spawn.spawn_spread_end.z, factor, &out->vz);
}

/* Lerp the emitter's velocity base angles from their start to their end triple by an animation-
 * curve factor (256 = end) into out. */
void effect_interpolate_emitter_velocity_base_angles(const effect_geometry_record_t* source, s32 factor, VECTOR* out) {
    const effect_geometry_entry_t* entry = &source->emitter;

    effect_lerp_q8_and_store(
        entry->spawn.velocity_base_angle_start.x, entry->spawn.velocity_base_angle_end.x, factor, &out->vx);
    effect_lerp_q8_and_store(
        entry->spawn.velocity_base_angle_start.y, entry->spawn.velocity_base_angle_end.y, factor, &out->vy);
    effect_lerp_q8_and_store(
        entry->spawn.velocity_base_angle_start.z, entry->spawn.velocity_base_angle_end.z, factor, &out->vz);
}

/* Lerp the emitter's velocity direction spread (the random cone around the base angles) from its
 * start to its end triple by an animation-curve factor (256 = end) into out. */
void effect_interpolate_emitter_velocity_direction_spread(
    const effect_geometry_record_t* source, s32 factor, VECTOR* out) {
    const effect_geometry_entry_t* entry = &source->emitter;

    effect_lerp_q8_and_store(
        entry->spawn.velocity_spread_start.x, entry->spawn.velocity_spread_end.x, factor, &out->vx);
    effect_lerp_q8_and_store(
        entry->spawn.velocity_spread_start.y, entry->spawn.velocity_spread_end.y, factor, &out->vy);
    effect_lerp_q8_and_store(
        entry->spawn.velocity_spread_start.z, entry->spawn.velocity_spread_end.z, factor, &out->vz);
}

/* Random integer between the two arguments, given in either order: at least the
 * smaller and less than the larger. Equal arguments return that value without
 * drawing a random number. */
static s32 effect_get_random_between(s32 value_a, s32 value_b) {
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

/* Empty, and nothing in the game calls it. */
void effect_noop_3(void) {
}

/* Select the single object's relative sections and rewind its primitive stream. */
s32 effect_model_init_sections(psyq_tmd_header_t* resource, s32 unused_1) {
    g_effect_model_object = resource->objects;
    /* TMD offsets are byte counts from the object table; word indexing keeps signed rounding. */
    g_effect_model_vertices = (SVECTOR*)&((u32*)resource->objects)[(s32)resource->objects[0].vertex_offset / 4];
    g_effect_model_normals = (SVECTOR*)&((u32*)resource->objects)[(s32)resource->objects[0].normal_offset / 4];
    g_effect_model_commands
        = (psyq_tmd_packet_t*)&((u32*)resource->objects)[(s32)resource->objects[0].primitive_offset / 4];
    g_effect_model_command_word_offset = 0;
    return resource->objects[0].primitive_count;
}

/* Decode the next effect command and advance the command index.
 *
 * Opcodes 0x31 and 0x39 carry three or four colors plus vertex indices; 0x35 and
 * 0x37 add five texture operands. Any other opcode is skipped by its length byte.
 *
 * The 0x35/0x37 stream uses variable-length commands, so its index is a 32-bit-element
 * offset rather than a psyq_tmd_packet_t array index. */
void effect_model_decode_next_command(effect_decoded_model_command_t* out) {
    psyq_tmd_packet_t* command;
    s32 index;
    effect_decoded_model_command_t* dst = out;
    u32 header;

    index = g_effect_model_command_word_offset;
    command = (psyq_tmd_packet_t*)((u8*)g_effect_model_commands + index * sizeof(u32));
    header = command->id;
    memcpy(&out->header, &command->id, sizeof(out->header));
    /* Struct assignments use unaligned copies; memcpy preserves the 32-bit transfers.
     * The cached header preserves dispatch load/store order and the unsigned range test. */
    switch (header >> 24) {
    case EFFECT_MODEL_COMMAND_G4:
        memcpy(&out->colors[0], &command->g4.r0, sizeof(out->colors[0]));
        memcpy(&out->colors[1], &command->g4.r1, sizeof(out->colors[1]));
        memcpy(&out->colors[2], &command->g4.r2, sizeof(out->colors[2]));
        memcpy(&out->colors[3], &command->g4.r3, sizeof(out->colors[3]));
        memcpy(&out->vertices.pairs[0], &command->g4.v0, sizeof(out->vertices.pairs[0]));
        /* A 32-bit load preserves the native delay-slot store; memcpy reschedules it. */
        out->vertices.pairs[1] = *(u32*)&command->g4.v2;
        g_effect_model_command_word_offset = index + 7;
        break;
    case EFFECT_MODEL_COMMAND_G3:
        memcpy(&out->colors[0], &command->g3.r0, sizeof(out->colors[0]));
        memcpy(&out->colors[1], &command->g3.r1, sizeof(out->colors[1]));
        memcpy(&out->colors[2], &command->g3.r2, sizeof(out->colors[2]));
        memcpy(&out->vertices.pairs[0], &command->g3.v0, sizeof(out->vertices.pairs[0]));
        /* A 32-bit load preserves the native delay-slot store; memcpy reschedules it. */
        out->vertices.pairs[1] = *(u32*)&command->g3.v2;
        g_effect_model_command_word_offset = index + 6;
        break;
    case EFFECT_MODEL_COMMAND_GT3_OPAQUE:
    case EFFECT_MODEL_COMMAND_GT3_SEMITRANSPARENT: {
        psyq_tmd_packet_t* extended;
        extended
            = (psyq_tmd_packet_t*)((u8*)g_effect_model_commands + g_effect_model_command_word_offset * sizeof(u32));
        memcpy(&dst->colors[0], &extended->gt3.r0, sizeof(dst->colors[0]));
        memcpy(&dst->colors[1], &extended->gt3.r1, sizeof(dst->colors[1]));
        memcpy(&dst->colors[2], &extended->gt3.r2, sizeof(dst->colors[2]));
        memcpy(&dst->vertices.pairs[0], &extended->gt3.v0, sizeof(dst->vertices.pairs[0]));
        memcpy(&dst->vertices.pairs[1], &extended->gt3.v2, sizeof(dst->vertices.pairs[1]));
        out->texture.tpage = extended->gt3.tpage;
        out->texture.clut = extended->gt3.clut;
        memcpy(&out->texture.uv[0], &extended->gt3.tu0, sizeof(out->texture.uv[0]));
        memcpy(&out->texture.uv[1], &extended->gt3.tu1, sizeof(out->texture.uv[1]));
        memcpy(&out->texture.uv[2], &extended->gt3.tu2, sizeof(out->texture.uv[2]));
        g_effect_model_command_word_offset += 9;
        break;
    }
    default:
        g_effect_model_command_word_offset += out->header.word_count + 1;
        break;
    }
}

/* Resolve one effect target to integer world coordinates.
 *
 * A unit target copies the unit's world-coordinate record, and when
 * add_depth is set a unit below its tile's surface (depth included) is lifted
 * onto it (y grows downward). A tile target uses the tile center
 * (x * 28 + 14, y * 28 + 14) and takes its height from the tile record,
 * adding the depth when add_depth is set. */
void effect_resolve_target_world_coords(s32 add_depth, s32 target_index, effect_context_t* coord_data, SVECTOR* out) {
    battle_map_tile_t* tile;

    if (target_index == EFFECT_TARGET_INDEX_CASTER) {
        if (coord_data->targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            *out = *unit_get_world_coords_by_runtime_id(coord_data->targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id);
            if (add_depth != 0) {
                s32 height;
                tile = battle_map_get_unit_tile_by_runtime_id(
                    coord_data->targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id);
                height = -((tile->height + tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP);
                if (height < out->vy) {
                    out->vy = height;
                }
            }
        } else {
            tile = battle_map_tile_get_pointer(coord_data->targets[EFFECT_CASTER_ENTRY].data.tile.vx,
                coord_data->targets[EFFECT_CASTER_ENTRY].data.tile.vz,
                coord_data->targets[EFFECT_CASTER_ENTRY].data.tile.vy);
            out->vx = coord_data->targets[EFFECT_CASTER_ENTRY].data.tile.vx * BATTLE_MAP_TILE_SIZE
                + (BATTLE_MAP_TILE_SIZE / 2);
            out->vz = coord_data->targets[EFFECT_CASTER_ENTRY].data.tile.vz * BATTLE_MAP_TILE_SIZE
                + (BATTLE_MAP_TILE_SIZE / 2);
            if (add_depth != 0) {
                out->vy = -((tile->height + tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP);
            } else {
                out->vy = -(tile->height * BATTLE_MAP_HEIGHT_STEP);
            }
        }
    } else {
        if (coord_data->targets[target_index].target_type == EFFECT_TARGET_TYPE_UNIT) {
            *out = *unit_get_world_coords_by_runtime_id(coord_data->targets[target_index].data.unit.runtime_id);
            if (add_depth != 0) {
                s32 height;
                tile = battle_map_get_unit_tile_by_runtime_id(coord_data->targets[target_index].data.unit.runtime_id);
                height = -((tile->height + tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP);
                if (height < out->vy) {
                    out->vy = height;
                }
            }
        } else {
            tile = battle_map_tile_get_pointer(coord_data->targets[target_index].data.tile.vx,
                coord_data->targets[target_index].data.tile.vz, coord_data->targets[target_index].data.tile.vy);
            out->vx
                = coord_data->targets[target_index].data.tile.vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
            out->vz
                = coord_data->targets[target_index].data.tile.vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
            if (add_depth != 0) {
                out->vy = -((tile->height + tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP);
            } else {
                out->vy = -(tile->height * BATTLE_MAP_HEIGHT_STEP);
            }
        }
    }
}

/* Meant to resolve a battle id's world coordinates, but it passes the unit's
 * runtime id where effect_resolve_target_world_coords expects an index
 * into the effect's target list. Nothing in the game calls it. */
void effect_get_world_coords_by_battle_id(
    s32 adjust_for_tile_height, u32 battle_id, effect_context_t* effect_coordinates, SVECTOR* destination) {
    effect_resolve_target_world_coords(
        adjust_for_tile_height, unit_get_runtime_id_by_battle_id(battle_id), effect_coordinates, destination);
}

/* Widen a packed 16-bit x, y, z triple (as the effect timeline stores them) into a
 * VECTOR. */
void effect_vector_load(const vec3s16_t* src, VECTOR* dst) {
    dst->vx = src->x;
    dst->vy = src->y;
    dst->vz = src->z;
}

/* Copy a packed 16-bit x, y, z triple into an SVECTOR. */
void effect_vector_load_svector(const vec3s16_t* src, SVECTOR* dst) {
    dst->vx = src->x;
    dst->vy = src->y;
    dst->vz = src->z;
}

/* Convert 20.12 fixed-point components to whole units, rounding down; the inverse of
 * effect_vector_scale_q12. */
void effect_vector_shift_right_12(const VECTOR* src, VECTOR* dst) {
    dst->vx = src->vx >> 12;
    dst->vy = src->vy >> 12;
    dst->vz = src->vz >> 12;
}

/* Convert integral vector components to 20.12 fixed point. */
void effect_vector_scale_q12(const VECTOR* src, VECTOR* dst) {
    dst->vx = src->vx * ONE;
    dst->vy = src->vy * ONE;
    dst->vz = src->vz * ONE;
}

/* Wrap each angle to one turn (0-0xfff). */
void effect_vector_mask_low_12_bits(const SVECTOR* src, SVECTOR* dst) {
    dst->vx = src->vx & 0xFFF;
    dst->vy = src->vy & 0xFFF;
    dst->vz = src->vz & 0xFFF;
}

/* out = first + second, component by component. */
void effect_vector_add(const VECTOR* first, const VECTOR* second, VECTOR* out) {
    out->vx = first->vx + second->vx;
    out->vy = first->vy + second->vy;
    out->vz = first->vz + second->vz;
}

/* out = first + second for SVECTORs, component by component. */
void effect_vector_add_svectors(const SVECTOR* first, const SVECTOR* second, SVECTOR* out) {
    out->vx = first->vx + second->vx;
    out->vy = first->vy + second->vy;
    out->vz = first->vz + second->vz;
}

/* out = first + second, widening the SVECTOR's components. */
static void effect_vector_add_svector_to_vector(const SVECTOR* first, const VECTOR* second, VECTOR* out) {
    out->vx = first->vx + second->vx;
    out->vy = first->vy + second->vy;
    out->vz = first->vz + second->vz;
}

/* Add to each component a random offset in [-range, range), range being the low 16 bits of the
 * matching component of ranges. The effect camera shake modes jitter the zoom with it. */
void effect_vector_add_random_offsets(const VECTOR* source, const VECTOR* ranges, VECTOR* destination) {
    u16 range = ranges->vx;

    destination->vx = source->vx + effect_get_random_between_s16(-range, range);

    range = ranges->vy;
    destination->vy = source->vy + effect_get_random_between_s16(-range, range);

    range = ranges->vz;
    destination->vz = source->vz + effect_get_random_between_s16(-range, range);
}

/* As effect_vector_add_random_offsets, then convert each result to 20.12 fixed point. */
void effect_vector_add_random_offsets_and_store_q12(const VECTOR* source, const VECTOR* ranges, VECTOR* destination) {
    u16 range;

    range = ranges->vx;
    destination->vx = (source->vx + effect_get_random_between_s16(-range, range)) * ONE;

    range = ranges->vy;
    destination->vy = (source->vy + effect_get_random_between_s16(-range, range)) * ONE;

    range = ranges->vz;
    destination->vz = (source->vz + effect_get_random_between_s16(-range, range)) * ONE;
}

/* Add delta into value and store the sum in 20.12 fixed point in out. */
void effect_vector_add_and_store_q12(const VECTOR* delta, VECTOR* value, VECTOR* out) {
    value->vx += delta->vx;
    out->vx = value->vx * ONE;
    value->vy += delta->vy;
    out->vy = value->vy * ONE;
    value->vz += delta->vz;
    out->vz = value->vz * ONE;
}

/* Add independent random offsets to three packed rotation components. */
void effect_vector_add_random_rotation_offsets(const SVECTOR* source, const SVECTOR* ranges, SVECTOR* destination) {
    u16 range = ranges->vx;

    destination->vx = source->vx + effect_get_random_between_s16(-range, range);

    range = ranges->vy;
    destination->vy = source->vy + effect_get_random_between_s16(-range, range);

    range = ranges->vz;
    destination->vz = source->vz + effect_get_random_between_s16(-range, range);
}

enum { CAMERA_ROTATION_Q12_MASK = 0x0FFF };

/* Add two effect rotations and wrap each axis to one Q12 revolution. */
void effect_vector_add_rotations_mod_q12(const SVECTOR* first, const SVECTOR* second, SVECTOR* out) {
    out->vx = (first->vx + second->vx) & CAMERA_ROTATION_Q12_MASK;
    out->vy = (first->vy + second->vy) & CAMERA_ROTATION_Q12_MASK;
    out->vz = (first->vz + second->vz) & CAMERA_ROTATION_Q12_MASK;
}

/* Nothing in the game calls this or its three siblings for 4, 6 and 8. */
void effect_add_2_to_halfword(u16* value) {
    *value += 2;
}

/* Nothing in the game calls it (see effect_add_2_to_halfword). */
void effect_add_4_to_halfword(u16* value) {
    *value += 4;
}

/* Nothing in the game calls it (see effect_add_2_to_halfword). */
void effect_add_6_to_halfword(u16* value) {
    *value += 6;
}

/* Nothing in the game calls it (see effect_add_2_to_halfword). */
void effect_add_8_to_halfword(u16* value) {
    *value += 8;
}

/* result = left + right, truncated to 16 bits per component; the unrotated form of the
 * rot_y_90/180/270 variants below. */
void effect_vector_add_to_svector(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = left->vx + right->vx;
    result->vy = left->vy + right->vy;
    result->vz = left->vz + right->vz;
}

/* result = left + right rotated 270 degrees about Y (RotMatrixY convention:
 * x' = x cos + z sin, z' = -x sin + z cos), i.e. (x - rz, y + ry, z + rx).
 * Only the low 16 bits of each input component are read. */
void effect_vector_add_to_svector_rot_y_270(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = left->vx - right->vz;
    result->vy = left->vy + right->vy;
    result->vz = left->vz + right->vx;
}

/* result = left + right rotated 180 degrees about Y, i.e. (x - rx, y + ry, z - rz).
 * Only the low 16 bits of each input component are read. */
void effect_vector_add_to_svector_rot_y_180(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = left->vx - right->vx;
    result->vy = left->vy + right->vy;
    result->vz = left->vz - right->vz;
}

/* result = left + right rotated 90 degrees about Y (RotMatrixY convention:
 * x' = x cos + z sin, z' = -x sin + z cos), i.e. (x + rz, y + ry, z - rx).
 * Only the low 16 bits of each input component are read. */
void effect_vector_add_to_svector_rot_y_90(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = left->vx + right->vz;
    result->vy = left->vy + right->vy;
    result->vz = left->vz - right->vx;
}

/* Set the gravity vector that every motion step adds to a particle's velocity, scaled by the
 * particle's weight. The effect script loads it from the particle-system header. */
void effect_set_gravity(const vec3s32_t* src) {
    g_effect_gravity = *src;
}

/* Set the damping term of particle motion: each step keeps (inertia - threshold) / inertia of a
 * particle's velocity. */
void effect_set_inertia_threshold(s32 threshold) {
    g_effect_inertia_threshold = threshold;
}

/* Save the gravity for effect_restore_gravity: secondary-effect handlers set their own
 * gravity around their slot updates. */
void effect_save_gravity(void) {
    g_effect_saved_gravity = g_effect_gravity;
}

/* Restore the gravity kept by effect_save_gravity. */
void effect_restore_gravity(void) {
    g_effect_gravity = g_effect_saved_gravity;
}

/* Save the damping term for effect_restore_inertia_threshold; secondary-effect handlers set
 * their own around their slot updates. */
void effect_save_inertia_threshold(void) {
    g_effect_saved_inertia_threshold = g_effect_inertia_threshold;
}

/* Restore the damping term kept by effect_save_inertia_threshold. */
void effect_restore_inertia_threshold(void) {
    g_effect_inertia_threshold = g_effect_saved_inertia_threshold;
}

/* Advance an effect slot's motion state by one frame: move by the velocity,
 * then damp the velocity by the inertia threshold, add the acceleration and
 * the weighted gravity, and step the acceleration. */
static void effect_slot_step_motion(effect_motion_t* motion) {
    s32 unused[4]; /* The target reserves an unused 16-byte frame. */

    motion->position.x += motion->velocity.x;
    motion->position.y += motion->velocity.y;
    motion->position.z += motion->velocity.z;
    motion->velocity.x
        = ((motion->inertia - g_effect_inertia_threshold) * motion->velocity.x + (motion->acceleration.x * ONE))
            / motion->inertia
        + ((motion->weight * g_effect_gravity.x) >> 12);
    motion->velocity.y
        = ((motion->inertia - g_effect_inertia_threshold) * motion->velocity.y + (motion->acceleration.y * ONE))
            / motion->inertia
        + ((motion->weight * g_effect_gravity.y) >> 12);
    motion->velocity.z
        = ((motion->inertia - g_effect_inertia_threshold) * motion->velocity.z + (motion->acceleration.z * ONE))
            / motion->inertia
        + ((motion->weight * g_effect_gravity.z) >> 12);
    motion->acceleration.x += motion->acceleration_step.x;
    motion->acceleration.y += motion->acceleration_step.y;
    motion->acceleration.z += motion->acceleration_step.z;
}

/* Integrate one frame of motion for every particle in a list.
 *
 * Applies motion integration to each node. A node with no
 * homing strength then adds its acceleration step to its acceleration. One
 * with a strength adds delta + (step - delta) * s / 127 instead, where delta
 * is the unit vector to its homing target scaled by the strength and s is
 * its homing curve's factor less 0x80: a factor of 0xff adds the step alone,
 * 0x80 the target vector alone, and 0, or no curve, uses -128/127 for the
 * blend weight (approximately twice the target vector less the step). The list parameter doubles as the next-node
 * cursor: that keeps both loop tests on it. */
void effect_particle_integrate_motion(effect_work_record_t* list) {
    effect_work_record_t* node;
    effect_motion_t* motion;
    s32 weight;
    s32 strength;
    s32 track;
    VECTOR delta;
    s32 threshold = g_effect_inertia_threshold;
    s32 gravity_x = g_effect_gravity.x;
    s32 gravity_y = g_effect_gravity.y;
    s32 gravity_z = g_effect_gravity.z;

    node = list;
    while (list != 0) {
        list = node->next;
        motion = &node->motion;

        motion->position.x += motion->velocity.x;
        motion->position.y += motion->velocity.y;
        motion->position.z += motion->velocity.z;

        /* `weight` holds the damping numerator here and the homing strength below. */
        weight = motion->inertia - threshold;

        motion->velocity.x = (weight * motion->velocity.x + (motion->acceleration.x * ONE)) / motion->inertia
            + ((gravity_x * motion->weight) >> 12);
        motion->velocity.y = (weight * motion->velocity.y + (motion->acceleration.y * ONE)) / motion->inertia
            + ((gravity_y * motion->weight) >> 12);
        motion->velocity.z = (weight * motion->velocity.z + (motion->acceleration.z * ONE)) / motion->inertia
            + ((gravity_z * motion->weight) >> 12);

        weight = node->homing_strength;
        if (weight != 0) {
            track = node->homing_curve_selector;
            track--;
            if (track != -1) {
                strength = g_effect_animation_curve_data->curves[track][node->curve_frame] - 0x80;
            } else {
                strength = -0x80;
            }

            setVector(&delta, node->target.x - (motion->position.x >> 12), node->target.y - (motion->position.y >> 12),
                node->target.z - (motion->position.z >> 12));
            if (delta.vx != 0 || delta.vy != 0 || delta.vz != 0) {
                VectorNormal(&delta, &delta);
                setVector(&delta, (weight * delta.vx) >> 12, (weight * delta.vy) >> 12, (weight * delta.vz) >> 12);
            }

            motion->acceleration.x += ((motion->acceleration_step.x - delta.vx) * strength) / 127 + delta.vx;
            motion->acceleration.y += ((motion->acceleration_step.y - delta.vy) * strength) / 127 + delta.vy;
            motion->acceleration.z += ((motion->acceleration_step.z - delta.vz) * strength) / 127 + delta.vz;
        } else {
            motion->acceleration.x += motion->acceleration_step.x;
            motion->acceleration.y += motion->acceleration_step.y;
            motion->acceleration.z += motion->acceleration_step.z;
        }
        node = list;
    }
}

/*
 * Rebuilds the effect-node free list: node 0 is the list sentinel, nodes
 * 1..256 are chained through next with 256 terminating, and g_effect_free_list_node_head holds
 * the free-list head.  The GetClut result is computed and discarded in the
 * target.
 */
static void effect_particle_init_free_list(void) {
    s32 i;

    GetClut(0, 0x1f0);

    g_effect_list_nodes[0].next = 0;
    g_effect_list_nodes[0].prev = 0;
    g_effect_free_list_node_head = 1;
    for (i = 1; i < EFFECT_SPRITE_NODE_CAPACITY; i++) {
        g_effect_list_nodes[i].index = i;
        g_effect_list_nodes[i].prev = i - 1;
        g_effect_list_nodes[i].next = i + 1;
    }
    g_effect_list_nodes[i].prev = i - 1;
    g_effect_list_nodes[i].next = 0;
    g_effect_list_nodes[i].index = i;
    g_effect_active_list_node_head = 0;
}

/* Take a sprite node from the free list, push it on the head of the active list
 * and give it a one-sprite block; kind is an EFFECT_LIST_NODE_* value and the
 * animation fields start at 0.
 *
 * The free list is not tested for exhaustion: with none free the sentinel node 0
 * would be handed out. There are as many nodes as work records, and
 * effect_particle_alloc_work_record is the only caller. */
static effect_list_node_t* effect_particle_alloc_list_node(s16 kind) {
    u16* free_head = &g_effect_free_list_node_head;
    u16* list_head = &g_effect_active_list_node_head;
    u16 index = *free_head;
    effect_list_node_t* node = &g_effect_list_nodes[index];

    *free_head = node->next;
    g_effect_list_nodes[node->next].prev = 0;
    g_effect_list_nodes[*list_head].prev = index;
    node->next = *list_head;
    node->prev = 0;
    *list_head = index;
    node->sprite_block = effect_render_alloc_sprite_block(1);
    node->screen_rotation_angle = 0;
    node->frame_timer = 0;
    node->sequence_data = 0;
    node->sequence_offset = 0;
    node->frame_group_offset = 0;
    node->sprite_offset_y = 0;
    node->sprite_offset_x = 0;
    node->kind = kind;
    node->sprite_frame_index = 0;
    return node;
}

/* Release a sprite work record's list node, the reverse of effect_particle_alloc_list_node:
 * free its sprite block, unlink it from the active list and push it on the free list. */
static void effect_particle_free_list_node(effect_list_node_t* node) {
    u16* free_head;

    if (node->sprite_block != 0) {
        effect_render_free_sprite_block(node->sprite_block);
        node->sprite_block = 0;
    }
    if (node->prev != 0) {
        g_effect_list_nodes[node->prev].next = node->next;
    } else {
        g_effect_active_list_node_head = node->next;
    }
    g_effect_list_nodes[node->next].prev = node->prev;
    free_head = &g_effect_free_list_node_head;
    node->next = *free_head;
    node->prev = 0;
    *free_head = node->index;
}

/* Point a sprite animation node at parameter set animation_index and restart
 * it: clear the sequence position and hold time, store frame_group doubled and
 * set kind bit 0 to mark the frame dirty. */
static void effect_particle_init_animation_sequence(effect_list_node_t* state, s32 frame_group, s32 animation_index) {
    u8* sequences = g_effect_animation_sequences;
    u8* entry;
    s16 offset;
    u16 flags;

    /* Integer addresses preserve the offset/base operand order at both additions. */
    entry = (u8*)((s16)animation_index * 2 + (s32)sequences);
    flags = state->kind | EFFECT_LIST_NODE_FRAME_CHANGED;
    offset = entry[0] + (entry[1] << 8);
    state->sequence_offset = 0;
    state->frame_group_offset = (frame_group & 0xFF) * 2;
    state->frame_timer = 0;
    state->kind = flags;
    state->sequence_data = (u8*)(offset + (s32)sequences);
}

/* Advances a list node's frame script, rebuilds its sprite-part table when the
 * frame changes, and queues the sprite at the node position.
 *
 * Script bytes: a value below 0x80 is a frame id followed by a hold count and a
 * depth mode; 0x81 restarts the script, 0x82 sets the 16-bit x/y offset and
 * 0x83 adds a signed 8-bit x/y delta. kind bit 0 marks the frame dirty; bits
 * 1-2 select camera projection or a screen-space position, with or without
 * the camera zoom. The oversized frame is reproduced with unused locals. */
static void effect_particle_render_sprite(effect_list_node_t* particle, s32 red, s32 green, s32 blue) {
    effect_list_node_t* node;
    SVECTOR input;
    SVECTOR unused_20;
    VECTOR output;
    s32 unused_38[6];
    DVECTOR position;
    effect_sprite_block_t* block;
    effect_sprite_frameset_t* frame;
    s16 frame_table_offset;
    s32 op;
    s16 count;
    s16 i;
    s32 depth;

    node = particle;
    block = node->sprite_block;

    node->frame_timer -= 2;
    if (node->frame_timer <= 0) {
        for (;;) {
            op = node->sequence_data[node->sequence_offset];
            if (!(op & EFFECT_SEQUENCE_OP_FLAG)) {
                if (node->sprite_frame_index != op) {
                    node->sprite_frame_index = op;
                    node->kind |= EFFECT_LIST_NODE_FRAME_CHANGED;
                }
                node->frame_timer = effect_get_current_sequence_instruction(node)->frame.hold;
                node->depth_mode = effect_get_current_sequence_instruction(node)->frame.depth_mode;
                /* A hold of 0 keeps the script on this frame for good. */
                if (node->frame_timer != 0) {
                    node->sequence_offset += 3;
                }
                break;
            }
            switch (op & EFFECT_SEQUENCE_OP_MASK) {
            case EFFECT_SEQUENCE_OP_LOOP:
                node->sequence_offset = 0;
                break;
            case EFFECT_SEQUENCE_OP_SET_OFFSET:
                node->sprite_offset_x = effect_get_current_sequence_instruction(node)->set_offset.x_le[0]
                    + (effect_get_current_sequence_instruction(node)->set_offset.x_le[1] << 8);
                node->sprite_offset_y = effect_get_current_sequence_instruction(node)->set_offset.y_le[0]
                    + (effect_get_current_sequence_instruction(node)->set_offset.y_le[1] << 8);
                node->sequence_offset += 5;
                break;
            case EFFECT_SEQUENCE_OP_ADD_OFFSET:
                node->sprite_offset_x += effect_get_current_sequence_instruction(node)->add_offset.x;
                node->sprite_offset_y += effect_get_current_sequence_instruction(node)->add_offset.y;
                node->sequence_offset += 3;
                break;
            }
        }
    }

    /* A new frame: point the sprite block at the frame's parts, reallocating it when
     * the part count differs. */
    if (node->kind & EFFECT_LIST_NODE_FRAME_CHANGED) {
        frame_table_offset
            = ((effect_sprite_group_t*)(g_effect_frame_data + node->frame_group_offset))->frame_table_offset;
        frame = (effect_sprite_frameset_t*)(*(u16*)(g_effect_frame_data
                                                + (node->sprite_frame_index * 2 + frame_table_offset))
            + g_effect_frame_data);
        count = frame->count;
        if (block->header.sprite_count != count) {
            effect_render_free_sprite_block(block);
            block = effect_render_alloc_sprite_block(count);
            node->sprite_block = block;
        }
        for (i = 0; i < count; i++) {
            block->frames[i] = &frame->parts[i];
        }
        node->kind &= ~EFFECT_LIST_NODE_FRAME_CHANGED;
    }

    block->header.color.r = red;
    block->header.color.g = green;
    block->header.color.b = blue;

    /* Place the sprite: projected through the camera or at a screen position,
     * with or without the camera zoom. */
    switch (node->kind & EFFECT_LIST_NODE_PLACEMENT_MASK) {
    case EFFECT_LIST_NODE_WORLD:
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        setVector(&input, node->x + node->sprite_offset_x, node->y + node->sprite_offset_y, node->z);
        RotTrans(&input, &output, &output.pad);
        depth = output.vz >> 2;
        switch (node->depth_mode) {
        case EFFECT_SPRITE_DEPTH_NEARER_8:
            depth -= 8;
            break;
        case EFFECT_SPRITE_DEPTH_FIXED_8:
            depth = 8;
            break;
        case EFFECT_SPRITE_DEPTH_FIXED_FARTHEST:
            depth = EFFECT_SPRITE_OT_DEPTH_MAX;
            break;
        case EFFECT_SPRITE_DEPTH_FIXED_16:
            depth = 16;
            break;
        case EFFECT_SPRITE_DEPTH_NEARER_16:
            depth -= 16;
            break;
        }
        /* A projected sprite pushed in front of the ordering table is culled; the screen
         * placements clamp to 0 instead. */
        if (depth < 0) {
            return;
        }
        if (depth > EFFECT_SPRITE_OT_DEPTH_MAX) {
            depth = EFFECT_SPRITE_OT_DEPTH_MAX;
        }
        position.vx = output.vx;
        position.vy = output.vy;
        effect_render_submit_sprite_to_ordering_table(
            node->sprite_block, &position, node->screen_rotation_angle, &g_camera_zoom, g_effect_otag + depth);
        break;
    case EFFECT_LIST_NODE_SCREEN_OFFSET:
        position.vx = node->x + node->sprite_offset_x + g_camera_matrix.t[0];
        position.vy = node->y + node->sprite_offset_y + g_camera_matrix.t[1];
        depth = node->z;
        if (depth < 0) {
            depth = 0;
        }
        if (depth > EFFECT_SPRITE_OT_DEPTH_MAX) {
            depth = EFFECT_SPRITE_OT_DEPTH_MAX;
        }
        effect_render_submit_sprite_to_ordering_table(
            node->sprite_block, &position, node->screen_rotation_angle, &g_camera_zoom, g_effect_otag + depth);
        break;
    case EFFECT_LIST_NODE_WORLD_UNSCALED:
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        setVector(&input, node->x + node->sprite_offset_x, node->y + node->sprite_offset_y, node->z);
        RotTrans(&input, &output, &output.pad);
        depth = output.vz >> 2;
        switch (node->depth_mode) {
        case EFFECT_SPRITE_DEPTH_NEARER_8:
            depth -= 8;
            break;
        case EFFECT_SPRITE_DEPTH_FIXED_8:
            depth = 8;
            break;
        case EFFECT_SPRITE_DEPTH_FIXED_FARTHEST:
            depth = EFFECT_SPRITE_OT_DEPTH_MAX;
            break;
        case EFFECT_SPRITE_DEPTH_FIXED_16:
            depth = 16;
            break;
        case EFFECT_SPRITE_DEPTH_NEARER_16:
            depth -= 16;
            break;
        }
        if (depth < 0) {
            return;
        }
        if (depth > EFFECT_SPRITE_OT_DEPTH_MAX) {
            depth = EFFECT_SPRITE_OT_DEPTH_MAX;
        }
        position.vx = output.vx;
        position.vy = output.vy;
        effect_render_submit_sprite_to_ordering_table(
            node->sprite_block, &position, node->screen_rotation_angle, 0, g_effect_otag + depth);
        break;
    case EFFECT_LIST_NODE_SCREEN:
        position.vx = node->x + node->sprite_offset_x;
        position.vy = node->y + node->sprite_offset_y;
        depth = node->z;
        if (depth < 0) {
            depth = 0;
        }
        if (depth > EFFECT_SPRITE_OT_DEPTH_MAX) {
            depth = EFFECT_SPRITE_OT_DEPTH_MAX;
        }
        effect_render_submit_sprite_to_ordering_table(
            node->sprite_block, &position, node->screen_rotation_angle, 0, g_effect_otag + depth);
        break;
    }
}

/* The two flag meanings remain unknown. */
enum { EFFECT_REPLACED_FLAGS_MASK = 0x0006 };

/* Nothing in the game calls it. */
void effect_replace_flags_0006(effect_flag_prefix_t* object, s32 value) {
    object->flags = (object->flags & ~EFFECT_REPLACED_FLAGS_MASK) | (value & EFFECT_REPLACED_FLAGS_MASK);
}

/* Advance one effect slot (a particle of a secondary effect) by a frame: step its
 * motion, advance its frame script, in which each step shows a frame for hold
 * calls and the last step wraps to the first, and draw it as a billboard.
 *
 * Returns 0 when the slot has finished: a positive life counts down and ends the
 * slot at 0; a negative life ends it on the frame the script wraps. */
s32 effect_slot_update(s32 id) {
    effect_slot_t* slot = &g_effect_slots[id];
    s32 wrapped;
    s32 result;

    effect_slot_step_motion(&slot->motion);
    wrapped = 0;
    if (--slot->hold <= 0) {
        slot->frame = slot->script->step[slot->step_index].frame;
        slot->hold = slot->script->step[slot->step_index].hold;
        slot->step_index++;
        if (slot->step_index >= slot->script->count) {
            wrapped = 1;
            slot->step_index = 0;
        }
    }
    effect_slot_draw_billboard_sprite(slot);
    result = 1;
    if (slot->life < 0) {
        result = wrapped == 0;
    } else if (--slot->life == 0) {
        result = 0;
    }
    return result;
}

/* Initialize fresh render, script, work-record and sprite-node pools at setup/restart. */
void effect_reset_subsystems(void) {
    effect_render_init_state();
    effect_script_init_state_records();
    effect_particle_init_record_chain();
    effect_particle_init_free_list();
}

/* Seed the effect camera's current, target, start and saved values from the battle camera: its
 * rotation wrapped to one turn, its position in whole units (real coordinates >> 12) and its zoom. */
void effect_camera_seed_tracks(void) {
    SVECTOR* rotation = camera_rotation_get();
    VECTOR* coords;

    g_effect_camera_rotation_saved.vx = g_effect_camera_rotation_start.vx = g_effect_camera_rotation_target.vx
        = g_effect_camera_rotation_current.vx = rotation->vx & 0xfff;
    g_effect_camera_rotation_saved.vy = g_effect_camera_rotation_start.vy = g_effect_camera_rotation_target.vy
        = g_effect_camera_rotation_current.vy = rotation->vy & 0xfff;
    g_effect_camera_rotation_saved.vz = g_effect_camera_rotation_start.vz = g_effect_camera_rotation_target.vz
        = g_effect_camera_rotation_current.vz = rotation->vz & 0xfff;
    coords = camera_real_coords_get();
    g_effect_camera_position_saved.vx = g_effect_camera_position_start.vx = g_effect_camera_position_target.vx
        = g_effect_camera_position_current.vx = coords->vx >> 12;
    g_effect_camera_position_saved.vy = g_effect_camera_position_start.vy = g_effect_camera_position_target.vy
        = g_effect_camera_position_current.vy = coords->vy >> 12;
    g_effect_camera_position_saved.vz = g_effect_camera_position_start.vz = g_effect_camera_position_target.vz
        = g_effect_camera_position_current.vz = coords->vz >> 12;
    g_effect_camera_zoom_saved = g_effect_camera_zoom_start = g_effect_camera_zoom_target = g_effect_camera_zoom_current
        = *camera_zoom_get();
}

/* Set the effect camera's rotation, position and zoom tracks idle, clear two unknown globals and
 * reseed the tracks from the battle camera. */
void effect_camera_init(void) {
    s32 pad[2];
    g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
    g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
    g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
    g_effect_camera_unread_2 = 0;
    g_effect_camera_unread_1 = 0;
    effect_camera_seed_tracks();
    (void)pad;
}
