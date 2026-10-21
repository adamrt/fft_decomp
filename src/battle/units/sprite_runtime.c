/* Construct and update live unit sprites, run their unit and weapon SEQ scripts, and load SHP, SEQ and SPR assets. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/ai.h"
#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/battle_state.h"
#include "fft/camera.h"
#include "fft/cursor.h"
#include "fft/deploy.h"
#include "fft/effect_secondary.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/move.h"
#include "fft/move_step.h"
#include "fft/projectile.h"
#include "fft/result.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* sheet files */
/* Unit SPR file: 16 palettes, two uncompressed 4bpp image blocks, then
 * a variable-length compressed attack-image stream. */
typedef struct unit_spr_file {
    clut_t clut;                          /* 0x0000; 16 palettes of 16 colors */
    u8 pixels[256][128];                  /* 0x0200; 256 x 256 pixels, two per byte */
    u8 portrait_and_body_pixels[32][128]; /* 0x8200; 256 x 32 pixels, two per byte */
    u8 compressed_attack_image[0];        /* 0x9200; variable byte tail */
} unit_spr_file_t;
typedef char unit_spr_file_size_must_be_37376[sizeof(unit_spr_file_t) == 37376 ? 1 : -1];

/* Packed unit SEQ file. Values and offsets are little-endian byte
 * sequences, so this view needs no native 16-bit or 32-bit alignment. */
typedef struct unit_seq_file {
    u8 first_attack_sequence[2];   /* 0x000; unsigned little-endian 16-bit value */
    u8 first_sp2_sequence[2];      /* 0x002; unsigned little-endian 16-bit value */
    u8 sequence_offsets[0x100][4]; /* 0x004; signed little-endian 32-bit values, -1 means offset zero */
    u8 sequence_data_size[2];      /* 0x404; unsigned little-endian 16-bit value */
    u8 sequence_data[0];           /* 0x406; variable byte tail */
} unit_seq_file_t;
typedef char unit_seq_file_size_must_be_1030[sizeof(unit_seq_file_t) == 1030 ? 1 : -1];

/* file layouts */
/* Serialized SHP/SEQ values are little-endian byte sequences. These file
 * views have byte alignment; their decoded RAM tables remain separate. */

/* A unit SHP frame set: primary at file +8; an optional submerged set is
 * addressed by the file header. Only the first 0xd0 offsets are installed. */
typedef struct unit_shp_frame_set {
    u8 frame_offsets[0x100][4]; /* 0x000; signed 32-bit values, -1 resolves to offset zero */
    u8 frame_data_size[2];      /* 0x400; unsigned 16-bit value */
    u8 frame_data[0];           /* 0x402; variable tail */
} unit_shp_frame_set_t;
typedef char unit_shp_frame_set_size_must_be_1026[sizeof(unit_shp_frame_set_t) == 1026 ? 1 : -1];

typedef struct unit_shp_file {
    u8 submerged_frame_set_offset[4];       /* 0x000; 8 means both native tables share the primary set */
    u8 first_attack_frame[2];               /* 0x004; unsigned 16-bit value */
    u8 first_sp2_frame[2];                  /* 0x006; unsigned 16-bit value */
    unit_shp_frame_set_t primary_frame_set; /* 0x008 */
} unit_shp_file_t;
typedef char unit_shp_file_size_must_be_1034[sizeof(unit_shp_file_t) == 1034 ? 1 : -1];

/* WEP/EFF SHP has a different header and twice as many frame offsets. */
typedef struct weapon_shp_file {
    u8 _unused_000[4];          /* 0x000; loader skips this 32-bit value */
    u8 first_frame[0x20][2];    /* 0x004; unsigned zero-frame indices by weapon type */
    u8 frame_offsets[0x200][4]; /* 0x044; signed 32-bit values, -1 resolves to offset zero */
    u8 frame_data_size[2];      /* 0x844; unsigned 16-bit value */
    u8 frame_data[0];           /* 0x846; variable tail */
} weapon_shp_file_t;
typedef char weapon_shp_file_size_must_be_2118[sizeof(weapon_shp_file_t) == 2118 ? 1 : -1];

/* WEP/EFF SEQ stores 256 offsets followed by a variable sequence payload. */
typedef struct packed_seq_file {
    u8 _unused_000[4];  /* 0x000; WEP/EFF loader skips the 32-bit header value */
    u8 entry[0x100][4]; /* 0x004; signed sequence offsets, -1 resolves to zero */
    u8 payload_size[2]; /* 0x404; unsigned 16-bit value */
    u8 payload[0];      /* 0x406; variable sequence data */
} packed_seq_file_t;
typedef char packed_seq_file_size_must_be_1030[sizeof(packed_seq_file_t) == 1030 ? 1 : -1];

/* OTHER.SPR: the palette helper copies only the first 21 of 32 palettes. */
typedef struct other_spr_file {
    palette_t palettes[32]; /* 0x0000 */
    u8 pixels[256 * 128];   /* 0x0400; uncompressed 256x256, 4bpp */
} other_spr_file_t;
typedef char other_spr_file_size_must_be_33792[sizeof(other_spr_file_t) == 33792 ? 1 : -1];

/* WEP.SPR contains three CLUT/image pairs: weapons, trails and particles. */
typedef struct weapon_spr_file {
    clut_t weapon_clut;           /* 0x00000 */
    u8 weapon_pixels[256][128];   /* 0x00200; 4bpp */
    clut_t trail_clut;            /* 0x08200 */
    u8 trail_pixels[256][128];    /* 0x08400; 4bpp */
    clut_t particle_clut;         /* 0x10400 */
    u8 particle_pixels[144][128]; /* 0x10600; 4bpp */
} weapon_spr_file_t;

/* Sprite part width/height in 8-pixel tiles, indexed by SHP size bits. */
extern size32_t g_sprite_part_sizes[16];

/* Loaded and fallback SHP/SEQ data. */
extern u8 g_sprite_shp_frame_data_buffer[];
extern u8 g_sprite_seq_data_buffer[];
extern u8 g_sprite_fallback_shp_frame_data[];
extern u8 g_sprite_fallback_seq_data[];

/* The SHP/SEQ loaders advance these allocation cursors by byte counts. */
extern u8* g_sprite_shp_frame_data_cursor;
extern u8* g_sprite_seq_data_cursor;
extern s32 g_status_bubble_delay;

/* The display rotations that the upper five bits of a frame's first byte select, ONE to the turn. */
extern u16 g_sprite_y_rotations[32];
extern clut_t g_sprite_weapon_clut;

/* Trail CLUT copied from WEP.SPR at offset 0x8200. */
extern clut_t g_sprite_trail_clut;

/* The 24 orders in which unit_layers_draw queues a unit's
 * four layers: 0 is the unit's own sprite, n its sprite block n - 1. */
extern s32 g_animation_layer_priorities[24][4];

/* By sprite_shp_type_e: the frame a unit is left on when its sequence ends or its attack image is taken away, seen
 * from behind and from the front. */
extern u8 g_animation_targeted_frame_back[];
extern u8 g_animation_targeted_frame_front[];

/* Set before allocation and read acceptance: a failed request can be skipped
 * on the next step because its ID already matches. */
static s32 g_sprite_last_requested_seq_id;
static s32 g_sprite_last_requested_shp_id;

static file_extent_t g_seq_files[9] = { { 57037, 8192 }, { 57045, 8192 }, { 57053, 4096 }, { 57055, 6144 },
    { 57058, 4096 }, { 57060, 4096 }, { 57062, 4096 }, { 57064, 4096 }, { 0, 0 } };

static file_extent_t g_shp_files[9] = { { 57017, 8192 }, { 57021, 10240 }, { 57026, 8192 }, { 57030, 4096 },
    { 57032, 4096 }, { 0, 0 }, { 57034, 2048 }, { 57035, 4096 }, { 0, 0 } };

static file_extent_t g_weapon_seq_files[2] = { { 57072, 4096 }, { 57074, 4096 } };

static file_extent_t g_weapon_shp_files[2] = { { 57066, 6144 }, { 57069, 6144 } };

static file_extent_t g_effect_seq_files[2] = { { 57080, 2048 }, { 0, 0 } };

static file_extent_t g_effect_shp_files[2] = { { 57076, 4096 }, { 0, 0 } };

static file_extent_t g_sheet_files[159] = { { 0, 0 }, { 59021, 45056 }, { 59043, 45056 }, { 59065, 45056 },
    { 57586, 45056 }, { 57608, 45056 }, { 57630, 45056 }, { 57229, 45056 }, { 59772, 45056 }, { 57519, 45056 },
    { 58998, 47104 }, { 57913, 45056 }, { 58247, 45056 }, { 58954, 45056 }, { 57805, 45056 }, { 59087, 45056 },
    { 59839, 47104 }, { 57363, 45056 }, { 57827, 43008 }, { 59308, 47104 }, { 57935, 45056 }, { 58932, 45056 },
    { 57848, 45056 }, { 57957, 45056 }, { 57652, 45056 }, { 58976, 45056 }, { 58600, 43008 }, { 57720, 45056 },
    { 57742, 38912 }, { 57319, 45056 }, { 58356, 45056 }, { 57408, 45056 }, { 59685, 45056 }, { 57341, 45056 },
    { 58800, 45056 }, { 59132, 45056 }, { 59619, 45056 }, { 57979, 45056 }, { 58001, 45056 }, { 58023, 45056 },
    { 58045, 45056 }, { 58067, 45056 }, { 58089, 45056 }, { 58111, 45056 }, { 58133, 45056 }, { 58155, 45056 },
    { 57251, 45056 }, { 58177, 45056 }, { 57273, 45056 }, { 57186, 45056 }, { 57473, 43008 }, { 59794, 45056 },
    { 57164, 45056 }, { 58334, 45056 }, { 59353, 49152 }, { 58509, 47104 }, { 58888, 45056 }, { 58621, 45056 },
    { 59286, 45056 }, { 58578, 45056 }, { 59596, 47104 }, { 58465, 45056 }, { 59816, 47104 }, { 59707, 45056 },
    { 58199, 49152 }, { 57295, 49152 }, { 58509, 47104 }, { 58554, 49152 }, { 59532, 45056 }, { 57141, 47104 },
    { 58888, 45056 }, { 59421, 45056 }, { 59109, 47104 }, { 58378, 49152 }, { 59862, 38912 }, { 59881, 38912 },
    { 59900, 38912 }, { 59919, 38912 }, { 59938, 38912 }, { 59957, 38912 }, { 59976, 38912 }, { 59995, 38912 },
    { 60014, 38912 }, { 60033, 38912 }, { 60052, 38912 }, { 60071, 38912 }, { 60090, 47104 }, { 58621, 45056 },
    { 58621, 45056 }, { 58621, 45056 }, { 58621, 45056 }, { 58621, 45056 }, { 58621, 45056 }, { 57541, 45056 },
    { 58402, 40960 }, { 58422, 40960 }, { 58621, 45056 }, { 58643, 45056 }, { 58312, 45056 }, { 58334, 45056 },
    { 58465, 45056 }, { 58487, 45056 }, { 59707, 45056 }, { 59729, 45056 }, { 58712, 45056 }, { 58734, 45056 },
    { 59331, 45056 }, { 59353, 49152 }, { 58509, 47104 }, { 58532, 45056 }, { 59510, 45056 }, { 59532, 45056 },
    { 59398, 47104 }, { 59421, 45056 }, { 59466, 45056 }, { 59488, 45056 }, { 59641, 45056 }, { 59663, 45056 },
    { 58888, 45056 }, { 58910, 45056 }, { 57761, 45056 }, { 57783, 45056 }, { 59154, 45056 }, { 59176, 45056 },
    { 59198, 45056 }, { 59220, 45056 }, { 58822, 45056 }, { 58844, 45056 }, { 59242, 45056 }, { 59264, 45056 },
    { 57870, 45056 }, { 58866, 45056 }, { 58756, 45056 }, { 58778, 45056 }, { 57494, 51200 }, { 57892, 43008 },
    { 57452, 43008 }, { 58269, 45056 }, { 58291, 43008 }, { 59377, 43008 }, { 59751, 43008 }, { 57208, 43008 },
    { 59554, 45056 }, { 59576, 40960 }, { 58442, 47104 }, { 58665, 49152 }, { 58689, 47104 }, { 57385, 47104 },
    { 57674, 47104 }, { 57697, 47104 }, { 57430, 45056 }, { 58223, 49152 }, { 57674, 47104 }, { 57563, 47104 },
    { 59443, 47104 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };

static unit_runtime_t* runtime_slot_claim(s16 requested_runtime_id);

static void unit_seq_run(unit_runtime_t* unit, animation_state_t* state, s32 advance);

static void weapon_seq_run(unit_runtime_t* unit, sprite_block_t* sprite, s32 index, s32 advance);

/*
 * Claim a unit runtime record slot and stamp its runtime ID.
 *
 * Ids below 16 take their own slot, failing if another in-use slot already
 * carries that id or the slot is taken. Larger ids take the first free slot,
 * whose index becomes the stored runtime ID; exhausting all 16 raises
 * SYSTEM_ERROR_NO_UNIT_RUNTIME_SLOT. Returns 0 on failure.
 */
static unit_runtime_t* runtime_slot_claim(s16 requested_runtime_id) {
    s32 i;

    if (requested_runtime_id < UNIT_RUNTIME_SLOT_COUNT) {
        for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
            if (g_unit_slot_states[i].in_use != 0 && g_unit_slot_states[i].unit.runtime_id == requested_runtime_id) {
                break;
            }
        }
        if (i < UNIT_RUNTIME_SLOT_COUNT) {
            return 0;
        }
        if (g_unit_slot_states[requested_runtime_id].in_use != 0) {
            return 0;
        }
        g_unit_slot_states[requested_runtime_id].in_use = 1;
        g_unit_slot_states[requested_runtime_id].unit.runtime_id = requested_runtime_id;
        return &g_unit_slot_states[requested_runtime_id].unit;
    }
    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        if (g_unit_slot_states[i].in_use == 0) {
            g_unit_slot_states[i].in_use = 1;
            g_unit_slot_states[i].unit.runtime_id = i;
            return &g_unit_slot_states[i].unit;
        }
    }
    system_error_handle(SYSTEM_ERROR_NO_UNIT_RUNTIME_SLOT);
    return 0;
}

/* Fill a sprite display from SHP frame `src`: each part takes its shift, a texture position in
 * 8-pixel tiles, its size from g_sprite_part_sizes and its flip bits; the frame's blend bits
 * are merged into texture page `tpage`. */
static void frame_parts_load(unit_runtime_t* unit, sprite_display_t* display, const sprite_frame_t* src, s32 tpage) {
    const sprite_frame_part_t* part;
    u32 i;
    u32 count;
    u8 flags;
    u16 attr;
    u16 size;
    u32 tile;
    s32 u;
    s32 v;
    s32 width;
    s32 height;

    flags = src->flags;
    count = display->part_count;
    display->tpage = (flags & SPRITE_DISPLAY_BLEND_MASK) | tpage;
    display->clut = unit->clut;
    for (i = 0; i < count; i++) {
        part = &src->parts[i];
        attr = part->attributes;
        size = (attr & SPRITE_SHP_PART_SIZE_MASK) >> 10;
        width = g_sprite_part_sizes[size].width;
        height = g_sprite_part_sizes[size].height;
        tile = attr & SPRITE_SHP_PART_TILE_MASK;
        u = (tile & 0x1f) * 8;
        v = (tile >> 5) * 8;
        sprite_display_store_data(display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8,
            (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13));
    }
}

/* Return the frame tables that a unit frame id indexes and the frame itself:
 * a sprite_frame_t pointer.
 *
 * A frame below SPRITE_SHP_FRAME_EVTCHR_FIRST is one of the unit's own SHP.
 * Its submerged table replaces the plain one for a rider that is not
 * jumping, and for a unit on a water tile whose surface is 12 or more above
 * the unit's feet; from 36 above, the unit shows submerged frame 0 whatever
 * the frame id. A unit that floats or moves on water, a crystal and a
 * treasure keep the plain table, and a chicken or a frog changes at 4.
 * Higher frame ids index the SHP of an EVTCHR slot: slot 0 for the sequences
 * below 600 and slot 1 for the rest, the submerged table on a water tile. */
static void frame_unit_shp_select(unit_runtime_t* unit, animation_state_t* state, u16 frame, u16 sequence_id,
    const sprite_unit_shp_frame_tables_t** out_table, const sprite_frame_t** out_frame) {
    battle_map_tile_t* tile;
    const sprite_unit_shp_frame_tables_t* table;
    const sprite_frame_t* selected;
    s32 flags;
    s32 water_height;
    s32 unit_height;

    tile = battle_map_tile_get_pointer((unit->world_coords.vx / BATTLE_MAP_TILE_SIZE),
        (unit->world_coords.vz / BATTLE_MAP_TILE_SIZE), unit->map_layer);
    if (frame < SPRITE_SHP_FRAME_EVTCHR_FIRST) {
        table = state->shp;
        if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
            if (unit->status_flags_5_6 & (UNIT_STATUS_JUMP | UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) {
                selected = table->frames[frame];
            } else {
                selected = table->submerged_frames[frame];
            }
        } else if (tile->depth_half_height.depth) {
            if (unit->stats != 0) {
                flags = move_get_effective_flags(unit->stats);
                /* The combined test keeps the table pointer's reference count
                 * low enough for the target's s0/s1 allocation. */
                if (unit->status_flags_5_6 & UNIT_STATUS_FLOAT) {
                    selected = table->frames[frame];
                } else if ((flags & MOVE_EFFECTIVE_ON_WATER) || (flags & MOVE_EFFECTIVE_FLOAT)) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == SPRITE_SHEET_ID_CRYSTAL) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == SPRITE_SHEET_ID_TREASURE) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == SPRITE_SHEET_ID_CHICKEN
                    || unit->spritesheet_id == SPRITE_SHEET_ID_FROG) {
                    if ((tile->height + tile->depth_half_height.depth) * 12 >= -unit->world_coords.vy + 4) {
                        selected = table->submerged_frames[frame];
                    } else {
                        selected = table->frames[frame];
                    }
                } else {
                    water_height = (tile->height + tile->depth_half_height.depth) * 12;
                    unit_height = -unit->world_coords.vy;
                    if (water_height >= unit_height + 0x24) {
                        selected = table->submerged_frames[0];
                    } else if (water_height >= unit_height + 0xc) {
                        selected = table->submerged_frames[frame];
                    } else {
                        selected = table->frames[frame];
                    }
                }
            } else {
                water_height = (tile->height + tile->depth_half_height.depth) * 12;
                unit_height = -unit->world_coords.vy;
                if (water_height >= unit_height + 0x24) {
                    selected = table->submerged_frames[0];
                } else if (water_height >= unit_height + 0xc) {
                    selected = table->submerged_frames[frame];
                } else {
                    selected = table->frames[frame];
                }
            }
        } else {
            selected = table->frames[frame];
        }
    } else {
        table = &g_sprite_evtchr_slots[1].shp.tables;
        if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_1) {
            table = &g_sprite_evtchr_slots[0].shp.tables;
        }
        if (tile->depth_half_height.depth) {
            selected = table->submerged_frames[frame - SPRITE_SHP_FRAME_EVTCHR_FIRST];
        } else {
            selected = table->frames[frame - SPRITE_SHP_FRAME_EVTCHR_FIRST];
        }
    }
    *out_table = table;
    *out_frame = selected;
}

/* Expand one SHP frame into the parts of a unit's sprite display.
 *
 * Where the frame's pictures are decides the texture page: the OTHER page
 * for the crystal, chicken, frog and treasure sheets; the unit's spritesheet
 * slot for frames below the SHP's first attack frame; above that, the EVTCHR
 * slot that holds the unit's attack image (no slot, no parts). Frames from
 * SPRITE_SHP_FRAME_EVTCHR_FIRST up belong to an EVTCHR slot's own SHP and
 * the sequence id says which slot.
 *
 * A 48x48 part from a spritesheet slot ends the frame: it is stored as two
 * 48x24 halves, the upper one flagged to draw nearer the camera. The two
 * Altima sheets keep theirs whole. */
void sprite_unit_frame_load(unit_runtime_t* unit, animation_state_t* state, u16 frame, u16 sequence_id) {
    const sprite_unit_shp_frame_tables_t* frame_table;
    const sprite_frame_t* frame_data;
    sprite_display_t* display;
    const sprite_frame_part_t* part;
    u32 y_offset;
    u32 count;
    u16 i;
    u8 flags;
    u16 attr;
    u16 size;
    s32 u;
    u32 tile_index;
    s16 part_flags;
    u32 row;

    y_offset = unit->special_graphic_y_offset;
    /* The unpackers never store the -1 tested here, and three bits
     * of part count cannot pass 8. */
    frame_unit_shp_select(unit, state, frame, sequence_id, &frame_table, &frame_data);
    if (frame_data == (const sprite_frame_t*)-1) {
        system_error_handle(SYSTEM_ERROR_SHP_FRAME);
    }
    display = state->display;
    count = display->part_count = (frame_data->part_count_and_rotation & 7) + 1;
    display->rotation = g_sprite_y_rotations[frame_data->part_count_and_rotation >> 3];
    if (count > 8) {
        count = 8;
        system_error_handle(SYSTEM_ERROR_FRAME_PART_COUNT);
    }
    flags = frame_data->flags;
    if (unit->spritesheet_id >= SPRITE_SHEET_ID_CRYSTAL) {
        display->tpage = (flags & SPRITE_DISPLAY_BLEND_MASK) | SPRITE_TPAGE_OTHER;
        display->clut = unit->clut;
        for (i = 0; i < count; i++) {
            part = &frame_data->parts[i];
            attr = part->attributes;
            size = (attr & SPRITE_SHP_PART_SIZE_MASK) >> 10;
            {
                u32 tile;
                s32 part_u;
                s16 part_v;
                s32 width;
                s32 height;
                width = g_sprite_part_sizes[size].width;
                height = g_sprite_part_sizes[size].height;
                tile = attr & SPRITE_SHP_PART_TILE_MASK;
                part_u = (tile & 0x1f) * 8;
                part_v = y_offset + (tile >> 5) * 8;
                sprite_display_store_data(display, i, part->x_shift, part->y_shift, part_u, part_v, width * 8,
                    height * 8,
                    (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13));
            }
        }
    } else if (frame < state->shp->first_attack_frame) {
        display->tpage = unit->tpage | (flags & SPRITE_DISPLAY_BLEND_MASK);
        display->clut = unit->clut;
        i = 0;
        if (count != 0) {
            do {
                part = &frame_data->parts[i];
                attr = part->attributes;
                size = (attr & SPRITE_SHP_PART_SIZE_MASK) >> 10;
                if (size == SPRITE_SHP_PART_SIZE_48X48) {
                    if (unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_SECOND_FORM
                        || unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_FIRST_FORM) {
                        u32 tile;
                        s32 part_u;
                        s16 part_v;
                        s32 width;
                        s32 height;
                        tile = attr & SPRITE_SHP_PART_TILE_MASK;
                        part_u = (tile & 0x1f) * 8;
                        part_v = y_offset + (tile >> 5) * 8;
                        width = g_sprite_part_sizes[size].width;
                        height = g_sprite_part_sizes[size].height;
                        sprite_display_store_data(display, i, part->x_shift, part->y_shift, part_u, part_v, width * 8,
                            height * 8,
                            (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                                | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13));
                    } else {
                        tile_index = attr & SPRITE_SHP_PART_TILE_MASK;
                        u = (tile_index & 0x1f) * 8;
                        row = tile_index >> 5;
                        part_flags = (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                            | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13);
                        sprite_display_store_data(
                            display, i, part->x_shift, part->y_shift + 0x18, u, row * 8 + 0x18, 0x30, 0x18, part_flags);
                        sprite_display_store_data(display, i + 1, part->x_shift, part->y_shift, u, row * 8, 0x30, 0x18,
                            part_flags | SPRITE_DISPLAY_NEARER);
                        display->part_count++;
                        return;
                    }
                } else {
                    u32 tile;
                    s32 part_u;
                    s16 part_v;
                    s32 width;
                    s32 height;
                    width = g_sprite_part_sizes[size].width;
                    height = g_sprite_part_sizes[size].height;
                    tile = attr & SPRITE_SHP_PART_TILE_MASK;
                    part_u = (tile & 0x1f) * 8;
                    part_v = y_offset + (tile >> 5) * 8;
                    sprite_display_store_data(display, i, part->x_shift, part->y_shift, part_u, part_v, width * 8,
                        height * 8,
                        (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                            | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13));
                }
            } while (++i < count);
        }
    } else if (frame < SPRITE_SHP_FRAME_EVTCHR_FIRST) {
        if ((g_sprite_evtchr_slots[0].owner & SPRITE_EVTCHR_SLOT_OWNER_ID_MASK) == unit->runtime_id) {
            frame_parts_load(unit, display, frame_data, SPRITE_TPAGE_EVTCHR_SLOT_0);
        } else if ((g_sprite_evtchr_slots[1].owner & SPRITE_EVTCHR_SLOT_OWNER_ID_MASK) == unit->runtime_id) {
            frame_parts_load(unit, display, frame_data, SPRITE_TPAGE_EVTCHR_SLOT_1);
        }
    } else if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_1) {
        frame_parts_load(unit, display, frame_data, SPRITE_TPAGE_EVTCHR_SLOT_0);
    } else {
        frame_parts_load(unit, display, frame_data, SPRITE_TPAGE_EVTCHR_SLOT_1);
    }
}

/* SEQ script step: moves the sprite offset by delta across the unit's facing: +x
 * facing north, -x facing south, +z facing west, -z facing east. A diagonal
 * facing matches no case. */
static void seq_lateral_shift(unit_runtime_t* unit, s8 delta) {
    switch (unit->facing & (UNIT_FACING_EAST | UNIT_FACING_NORTH)) {
    case UNIT_FACING_NORTH:
        unit->seq_offset.vx += delta;
        return;
    case UNIT_FACING_SOUTH:
        unit->seq_offset.vx -= delta;
        return;
    case UNIT_FACING_WEST:
        unit->seq_offset.vz += delta;
        return;
    case UNIT_FACING_EAST:
        unit->seq_offset.vz -= delta;
        return;
    }
}

/* SEQ script step: moves the sprite offset by delta along the height axis;
 * positive is down. */
static void seq_height_shift(unit_runtime_t* unit, s8 delta) {
    unit->seq_offset.vy += delta;
}

/* SEQ script step: moves the sprite offset by delta along the unit's facing
 * (north is +z, east is +x). A diagonal facing matches no case. */
static void seq_forward_shift(unit_runtime_t* unit, s8 delta) {
    switch (unit->facing & (UNIT_FACING_EAST | UNIT_FACING_NORTH)) {
    case UNIT_FACING_NORTH:
        unit->seq_offset.vz += delta;
        break;
    case UNIT_FACING_SOUTH:
        unit->seq_offset.vz -= delta;
        break;
    case UNIT_FACING_WEST:
        unit->seq_offset.vx -= delta;
        break;
    case UNIT_FACING_EAST:
        unit->seq_offset.vx += delta;
        break;
    }
}

/* Run a unit's SEQ sequence up to its next frame instruction, or to its end.
 *
 * `state->sequence_id` selects the sequence: one of the unit's own SEQ table,
 * or sequence n of an EVTCHR slot for 500 + n and 600 + n. The commands in
 * front of the frame instruction run at once (animation_seq_opcode_e); then the
 * frame's parts are expanded into the unit's display and the state keeps the
 * frame, its wait and the position after it. With `advance` clear the
 * position first steps back to the frame instruction read last, which shows
 * that frame again without restarting its wait.
 *
 * A sequence that ends leaves a unit on its SHP type's
 * g_animation_targeted_frame_front or _back frame with a wait of 0. The
 * unpackers resolve a missing sequence to the start of the sequence data,
 * never to -1, so the SYSTEM_ERROR_SEQ_ANIMATION test cannot fire. */
static void unit_seq_run(unit_runtime_t* unit, animation_state_t* state, s32 advance) {
    sprite_unit_seq_table_t* seq;
    sprite_block_t* block;
    unit_runtime_t* target;
    u16 sequence_id;
    u16 index;
    u16 frame;
    u16 i;
    s16 pos;
    s16 attack_id;
    u16 value;
    s16 offset;
    u8 block_id;
    u8 block_sequence;
    u8 timer;

    sequence_id = state->sequence_id;
    pos = state->script_pos;
    index = sequence_id;
    if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
        seq = state->seq;
    } else if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_1) {
        index = sequence_id - ANIMATION_SEQUENCE_EVTCHR_SLOT_0;
        seq = &g_sprite_evtchr_slots[0].seq.header;
    } else {
        seq = &g_sprite_evtchr_slots[1].seq.header;
        index = sequence_id - ANIMATION_SEQUENCE_EVTCHR_SLOT_1;
    }

    /* A frame instruction is two bytes, so this backs up to the frame read last. */
    if (!advance) {
        pos -= 2;
    }
    if (seq->sequences[index] == (u8*)-1) {
        system_error_handle(SYSTEM_ERROR_SEQ_ANIMATION);
    }

    for (;;) {
        u8 op;

        if (seq->sequences[index][pos] == ANIMATION_SEQ_OP_PREFIX) {
            pos++;
            op = seq->sequences[index][pos++];
            switch (op) {
            case ANIMATION_SEQ_OP_FLIP_HORIZONTAL:
                state->flags ^= SPRITE_DISPLAY_HORIZONTAL_FLIP;
                break;
            case ANIMATION_SEQ_OP_FLIP_VERTICAL:
                state->flags ^= SPRITE_DISPLAY_VERTICAL_FLIP;
                break;
            case ANIMATION_SEQ_OP_SHOW_SHADOW:
                unit->shadow_graphic_trigger = 1;
                break;
            case ANIMATION_SEQ_OP_HIDE_SHADOW:
                unit->shadow_graphic_trigger = 0;
                break;
            case ANIMATION_SEQ_OP_END_ANIMATION:
            case ANIMATION_SEQ_OP_PAUSE_ANIMATION:
                if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
                    /* Odd sequence ids are the views from behind. */
                    if (sequence_id % 2) {
                        frame = g_animation_targeted_frame_back[g_sprite_sheet_data[unit->spritesheet_id].shp_id];
                    } else {
                        frame = g_animation_targeted_frame_front[g_sprite_sheet_data[unit->spritesheet_id].shp_id];
                    }
                    unit->seq_offset.vz = 0;
                    unit->seq_offset.vy = 0;
                    unit->seq_offset.vx = 0;
                    sprite_unit_frame_load(unit, state, frame, sequence_id);
                }
                state->wait = 0;
                return;
            case ANIMATION_SEQ_OP_RESTART:
                pos = 0;
                state->restart_count++;
                break;
            case ANIMATION_SEQ_OP_CLEAR_ROTATION:
                state->display->rotation = 0;
                break;
            case ANIMATION_SEQ_OP_STRIKE_TARGETS:
                if (g_battle_state == BATTLE_STATE_EVENT) {
                    break;
                }
                /* The abilities whose hit is the weapon swing itself: a plain
                 * attack (0), the frog's, the Battle Skill breaks, the Snipe
                 * shots, Jump and Charge. */
                attack_id = unit->strike.last_attack_id;
                if (attack_id == 0 || attack_id == ABILITY_ID_FROG_ATTACK
                    || (attack_id >= ABILITY_ID_BATTLE_SKILL_HEAD_BREAK
                        && attack_id <= ABILITY_ID_BATTLE_SKILL_MIND_BREAK)
                    || (attack_id >= ABILITY_ID_SNIPE_LEG_AIM && attack_id <= ABILITY_ID_SNIPE_SEAL_EVIL)
                    || (attack_id >= ABILITY_ID_JUMP_FIRST && attack_id < ABILITY_ID_MATH_FIRST)) {
                    if (unit->strike.target_count != 0) {
                        for (i = 0; i < unit->strike.target_count; i++) {
                            target = unit_battle_get(unit->strike.target_list[i]);
                            if (target != 0) {
                                action_start_target_response(unit, target);
                                animation_shield_block_attacker_set(unit, target);
                                result_runtime_display_prepare(target->runtime_id);
                                effect_secondary_weapon_hit_start(unit, 0);
                            }
                        }
                    }
                }
                break;
            case ANIMATION_SEQ_OP_LAUNCH_PROJECTILE:
                effect_secondary_projectile_start(unit);
                /* fallthrough: the command's two operand bytes are skipped like the unknowns' */
            case ANIMATION_SEQ_OP_UNKNOWN_C7:
            case ANIMATION_SEQ_OP_UNKNOWN_C8:
            case ANIMATION_SEQ_OP_UNKNOWN_C9:
            case ANIMATION_SEQ_OP_UNKNOWN_E6:
            case ANIMATION_SEQ_OP_UNKNOWN_E7:
            case ANIMATION_SEQ_OP_UNKNOWN_E8:
            case ANIMATION_SEQ_OP_UNKNOWN_EA:
            case ANIMATION_SEQ_OP_UNKNOWN_F1:
            case ANIMATION_SEQ_OP_UNKNOWN_F8:
            case ANIMATION_SEQ_OP_UNKNOWN_FB:
                pos += 2;
                break;
            case ANIMATION_SEQ_OP_RETURN:
                sequence_id = state->saved_sequence_id;
                pos = state->saved_script_pos;
                index = sequence_id;
                if (sequence_id >= ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
                    if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_1) {
                        index = sequence_id - ANIMATION_SEQUENCE_EVTCHR_SLOT_0;
                    } else {
                        index = sequence_id - ANIMATION_SEQUENCE_EVTCHR_SLOT_1;
                    }
                }
                break;
            case ANIMATION_SEQ_OP_WAIT_FOR_EFFECT:
                if (g_battle_animation_wait_state != 0) {
                    /* Jump offsets count from the prefix byte, two bytes behind pos. */
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case ANIMATION_SEQ_OP_PLAY_SOUND:
                value = seq->sequences[index][pos++];
                unit_sfx_play(unit, value);
                break;
            case ANIMATION_SEQ_OP_MOVE_UNIT_RL:
                seq_lateral_shift(unit, seq->sequences[index][pos++]);
                break;
            case ANIMATION_SEQ_OP_MOVE_UNIT_DU:
                seq_height_shift(unit, seq->sequences[index][pos++]);
                break;
            case ANIMATION_SEQ_OP_MOVE_UNIT_FB:
                seq_forward_shift(unit, seq->sequences[index][pos++]);
                break;
            case ANIMATION_SEQ_OP_SET_WAIT_BIAS:
                state->wait_bias = (s8)seq->sequences[index][pos++];
                break;
            case ANIMATION_SEQ_OP_SET_FRAME_BIAS:
                state->frame_bias = (s8)seq->sequences[index][pos++];
                break;
            case ANIMATION_SEQ_OP_CALL_SEQUENCE:
                state->saved_sequence_id = sequence_id;
                sequence_id = seq->sequences[index][pos++];
                index = sequence_id;
                if (sequence_id >= ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
                    if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_1) {
                        index = sequence_id - ANIMATION_SEQUENCE_EVTCHR_SLOT_0;
                    } else {
                        index = sequence_id - ANIMATION_SEQUENCE_EVTCHR_SLOT_1;
                    }
                }
                state->saved_script_pos = pos;
                pos = 0;
                break;
            case ANIMATION_SEQ_OP_SET_LAYER_PRIORITY:
                unit->layer_priority = seq->sequences[index][pos++];
                break;
            case ANIMATION_SEQ_OP_START_BLOCK_SEQUENCE:
                block_id = seq->sequences[index][pos++];
                block_sequence = seq->sequences[index][pos++];
                if (block_id == 0) {
                    system_error_handle(SYSTEM_ERROR_SEQ_OPCODE);
                }
                block = &unit->sprite_blocks[block_id - 1];
                block->wait = 1;
                block->sequence_id = block_sequence;
                block->script_pos = 0;
                block->wait_bias = 0;
                block->frame_bias = 0;
                block->repeat_count = 0;
                block->trigger = 1;
                break;
            case ANIMATION_SEQ_OP_REPEAT:
                offset = (s8)seq->sequences[index][pos++];
                value = seq->sequences[index][pos++];
                /* pos is four bytes past the prefix the offset counts from. */
                if (state->repeat_count != 0) {
                    if (--state->repeat_count != 0) {
                        pos += offset - 4;
                    }
                } else {
                    pos += offset - 4;
                    state->repeat_count = value;
                }
                break;
            case ANIMATION_SEQ_OP_MOVE_SCREEN_OFFSET:
                unit->screen_offset.vx += (s8)seq->sequences[index][pos++];
                unit->screen_offset.vy += (s8)seq->sequences[index][pos++];
                break;
            case ANIMATION_SEQ_OP_SET_ROTATION:
                value = seq->sequences[index][pos++];
                value |= seq->sequences[index][pos++] << 8;
                state->display->rotation = value;
                break;
            case ANIMATION_SEQ_OP_MOVE_UNIT:
                seq_lateral_shift(unit, seq->sequences[index][pos++]);
                seq_height_shift(unit, seq->sequences[index][pos++]);
                seq_forward_shift(unit, seq->sequences[index][pos++]);
                break;
            case ANIMATION_SEQ_OP_UNKNOWN_F7:
                pos += 3;
                break;
            case ANIMATION_SEQ_OP_MOVE_FORWARD_1:
                seq_forward_shift(unit, 1);
                break;
            case ANIMATION_SEQ_OP_MOVE_FORWARD_2:
                seq_forward_shift(unit, 2);
                break;
            case ANIMATION_SEQ_OP_MOVE_DOWN_1:
                seq_height_shift(unit, 1);
                break;
            case ANIMATION_SEQ_OP_MOVE_DOWN_2:
                seq_height_shift(unit, 2);
                break;
            case ANIMATION_SEQ_OP_MOVE_BACKWARD_1:
                seq_forward_shift(unit, -1);
                break;
            case ANIMATION_SEQ_OP_MOVE_BACKWARD_2:
                seq_forward_shift(unit, -2);
                break;
            case ANIMATION_SEQ_OP_MOVE_UP_1:
                seq_height_shift(unit, -1);
                break;
            case ANIMATION_SEQ_OP_MOVE_UP_2:
                seq_height_shift(unit, -2);
                break;
            case ANIMATION_SEQ_OP_SHOW_ITEM:
                unit->item_display_active = 1;
                break;
            case ANIMATION_SEQ_OP_HIDE_ITEM:
                unit->item_display_active = 0;
                break;
            case ANIMATION_SEQ_OP_WAIT_FOR_INPUT:
                if (g_battle_window_closed == 0) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case ANIMATION_SEQ_OP_PLAY_WEAPON_SOUND:
                action_play_weapon_sfx(unit, seq->sequences[index][pos++]);
                break;
            case ANIMATION_SEQ_OP_WAIT_FOR_EFFECT_LOAD:
                if (g_battle_animation_wait_state == 1) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case ANIMATION_SEQ_OP_SET_ITEM_POSITION:
                unit->item_display_x_offset = seq->sequences[index][pos++];
                unit->item_display_y_offset = seq->sequences[index][pos++];
                break;
            case ANIMATION_SEQ_OP_START_DISTORTION:
                /* The operand stores the id less 2, so a script cannot start NONE or the scale pulse. */
                unit->distortion_animation_id = seq->sequences[index][pos++] + 2;
                timer = seq->sequences[index][pos++];
                unit->distortion_phase = 0;
                unit->distortion_timer = timer;
                break;
            case ANIMATION_SEQ_OP_WAIT_FOR_DISTORTION:
                if (unit->distortion_animation_id != ANIMATION_DISTORTION_NONE) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case ANIMATION_SEQ_OP_JUMP:
                offset = (s8)seq->sequences[index][pos];
                pos += offset - 2;
                break;
            case ANIMATION_SEQ_OP_START_POSITION_COPIES:
                sprite_afterimage_init(unit);
                break;
            case ANIMATION_SEQ_OP_STOP_POSITION_COPIES:
                unit->position_copies_active = 0;
                break;
            case ANIMATION_SEQ_OP_NOP:
                break;
            case ANIMATION_SEQ_OP_UNKNOWN_CA:
            case ANIMATION_SEQ_OP_UNKNOWN_D7:
            case ANIMATION_SEQ_OP_UNKNOWN_E3:
            case ANIMATION_SEQ_OP_UNKNOWN_E4:
            case ANIMATION_SEQ_OP_UNKNOWN_E9:
            case ANIMATION_SEQ_OP_UNKNOWN_ED:
            case ANIMATION_SEQ_OP_UNKNOWN_F3:
            case ANIMATION_SEQ_OP_UNKNOWN_F4:
            case ANIMATION_SEQ_OP_UNKNOWN_F5:
                pos++;
                break;
            default:
                system_error_handle(SYSTEM_ERROR_SEQ_OPCODE);
                break;
            }
        } else {
            /* A frame instruction: the frame, then its wait, which a replay skips. */
            frame = state->frame_bias + seq->sequences[index][pos++];
            if (advance) {
                state->wait = seq->sequences[index][pos++] + state->wait_bias;
                /* A bias below the wait wraps the 16-bit value: wait one frame. */
                if (state->wait > 0x100) {
                    state->wait = 1;
                }
            } else {
                pos++;
            }
            sprite_unit_frame_load(unit, state, frame, sequence_id);
            state->script_pos = pos;
            state->frame = frame;
            state->sequence_id = sequence_id;
            return;
        }
    }
}

/* Rebuilds the casting unit's sprite parts for its current animation and frame. */
static void frame_casting_unit_load(void) {
    unit_runtime_t* unit = unit_casting_get();

    if (unit != 0) {
        sprite_unit_frame_load(unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    }
}

/* Run a weapon or effect sprite block's sequence up to its next frame
 * instruction, or to its end, as unit_seq_run does for the
 * unit's own sprite. The unit-only commands are errors here, the ones that
 * move a unit only skip their operands, and ANIMATION_SEQ_OP_STRIKE_TARGETS and
 * the two shadow commands do nothing.
 *
 * A frame id counts from the first frame of the unit's weapon type
 * (sprite_weapon_shp_t.first_frame). The frame's parts, three at most,
 * are expanded into the block's display: a weapon block draws them from the
 * weapon sheet, at the row of the used item's graphic, and an effect block
 * from the effect sheet. `index` is the block's number; it selects the CLUT
 * row.
 *
 * Matching constraints: the size table is indexed by element (`>> 10`); the
 * shift that combine later folds into `>> 7` keeps the script loop just large
 * enough that loop.c does not hoist the constant 1. `part` and `attr` are
 * function-scope (global allocation) while the other part temporaries are
 * block-local and `mode` is single-set, which gives the target's register
 * ties and schedule. */
static void weapon_seq_run(unit_runtime_t* unit, sprite_block_t* sprite, s32 index, s32 advance) {
    const sprite_weapon_shp_t* shp;
    u8** seq;
    u8 op;
    u8* data;
    u16 sequence_id;
    s16 pos;
    u16 frame;
    u16 graphic_v;
    const sprite_frame_t* src;
    sprite_display_t* display;
    sprite_block_t* block;
    const sprite_frame_part_t* part;
    s16 offset;
    u16 value;
    u8 block_id;
    u8 block_sequence;
    u32 parts;
    u8 flags;
    u16 i;
    u16 attr;

    sequence_id = sprite->sequence_id;
    graphic_v = g_sprite_item_graphic_data[unit->used_item_or_weapon_id].texture_row * 8;
    pos = sprite->script_pos;
    shp = sprite->shp;
    seq = sprite->seq;
    if (advance == 0) {
        pos -= 2;
    }
    if (sequence_id > 0x100) {
        system_error_handle(SYSTEM_ERROR_SEQ_ANIMATION);
    }
    if (seq[sequence_id] == (u8*)-1) {
        system_error_handle(SYSTEM_ERROR_SEQ_ANIMATION);
    }
    for (;;) {
        data = seq[sequence_id];
        if (data[pos] == ANIMATION_SEQ_OP_PREFIX) {
            pos++;
            op = seq[sequence_id][pos++];
            switch (op) {
            case ANIMATION_SEQ_OP_FLIP_HORIZONTAL:
                sprite->flags ^= SPRITE_DISPLAY_HORIZONTAL_FLIP;
                break;
            case ANIMATION_SEQ_OP_FLIP_VERTICAL:
                sprite->flags ^= SPRITE_DISPLAY_VERTICAL_FLIP;
                break;
            case ANIMATION_SEQ_OP_END_ANIMATION:
                /* Ending also switches the block off; both stop the script. */
                sprite->trigger = 0;
            case ANIMATION_SEQ_OP_PAUSE_ANIMATION:
                sprite->wait = 0;
                return;
            case ANIMATION_SEQ_OP_RESTART:
                sprite->restart_count++;
                pos = 0;
                break;
            case ANIMATION_SEQ_OP_CLEAR_ROTATION:
                sprite->display->rotation = 0;
                break;
            case ANIMATION_SEQ_OP_LAUNCH_PROJECTILE:
                effect_secondary_projectile_start(unit);
                /* fallthrough: the command's two operand bytes are skipped like the unknowns' */
            case ANIMATION_SEQ_OP_UNKNOWN_E6:
            case ANIMATION_SEQ_OP_UNKNOWN_E7:
            case ANIMATION_SEQ_OP_UNKNOWN_E8:
            case ANIMATION_SEQ_OP_UNKNOWN_EA:
            case ANIMATION_SEQ_OP_UNKNOWN_F1:
            case ANIMATION_SEQ_OP_UNKNOWN_F8:
            case ANIMATION_SEQ_OP_UNKNOWN_FB:
                pos += 2;
                break;
            case ANIMATION_SEQ_OP_RETURN:
                pos = sprite->saved_script_pos;
                sequence_id = sprite->saved_sequence_id;
                break;
            case ANIMATION_SEQ_OP_SET_LAYER_PRIORITY:
                unit->layer_priority = seq[sequence_id][pos++];
                break;
            case ANIMATION_SEQ_OP_WAIT_FOR_EFFECT:
                if (g_battle_animation_wait_state != 0) {
                    offset = (s8)seq[sequence_id][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case ANIMATION_SEQ_OP_PLAY_SOUND:
                value = seq[sequence_id][pos++];
                unit_sfx_play(unit, value);
                break;
            case ANIMATION_SEQ_OP_SET_WAIT_BIAS:
                sprite->wait_bias = (s8)seq[sequence_id][pos++];
                break;
            case ANIMATION_SEQ_OP_SET_FRAME_BIAS:
                sprite->frame_bias = (s8)seq[sequence_id][pos++];
                break;
            case ANIMATION_SEQ_OP_CALL_SEQUENCE:
                sprite->saved_sequence_id = sequence_id;
                sequence_id = seq[sequence_id][pos++];
                sprite->saved_script_pos = pos;
                pos = 0;
                break;
            case ANIMATION_SEQ_OP_MOVE_SCREEN_OFFSET:
                unit->screen_offset.vx += (s8)seq[sequence_id][pos++];
                unit->screen_offset.vy += (s8)seq[sequence_id][pos++];
                break;
            case ANIMATION_SEQ_OP_REPEAT:
                offset = (s8)seq[sequence_id][pos++];
                value = seq[sequence_id][pos++];
                if (sprite->repeat_count != 0) {
                    if (--sprite->repeat_count != 0) {
                        pos += offset - 4;
                    }
                } else {
                    pos += offset - 4;
                    sprite->repeat_count = value;
                }
                break;
            case ANIMATION_SEQ_OP_START_BLOCK_SEQUENCE:
                block_id = seq[sequence_id][pos++];
                block_sequence = seq[sequence_id][pos++];
                if (block_id == 0) {
                    system_error_handle(SYSTEM_ERROR_SEQ_OPCODE);
                }
                block = &unit->sprite_blocks[block_id - 1];
                block->sequence_id = block_sequence;
                block->script_pos = 0;
                block->wait = 1;
                block->wait_bias = 0;
                block->frame_bias = 0;
                block->repeat_count = 0;
                block->trigger = 1;
                break;
            case ANIMATION_SEQ_OP_SET_ROTATION:
                value = seq[sequence_id][pos++];
                value |= seq[sequence_id][pos++] << 8;
                sprite->display->rotation = value;
                break;
            case ANIMATION_SEQ_OP_UNKNOWN_F7:
            case ANIMATION_SEQ_OP_MOVE_UNIT:
                pos += 3;
                break;
            case ANIMATION_SEQ_OP_WAIT_FOR_EFFECT_LOAD:
                if (g_battle_animation_wait_state == 1) {
                    offset = (s8)seq[sequence_id][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case ANIMATION_SEQ_OP_JUMP:
                offset = (s8)seq[sequence_id][pos];
                pos += offset - 2;
                break;
            case ANIMATION_SEQ_OP_UNKNOWN_D7:
            case ANIMATION_SEQ_OP_UNKNOWN_E3:
            case ANIMATION_SEQ_OP_UNKNOWN_E4:
            case ANIMATION_SEQ_OP_UNKNOWN_E9:
            case ANIMATION_SEQ_OP_UNKNOWN_ED:
            case ANIMATION_SEQ_OP_MOVE_UNIT_FB:
            case ANIMATION_SEQ_OP_MOVE_UNIT_DU:
            case ANIMATION_SEQ_OP_MOVE_UNIT_RL:
            case ANIMATION_SEQ_OP_UNKNOWN_F3:
            case ANIMATION_SEQ_OP_UNKNOWN_F4:
            case ANIMATION_SEQ_OP_UNKNOWN_F5:
                pos++;
                break;
            case ANIMATION_SEQ_OP_STRIKE_TARGETS:
            case ANIMATION_SEQ_OP_HIDE_SHADOW:
            case ANIMATION_SEQ_OP_SHOW_SHADOW:
                break;
            default:
                system_error_handle(SYSTEM_ERROR_SEQ_OPCODE);
                break;
            }
        } else {
            frame = sprite->frame_bias + (data[pos++] + shp->first_frame[unit->equipped_weapon_type]);
            if (advance != 0) {
                sprite->wait = data[pos++] + sprite->wait_bias;
            } else {
                pos++;
            }
            src = shp->frames[frame];
            if (frame > 0x200) {
                system_error_handle(SYSTEM_ERROR_SHP_FRAME);
            }
            if (src == (const sprite_frame_t*)-1) {
                system_error_handle(SYSTEM_ERROR_SHP_FRAME);
            }
            display = sprite->display;
            parts = display->part_count = (src->part_count_and_rotation & 7) + 1;
            display->rotation = g_sprite_y_rotations[src->part_count_and_rotation >> 3];
            if (unit->sprite_display_flags & SPRITE_DISPLAY_HORIZONTAL_FLIP) {
                display->rotation = -display->rotation;
            }
            if (parts > 3) {
                parts = 3;
            }
            flags = src->flags;
            switch (sprite->graphic_type) {
            case SPRITE_GRAPHIC_WEAPON:
                display->tpage = (flags & SPRITE_DISPLAY_BLEND_MASK) | SPRITE_TPAGE_WEAPON;
                display->clut = unit->clut + index * 64 + 0x80; /* BATTLE_MAP_CLUT_ROW_ITEM_A or _B */
                for (i = 0; i < parts; i++) {
                    u32 size;
                    u32 tile;
                    s32 width;
                    s32 height;
                    s32 u;
                    s32 v;
                    u32 mode;

                    part = &src->parts[i];
                    attr = part->attributes;
                    size = (attr & SPRITE_SHP_PART_SIZE_MASK) >> 10;
                    width = g_sprite_part_sizes[size].width;
                    height = g_sprite_part_sizes[size].height;
                    tile = attr & SPRITE_SHP_PART_TILE_MASK;
                    u = (tile & 0x1f) * 8;
                    v = (tile >> 5) * 8 + graphic_v;
                    mode = (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13);
                    sprite_display_store_data(
                        display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8, mode);
                }
                break;
            case SPRITE_GRAPHIC_EFFECT:
                display->tpage = (flags & SPRITE_DISPLAY_BLEND_MASK) | SPRITE_TPAGE_EFFECT;
                display->clut = unit->clut + index * 64 + 0x80; /* BATTLE_MAP_CLUT_ROW_ITEM_A or _B */
                for (i = 0; i < parts; i++) {
                    u32 size;
                    u32 tile;
                    s32 width;
                    s32 height;
                    s32 u;
                    s32 v;
                    u32 mode;

                    part = &src->parts[i];
                    attr = part->attributes;
                    size = (attr & SPRITE_SHP_PART_SIZE_MASK) >> 10;
                    width = g_sprite_part_sizes[size].width;
                    height = g_sprite_part_sizes[size].height;
                    tile = attr & SPRITE_SHP_PART_TILE_MASK;
                    u = (tile & 0x1f) * 8;
                    v = (tile >> 5) * 8;
                    mode = (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13);
                    sprite_display_store_data(
                        display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8, mode);
                }
                break;
            }
            sprite->script_pos = pos;
            sprite->frame = frame;
            sprite->sequence_id = sequence_id;
            return;
        }
    }
}

/* Capture a unit's graphics into an EVTCHR VRAM cache slot.
 *
 * The unit's compressed spritesheet slot is expanded into `image_slot`'s
 * image buffer and uploaded to `slot`'s rectangle; the unit's 64x32 VRAM
 * block (column spritesheet slot / 8, row spritesheet slot % 8 of the
 * SPRITE_SHEET_STRIP_VRAM_X/Y area) is copied 200 lines below it. `slot` is then owned by the unit's attack
 * image (runtime id | SPRITE_EVTCHR_SLOT_OWNER_ATTACK_IMAGE), and the image
 * buffer stays busy for SPRITE_EVTCHR_SLOT_UPLOAD_FRAMES frames while the
 * upload completes. */
static void evtchr_unit_load(unit_runtime_t* unit, s32 slot, s32 image_slot) {
    RECT rect;

    sprite_attack_image_decompress(g_sprite_sheet_slots[unit->spritesheet_vram_slot].compressed_attack_image,
        g_sprite_evtchr_slots[image_slot].image_data);
    LoadImage(&g_sprite_evtchr_slots[slot].image_rect, (u32*)g_sprite_evtchr_slots[image_slot].image_data);
    rect.x = (unit->spritesheet_vram_slot >> 3) * SPRITE_SHEET_STRIP_WIDTH + SPRITE_SHEET_STRIP_VRAM_X;
    rect.y = (unit->spritesheet_vram_slot & 7) * SPRITE_SHEET_STRIP_HEIGHT + SPRITE_SHEET_STRIP_VRAM_Y;
    rect.w = SPRITE_SHEET_STRIP_WIDTH;
    rect.h = SPRITE_SHEET_STRIP_HEIGHT;
    MoveImage(&rect, g_sprite_evtchr_slots[slot].image_rect.x, g_sprite_evtchr_slots[slot].image_rect.y + 200);
    g_sprite_evtchr_slots[slot].owner = unit->runtime_id | SPRITE_EVTCHR_SLOT_OWNER_ATTACK_IMAGE;
    g_system_frame_pacing_suppressed = 1;
    g_sprite_evtchr_slots[image_slot].image_buffer_state = SPRITE_EVTCHR_SLOT_UPLOAD_FRAMES;
    g_sprite_evtchr_slots[slot].evtchr_load_state = 1;
}

/* Free the EVTCHR slots whose page holds the unit's attack image. One that
 * holds its SP2 image stays. */
static void evtchr_unit_release(unit_runtime_t* unit) {
    s32 i;
    s32 slot_id;

    for (i = 0; i < SPRITE_EVTCHR_SLOT_COUNT; i++) {
        slot_id = g_sprite_evtchr_slots[i].owner;
        if (slot_id != SPRITE_EVTCHR_SLOT_OWNER_EVTCHR && slot_id != SPRITE_EVTCHR_SLOT_OWNER_NONE
            && (slot_id & SPRITE_EVTCHR_SLOT_OWNER_ID_MASK) == unit->runtime_id
            && (slot_id & SPRITE_EVTCHR_SLOT_OWNER_SP2) == 0) {
            g_sprite_evtchr_slots[i].owner = SPRITE_EVTCHR_SLOT_OWNER_NONE;
            g_sprite_evtchr_slots[i].evtchr_load_state = 0;
        }
    }
}

/* Update one unit's sprite animation for the frame.
 *
 * The unit's facing relative to the camera yaw gives the quadrant and the
 * sixteenth of a turn it is seen from. A request for
 * ANIMATION_STAND shows the standing sequence of that sixteenth.
 * Any other request becomes sequence 2 * animation, + 1 seen from behind
 * (a request from 500 up is the sequence id itself). Animations below 11
 * start only on the unit's frame in four unless forced, and a request for
 * the sequence already playing is dropped. A sequence of the SEQ file's attack
 * set needs the unit's attack image in an EVTCHR slot first: a free slot and
 * a free image buffer or, failing that, slot 1 taken from the unit that
 * holds it, which is left on its rest frame. Without a request, a change of
 * sixteenth turns the standing pose or swaps the running sequence for its
 * other side; otherwise the sequence and the three weapon/effect blocks
 * count their waits down by g_system_vsync_interval.
 *
 * unit is initialized in its declaration so its copy precedes the table
 * copies, and the unread unused_10 sizes the target's frame. */
static void unit_wep_eff_update(unit_runtime_t* unit_data) {
    unit_runtime_t* unit = unit_data;
    s16 unused_10[4]; /* Unread; occupies sp+0x10 like the target. */
    /* The flip tables hold SPRITE_DISPLAY_HORIZONTAL_FLIP for the views drawn mirrored. */
    s16 flip_by_quadrant[4] = { 0, 0, 2, 2 };
    s16 back_by_quadrant[4] = { 0, 1, 1, 0 };
    s16 flip_by_direction[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0 };
    s16 stand_by_direction[16] = { 1, 2, 2, 3, 3, 4, 4, 5, 5, 4, 4, 3, 3, 2, 2, 1 };
    unit_runtime_t* other;
    sprite_unit_seq_table_t* seq;
    s32 direction;
    s16 quadrant;
    s16 sixteenth;
    s32 animation;
    s32 index;
    s32 ok;
    u16 sequence_id;
    s32 i;
    s32 j;

    if (unit->draw_enabled == 0) {
        return;
    }

    if (unit->position_copies_active != 0) {
        sprite_afterimage_shift_and_fill(unit);
    }

    direction = (g_camera_render_state.vy + unit->facing) & 0xfff;
    quadrant = direction / 1024;
    sixteenth = direction / 256;

    /* requested_animation holds the animation plus one; 0 means no request. */
    if (unit->requested_animation != 0) {
        if (unit->requested_animation - 1 == ANIMATION_STAND) {
            for (i = 0; i < 3; i++) {
                unit->sprite_blocks[i].trigger = 0;
            }

            unit->animation.sequence_id = stand_by_direction[sixteenth];
            unit->sprite_display_flags
                = (unit->sprite_display_flags & ~(SPRITE_DISPLAY_HORIZONTAL_FLIP | SPRITE_DISPLAY_VERTICAL_FLIP))
                | flip_by_direction[sixteenth];
            unit->camera_facing_quadrant = quadrant;
            unit->camera_facing_sixteenth = sixteenth;

            unit->animation.script_pos = 0;
            unit->animation.wait = 0;
            unit->animation.restart_count = 0;
            unit->seq_offset.vz = 0;
            unit->seq_offset.vy = 0;
            unit->seq_offset.vx = 0;
            unit->screen_offset.vz = 0;
            unit->screen_offset.vy = 0;
            unit->screen_offset.vx = 0;
            unit->animation.wait_bias = 0;
            unit->animation.frame_bias = 0;
            unit->animation.repeat_count = 0;
            unit->animation.flags = 0;

            unit_seq_run(unit, &unit->animation, 1);
            unit->requested_animation = 0;
            evtchr_unit_release(unit);
        } else if (unit->requested_animation >= 12 || (unit->runtime_id & 3) == g_frame_counter % 4
            || (unit->movement.force_animation_update)) {
            /* An animation below 11 waits for the unit's frame in four. */
            ok = 1;
            if ((unit->requested_animation - 1) * 2 + back_by_quadrant[quadrant] != unit->animation.sequence_id
                || unit->animation.wait == 0 || (unit->movement.force_animation_update)) {
                animation = unit->requested_animation - 1;
                if (animation < ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
                    seq = unit->animation.seq;
                    index = animation * 2 + back_by_quadrant[quadrant];
                    if (index < seq->first_sp2_sequence) {
                        if (index >= seq->first_attack_sequence) {
                            /* Keep a slot the unit already owns; otherwise load into a free slot and a free
                             * image buffer. */
                            for (i = 0; i < SPRITE_EVTCHR_SLOT_COUNT; i++) {
                                if ((g_sprite_evtchr_slots[i].owner & SPRITE_EVTCHR_SLOT_OWNER_ID_MASK)
                                    == unit->runtime_id) {
                                    break;
                                }
                            }
                            if (i == SPRITE_EVTCHR_SLOT_COUNT) {
                                for (i = 0; i < SPRITE_EVTCHR_SLOT_COUNT; i++) {
                                    if (g_sprite_evtchr_slots[i].owner == SPRITE_EVTCHR_SLOT_OWNER_NONE) {
                                        for (j = 0; j < SPRITE_EVTCHR_SLOT_COUNT; j++) {
                                            if (g_sprite_evtchr_slots[j].image_buffer_state
                                                == SPRITE_EVTCHR_SLOT_BUFFER_FREE) {
                                                evtchr_unit_load(unit, i, j);
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                                if (i == SPRITE_EVTCHR_SLOT_COUNT) {
                                    ok = 0;
                                }
                                /* With no free slot, j was never set (QUIRKS.md). */
                                if (j == SPRITE_EVTCHR_SLOT_COUNT) {
                                    ok = 0;
                                }

                                /* None free: take slot 1 unless it holds an EVTCHR image or nothing, and leave
                                 * its owner on its rest frame. */
                                if (ok == 0) {
                                    if (g_sprite_evtchr_slots[1].owner - SPRITE_EVTCHR_SLOT_OWNER_EVTCHR >= 2) {
                                        other = unit_runtime_get(
                                            g_sprite_evtchr_slots[1].owner & SPRITE_EVTCHR_SLOT_OWNER_ID_MASK);
                                        if (other != 0) {
                                            if (other->animation.sequence_id & 1) {
                                                other->animation.frame = g_animation_targeted_frame_back
                                                    [g_sprite_sheet_data[other->spritesheet_id].shp_id];
                                            } else {
                                                other->animation.frame = g_animation_targeted_frame_front
                                                    [g_sprite_sheet_data[other->spritesheet_id].shp_id];
                                            }
                                            other->seq_offset.vz = 0;
                                            other->seq_offset.vy = 0;
                                            other->seq_offset.vx = 0;
                                            sprite_unit_frame_load(other, &other->animation, other->animation.frame,
                                                other->animation.sequence_id);
                                            ok = 1;
                                            unit->animation.wait = 0;
                                            evtchr_unit_load(unit, 1, 1);
                                        }
                                    }
                                }
                            }
                        } else {
                            evtchr_unit_release(unit);
                        }
                    }
                }

                if (ok != 0) {
                    for (i = 0; i < 3; i++) {
                        unit->sprite_blocks[i].trigger = 0;
                    }

                    animation = unit->requested_animation - 1;
                    if (animation >= ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
                        unit->animation.sequence_id = unit->requested_animation - 1;
                    } else {
                        unit->animation.sequence_id = animation * 2 + back_by_quadrant[quadrant];
                    }

                    unit->sprite_display_flags = (unit->sprite_display_flags
                                                     & ~(SPRITE_DISPLAY_HORIZONTAL_FLIP | SPRITE_DISPLAY_VERTICAL_FLIP))
                        | flip_by_quadrant[quadrant];
                    unit->camera_facing_quadrant = quadrant;
                    unit->camera_facing_sixteenth = sixteenth;

                    unit->animation.script_pos = 0;
                    unit->animation.wait = 0;
                    unit->animation.restart_count = 0;
                    unit->seq_offset.vz = 0;
                    unit->seq_offset.vy = 0;
                    unit->seq_offset.vx = 0;
                    unit->screen_offset.vz = 0;
                    unit->screen_offset.vy = 0;
                    unit->screen_offset.vx = 0;
                    unit->animation.wait_bias = 0;
                    unit->animation.frame_bias = 0;
                    unit->animation.repeat_count = 0;
                    unit->animation.flags = 0;

                    unit_seq_run(unit, &unit->animation, 1);
                    unit->movement.force_animation_update = 0;
                    unit->requested_animation = 0;

                    /* A mount restarts its rider's sequence along with its own. */
                    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
                        other = unit_runtime_get(unit->mount_partner_runtime_id);
                        if (other != 0) {
                            other->animation.script_pos = 0;
                            other->animation.wait = 0;
                            unit_seq_run(other, &other->animation, 1);
                        }
                    }
                } else if (unit->animation.wait != 0) {
                    /* No slot could be had: the request stays pending while the current sequence runs on. */
                    for (i = 0; i < g_system_vsync_interval; i++) {
                        if (unit->animation.wait != 0) {
                            unit->animation.wait--;
                            if (unit->animation.wait == 0) {
                                unit_seq_run(unit, &unit->animation, 1);
                            }
                        }
                    }
                }
            } else {
                /* The requested sequence is already playing: drop the request. */
                unit->requested_animation = 0;
            }
        }
    } else if (!(unit->movement.suppress_facing_update)) {
        /* No request: a new view direction re-aims the running sequence without restarting
         * its wait. */
        if (sixteenth != unit->camera_facing_sixteenth) {
            sequence_id = unit->animation.sequence_id;
            /* Sequences 0-5 are the standing poses, chosen per sixteenth; the others keep their
             * front/back pair and switch side. */
            if (sequence_id < (ANIMATION_IDLE << 1)) {
                unit->animation.sequence_id = stand_by_direction[sixteenth];
                unit->sprite_display_flags
                    = (unit->sprite_display_flags & ~(SPRITE_DISPLAY_HORIZONTAL_FLIP | SPRITE_DISPLAY_VERTICAL_FLIP))
                    | flip_by_direction[sixteenth];
            } else {
                unit->animation.sequence_id = (sequence_id & 0xfffe) + back_by_quadrant[quadrant];
                unit->sprite_display_flags
                    = (unit->sprite_display_flags & ~(SPRITE_DISPLAY_HORIZONTAL_FLIP | SPRITE_DISPLAY_VERTICAL_FLIP))
                    | flip_by_quadrant[quadrant];
            }
            unit->camera_facing_quadrant = quadrant;
            unit->camera_facing_sixteenth = sixteenth;
            unit_seq_run(unit, &unit->animation, 0);
        } else if (unit->animation.wait != 0) {
            for (i = 0; i < g_system_vsync_interval; i++) {
                if (unit->animation.wait != 0) {
                    unit->animation.wait--;
                    if (unit->animation.wait == 0) {
                        unit_seq_run(unit, &unit->animation, 1);
                    }
                }
                animation_distortion_dispatch(unit);
            }
        }
    } else if (unit->animation.wait != 0) {
        /* Facing updates are suppressed: only count the wait down. */
        for (i = 0; i < g_system_vsync_interval; i++) {
            if (unit->animation.wait != 0) {
                unit->animation.wait--;
                if (unit->animation.wait == 0) {
                    unit_seq_run(unit, &unit->animation, 1);
                }
            }
            animation_distortion_dispatch(unit);
        }
    }

    /* Count down the three weapon/effect sprite blocks. */
    for (i = 0; i < 3; i++) {
        if (unit->sprite_blocks[i].trigger != 0 && unit->sprite_blocks[i].wait != 0) {
            for (j = 0; j < g_system_vsync_interval; j++) {
                if (unit->sprite_blocks[i].wait != 0) {
                    unit->sprite_blocks[i].wait--;
                    if (unit->sprite_blocks[i].wait == 0) {
                        weapon_seq_run(unit, &unit->sprite_blocks[i], i, 1);
                    }
                }
            }
        }
    }
}

/* Draws the unit's status bubble when the unit is on screen and has one.
 *
 * Nothing is drawn unless the camera-relative position lies within x 0x61..0x19f
 * and y -0x1f..0x10f, the ordering-table depth within 1..0x17f,
 * animation.trigger is set, the unit is not a mount carrying its rider and
 * status_bubble_active is set. */
static void bubble_display_update(unit_runtime_t* unit) {
    DVECTOR screen_coords;

    /* The camera-relative position is read unsigned (lhu) here. */
    screen_coords.vx = unit->camera_relative_x;
    screen_coords.vy = unit->camera_relative_y;
    if (screen_coords.vx < 0x61 || screen_coords.vx > 0x19f) {
        return;
    }
    if (screen_coords.vy < -0x1f || screen_coords.vy > 0x10f) {
        return;
    }
    if ((unit->otag_depth_index - 1) >= 0x17f) {
        return;
    }
    if (unit->animation.trigger == 0) {
        return;
    }
    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        return;
    }
    if (unit->status_bubble_active == 0) {
        return;
    }
    sprite_status_bubble_draw(unit, &screen_coords);
}

/* Queue a unit's sprite layers into the ordering table at its depth.
 *
 * g_animation_layer_priorities[layer_priority] lists four layers in draw order: 0 is the unit's own
 * sprite display, n > 0 is weapon/effect sprite block n - 1 (skipped while
 * its trigger is clear). A rider (mount state 1) is skipped here; a mount
 * (state 2) draws its first three body parts, then its rider's layers, then
 * the rest of its body. Unmounted units mirror their body sprite by the
 * animation state's flags. The shadow and the position copies follow.
 *
 * The rider loop issues one call per arm; cross-jumping merges them after
 * reload, but the duplicated RTL makes the mount loop large enough that
 * loop.c leaves `row * 16 + g_animation_layer_priorities` inside it, which is why the target
 * recomputes the table address each iteration and spills `row`. */
static void unit_layers_draw(unit_runtime_t* unit) {
    SVECTOR position;
    SVECTOR rider_position;
    SVECTOR zoom = { ONE, ONE, ONE };
    unit_runtime_t* rider;
    sprite_display_t* display;
    sprite_display_t* rider_display;
    s32 row;
    s32 rider_row;
    s32 i;
    s32 j;
    s32 entry;
    s16 angle;
    s16 flags;
    s16 rider_flags;

    position.vx = unit->camera_relative_x;
    position.vy = unit->camera_relative_y;
    SetGeomScreen(0x200);
    SetGeomOffset(0, 0);
    angle = 0;
    if (unit->otag_depth_index - 1 >= 0x17f) {
        return;
    }
    if (unit->numeric_display.active) {
        result_text_animate(unit, &position);
    }
    if (!((position.vx >= 0x61 && position.vx <= 0x19f) && (position.vy >= -0x1f && position.vy <= 0x10f))
        && unit->spritesheet_id != SPRITE_SHEET_ID_ALTIMA_SECOND_FORM) {
        return;
    }
    copyVector(&zoom, &g_camera_zoom);
    flags = unit->horizontal_flip_flag ^ unit->sprite_display_flags;
    row = unit->layer_priority;
    if (unit->animation.trigger == 0) {
        return;
    }
    if (unit->item_display_active) {
        sprite_item_display_draw(unit);
    }
    if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
        if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
            /* Its mount draws it. */
        } else if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
            for (i = 0; i < 4; i++) {
                entry = g_animation_layer_priorities[row][i];
                if (entry != 0) {
                    if (unit->sprite_blocks[entry - 1].trigger == 0) {
                        continue;
                    }
                    display = unit->sprite_blocks[entry - 1].display;
                    sprite_display_construct_polygon_data(display, display->part_count, 0, &position, angle, flags,
                        &zoom, gfx_battle_otag_get() + unit->otag_depth_index);
                } else {
                    display = unit->animation.display;
                    sprite_display_construct_polygon_data(
                        display, 3, 0, &position, angle, flags, &zoom, gfx_battle_otag_get() + unit->otag_depth_index);
                    rider = unit_runtime_get(unit->mount_partner_runtime_id);
                    if (rider != 0 && rider->draw_enabled != 0) {
                        rider_position.vx = rider->camera_relative_x;
                        rider_position.vy = rider->camera_relative_y;
                        rider_flags = rider->sprite_display_flags;
                        rider_row = rider->layer_priority;
                        for (j = 0; j < 4; j++) {
                            entry = g_animation_layer_priorities[rider_row][j];
                            if (entry != 0) {
                                if (rider->sprite_blocks[entry - 1].trigger == 0) {
                                    continue;
                                }
                                rider_display = rider->sprite_blocks[entry - 1].display;
                                sprite_display_construct_polygon_data(rider_display, rider_display->part_count, 0,
                                    &rider_position, angle, rider_flags, &zoom,
                                    gfx_battle_otag_get() + unit->otag_depth_index);
                            } else {
                                rider_display = rider->animation.display;
                                sprite_display_construct_polygon_data(rider_display, rider_display->part_count, 0,
                                    &rider_position, angle, rider_flags, &zoom,
                                    gfx_battle_otag_get() + unit->otag_depth_index);
                            }
                        }
                    }
                    sprite_display_construct_polygon_data(display, display->part_count, 3, &position, angle, flags,
                        &zoom, gfx_battle_otag_get() + unit->otag_depth_index);
                }
            }
            if (unit->shadow_graphic_trigger) {
                sprite_shadow_draw(unit, gfx_battle_otag_get() + unit->otag_depth_index);
            }
        }
    } else {
        for (i = 0; i < 4; i++) {
            entry = g_animation_layer_priorities[row][i];
            if (entry != 0) {
                if (unit->sprite_blocks[entry - 1].trigger == 0) {
                    continue;
                }
                display = unit->sprite_blocks[entry - 1].display;
                sprite_display_construct_polygon_data(display, display->part_count, 0, &position, angle, flags, &zoom,
                    gfx_battle_otag_get() + unit->otag_depth_index);
            } else {
                display = unit->animation.display;
                sprite_display_construct_polygon_data(display, display->part_count, 0, &position, angle,
                    flags ^ unit->animation.flags, &zoom, gfx_battle_otag_get() + unit->otag_depth_index);
            }
        }
        if (unit->shadow_graphic_trigger) {
            sprite_shadow_draw(unit, gfx_battle_otag_get() + unit->otag_depth_index);
        }
    }
    if (unit->position_copies_active) {
        sprite_afterimage_draw(unit);
    }
}

/* Project every unit record into camera space for drawing.
 *
 * camera_relative_x and _y take the projected draw position plus the unit's
 * screen offset, subtracted along an axis whose sprite is mirrored. The
 * ordering-table depth comes from a second projection of the unit's depth
 * point (the stepping variant while the unit is mid-step, offset 5 instead
 * of 1 on a tile with a depth bias). A unit standing in water takes that
 * depth at the water surface, 12 units per depth level above its feet; In
 * Water moves it up one level only on a water tile, and Float and Walk on
 * Water not at all. */
void sprite_units_project(void) {
    VECTOR transformed;
    SVECTOR screen;
    SVECTOR adjusted;
    s32 gte_flag;
    unit_runtime_t* unit;
    battle_map_tile_t* tile;
    move_effective_flags_e movement_flags;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);

    unit = g_unit_list_head;
    while (unit != 0) {
        unit_draw_coords_copy(&screen, unit);
        RotTrans(&screen, &transformed, &unit->gte_flag);

        if ((unit->sprite_display_flags & SPRITE_DISPLAY_HORIZONTAL_FLIP) != 0) {
            unit->camera_relative_x = transformed.vx - unit->screen_offset.vx;
        } else {
            unit->camera_relative_x = transformed.vx + unit->screen_offset.vx;
        }
        if ((unit->sprite_display_flags & SPRITE_DISPLAY_VERTICAL_FLIP) != 0) {
            unit->camera_relative_y = transformed.vy - unit->screen_offset.vy;
        } else {
            unit->camera_relative_y = transformed.vy + unit->screen_offset.vy;
        }

        tile = battle_map_tile_get_pointer(unit->world_coords.vx / BATTLE_MAP_TILE_SIZE,
            unit->world_coords.vz / BATTLE_MAP_TILE_SIZE, unit->map_layer);
        if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
            if ((tile->surface.depth_bias) != 0) {
                camera_get_step_depth_point_biased(&screen, &adjusted);
            } else {
                camera_get_step_depth_point_unbiased(&screen, &adjusted);
            }
        } else if ((tile->surface.depth_bias) != 0) {
            camera_get_tile_depth_point_biased(&screen, &adjusted);
        } else {
            camera_get_tile_depth_point_unbiased(&screen, &adjusted);
        }

        if (unit->stats != 0) {
            movement_flags = move_get_effective_flags(unit->stats);
            if ((movement_flags & MOVE_EFFECTIVE_IN_WATER) != 0) {
                if (tile->depth_half_height.depth != 0) {
                    adjusted.vy -= 12;
                }
            } else if ((movement_flags & MOVE_EFFECTIVE_FLOAT) == 0) {
                if ((movement_flags & MOVE_EFFECTIVE_ON_WATER) == 0) {
                    adjusted.vy -= tile->depth_half_height.depth * 12;
                }
            }
        }

        RotTrans(&adjusted, &transformed, &gte_flag);
        unit->otag_depth_index = transformed.vz / 4;
        unit = unit->previous;
    }
}

/*
 * Per-frame status-bubble and unit-graphics update.
 *
 * In the cursor, menu, move and targeting states listed below, status bubbles
 * stay hidden while the camera zooms, tilts, rotates or pans and for 0x3c
 * vertical blanks (one second) after it stops; then every unit record updates
 * its bubble. Every state then reloads the casting unit's frame parts and
 * draws every unit record's sprite layers.
 */
void sprite_units_display_update(void) {
    unit_runtime_t* unit;

    switch (g_battle_state) {
    case BATTLE_STATE_FREE_CURSOR:
    case BATTLE_STATE_HIGHLIGHT_UNITS:
    case BATTLE_STATE_OPEN_ACTION_MENUS:
    case BATTLE_STATE_IDLING_ACTION_MENUS:
    case BATTLE_STATE_AI_COMMAND:
    case BATTLE_STATE_DISPLAY_MOVE_AREA:
    case BATTLE_STATE_UNIT_MOVE:
    case BATTLE_STATE_SELECT_MOVE_TILE:
    case BATTLE_STATE_TARGETING_PROMPT:
    case BATTLE_STATE_TARGETING_RANGE:
    case BATTLE_STATE_ABILITY_PREVIEW_HANDLING:
    case BATTLE_STATE_CONFIRM_ACTION:
        if (g_camera_zoom_action == CAMERA_STEP_NONE && g_camera_tilt_action == CAMERA_STEP_NONE
            && g_camera_rotation_action == CAMERA_ROTATION_NONE
            && !(g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz)) {
            if (g_status_bubble_delay != 0) {
                g_status_bubble_delay -= g_system_vsync_interval;
                if (g_status_bubble_delay <= 0) {
                    g_status_bubble_delay = 0;
                }
            } else {
                for (unit = g_unit_list_head; unit != 0; unit = unit->previous) {
                    bubble_display_update(unit);
                }
            }
        } else {
            g_status_bubble_delay = 0x3c;
        }
        break;
    }
    frame_casting_unit_load();
    for (unit = g_unit_list_head; unit != 0; unit = unit->previous) {
        unit_layers_draw(unit);
    }
}

/* Unlink a unit record from the unit list and free its runtime slot; its spritesheet VRAM slot is
 * freed too unless another live unit still uses it. */
void unit_remove(unit_runtime_t* unit) {
    unit_runtime_t* it;
    s32 shared = 0;

    sprite_display_release_unit_stub(unit->animation.display);
    g_unit_slot_states[unit->runtime_id].in_use = 0;
    /* The 32-bit list-head pointer doubles as a sentinel record (previous is at 0x000);
     * reading the first link through that view keeps the target's addressing. */
    for (it = ((unit_runtime_t*)&g_unit_list_head)->previous; it != 0; it = it->previous) {
        if (it->spritesheet_vram_slot == unit->spritesheet_vram_slot
            && g_unit_slot_states[it->runtime_id].in_use != 0) {
            shared = 1;
        }
    }
    if (shared == 0) {
        g_sprite_sheet_slots[unit->spritesheet_vram_slot].in_use = 0;
    }
    for (it = (unit_runtime_t*)&g_unit_list_head; it != 0; it = it->previous) {
        if (it->previous == unit) {
            it->previous = unit->previous;
            return;
        }
    }
}

/* Remove every unit record. The walk reads each record's link after removing it, which works
 * because unit_remove leaves that link in place. */
void unit_all_clear(void) {
    unit_runtime_t* unit = g_unit_list_head;

    while (unit != 0) {
        unit_remove(unit);
        unit = unit->previous;
    }
}

/* Steps a unit's fade-out removal; each phase waits for the unit's palette
 * animation to end.
 *
 * FLASH starts the fade to black and moves to DARKEN. DARKEN then disables the
 * battle unit slot, removes the runtime record and returns 1; for the source unit it
 * only hides the unit. Every other path returns 0. */
static s32 fade_out_removal_update(unit_runtime_t* unit) {
    u8 phase = unit->fade_out_phase;

    if (phase == UNIT_FADE_OUT_NONE) {
        return 0;
    }
    if (g_battle_map_palette_state.clut_rows[BATTLE_MAP_CLUT_ROW_UNITS].animations[unit->runtime_id].active != 0) {
        return 0;
    }
    switch (phase) {
    case UNIT_FADE_OUT_FLASH:
        unit->fade_out_phase = UNIT_FADE_OUT_DARKEN;
        sprite_palette_apply_modulation_to_units(BATTLE_MAP_COLOR_MODE_BASE, 4, unit->runtime_id, -0x1f, -0x1f, -0x1f);
        return 0;
    case UNIT_FADE_OUT_DARKEN:
        if (unit->runtime_id != g_unit_source_runtime_id) {
            unit->fade_out_phase = UNIT_FADE_OUT_REMOVED;
            if (unit->stats != 0) {
                unit_disable(unit->stats->battle_id);
            }
            unit_remove(unit);
            return 1;
        }
        unit_runtime_hide(unit->runtime_id);
        return 0;
    }
    return 0;
}

/* Per-frame update of every unit record: its animation with the weapon and effect
 * sprites, its status bubble and its fade-out removal. */
void unit_animations_update(void) {
    unit_runtime_t* unit = g_unit_list_head;

    while (unit != 0) {
        unit_wep_eff_update(unit);
        unit_float_status_bubble_update(unit);
        fade_out_removal_update(unit);
        unit = unit->previous;
    }
}

/* Unpack a unit SEQ file: copy its first attack and SP2 sequence numbers, turn its 256 sequence
 * offsets into pointers and append the sequence data at g_sprite_seq_data_cursor. An offset of
 * -1 resolves to the start of that data. */
static void unit_seq_data_unpack(sprite_native_unit_seq_table_t* destination, const unit_seq_file_t* source) {
    const unit_seq_file_t* header;
    const u8(*entry)[4];
    u32 cursor;
    u32 index;
    u32 payload_size;
    s32 offset;

    header = source;
    cursor = (u32)g_sprite_seq_data_cursor;
    destination->header.first_attack_sequence
        = source->first_attack_sequence[0] + (source->first_attack_sequence[1] << 8);
    destination->header.first_sp2_sequence = source->first_sp2_sequence[0] + (source->first_sp2_sequence[1] << 8);
    entry = source->sequence_offsets;
    for (index = 0; index < 0x100; index++) {
        offset = entry[index][0] + (entry[index][1] << 8) + (entry[index][2] << 16) + (entry[index][3] << 24);
        if (offset == -1) {
            offset = 0;
        }
        destination->sequences[index] = (u8*)(offset + cursor);
    }

    payload_size = header->sequence_data_size[0] + (header->sequence_data_size[1] << 8);
    for (index = 0; index < payload_size; index++) {
        const u8* payload = source->sequence_data;

        g_sprite_seq_data_cursor[index] = payload[index];
    }
    g_sprite_seq_data_cursor += index;
}

/* Unpack a weapon or effect SEQ file, which has no header fields: its 256
 * sequence offsets become pointers into the sequence data, which is appended
 * at g_sprite_seq_data_cursor.
 *
 * An offset of -1 resolves to the start of that data instead of to a real
 * sequence.
 *
 * `header` and `unused_frame_pad` are matching constraints. The target keeps
 * two live copies of the record pointer (`move t2,a1` at +0x4, with `a1` still
 * used for the entry base and the payload address), which only a second
 * variable reproduces; and it reserves an 8-byte frame that no instruction
 * touches, which only a declared local aggregate reproduces. */
static void packed_offset_table_resolve(u8** entries, const packed_seq_file_t* record) {
    const u8(*entry)[4];
    u32 index;
    u32 payload_size;
    const u8* payload;
    s32 offset;
    u8 unused_frame_pad[8];
    const packed_seq_file_t* header;

    header = record;
    entry = record->entry;
    index = 0;
    do {
        offset = entry[index][0] + (entry[index][1] << 8) + (entry[index][2] << 16) + (entry[index][3] << 24);
        if (offset == -1) {
            offset = 0;
        }
        entries[index] = g_sprite_seq_data_cursor + offset;
        index += 1;
    } while (index < 0x100);

    payload_size = header->payload_size[0] + (header->payload_size[1] << 8);
    index = 0;
    if (payload_size != 0) {
        payload = record->payload;
        do {
            g_sprite_seq_data_cursor[index] = *payload;
            index += 1;
            payload += 1;
        } while (index < payload_size);
    }
    g_sprite_seq_data_cursor += index;
}

/* Unpack a unit SHP file into its two frame tables.
 *
 * The file's first 32-bit value is the offset of its second frame set, the submerged
 * frames; 8 means it has only the set that starts there and both tables get
 * its frames. A set is 0x100 frame offsets, of which the first 0xd0 become
 * pointers into the frame data (-1 maps to its start), then the frame data:
 * a 16-bit count and that many bytes, appended at
 * g_sprite_shp_frame_data_cursor.
 */
static void unit_shp_data_unpack(sprite_unit_shp_frame_tables_t* dst, const unit_shp_file_t* src) {
    u32 i;
    u32 j;
    s32 offset;
    s32 value;
    u32 count;
    const u8* data;
    s32 header;

    /* Named table indexing removes the retail cursor and skip-loop instructions. */
    data = (const u8*)src;
    header = src->submerged_frame_set_offset[0] + (src->submerged_frame_set_offset[1] << 8)
        + (src->submerged_frame_set_offset[2] << 16) + (src->submerged_frame_set_offset[3] << 24);
    dst->first_attack_frame = src->first_attack_frame[0] + (src->first_attack_frame[1] << 8);
    dst->first_sp2_frame = src->first_sp2_frame[0] + (src->first_sp2_frame[1] << 8);
    offset = offsetof(unit_shp_file_t, primary_frame_set);
    for (i = 0; i < 0xd0; i++) {
        value = data[offset] + (data[offset + 1] << 8) + (data[offset + 2] << 16) + (data[offset + 3] << 24);
        if (value == -1) {
            value = 0;
        }
        dst->frames[i] = (sprite_frame_t*)(g_sprite_shp_frame_data_cursor + value);
        if (header == 8) {
            dst->submerged_frames[i] = (sprite_frame_t*)(g_sprite_shp_frame_data_cursor + value);
        }
        offset += 4;
    }
    for (i = 0xd0; i < 0x100; i++) {
        offset += 4;
    }
    count = data[offset] + (data[offset + 1] << 8);
    offset += 2;
    for (j = 0; j < count; j++) {
        g_sprite_shp_frame_data_cursor[j] = ((const u8*)src)[offset++];
    }
    g_sprite_shp_frame_data_cursor += j;
    if (header != 8) {
        offset = header;
        for (i = 0; i < 0xd0; i++) {
            value = data[offset] + (data[offset + 1] << 8) + (data[offset + 2] << 16) + (data[offset + 3] << 24);
            if (value == -1) {
                value = 0;
            }
            dst->submerged_frames[i] = (sprite_frame_t*)(g_sprite_shp_frame_data_cursor + value);
            offset += 4;
        }
        for (i = 0xd0; i < 0x100; i++) {
            offset += 4;
        }
        count = data[offset] + (data[offset + 1] << 8);
        offset += 2;
        for (j = 0; j < count; j++) {
            g_sprite_shp_frame_data_cursor[j] = ((const u8*)src)[offset++];
        }
        g_sprite_shp_frame_data_cursor += j;
    }
}

/*
 * Unpack a weapon or effect SHP file into dst: after four unread bytes, the
 * first frame of each of 32 weapon types, 0x200 frame offsets that become
 * pointers into the frame data (-1 maps to its start), and the frame data
 * itself, a 16-bit count and that many bytes, which is appended at
 * g_sprite_shp_frame_data_cursor.
 *
 * The header loops read through `data` while the blob copy indexes `src`;
 * using one name for both loses the target's separate copy of the argument.
 */
static void shp_frames_copy(sprite_weapon_shp_t* dst, const weapon_shp_file_t* src) {
    u32 i;
    u32 j;
    s32 offset;
    s32 value;
    u32 count;
    const u8* word;
    const u8* data;

    /* Named field indexing removes the retail running-offset instructions. */
    data = (const u8*)src;
    offset = offsetof(weapon_shp_file_t, first_frame);
    for (i = 0; i < 32; i++) {
        word = data + offset;
        dst->first_frame[i] = word[0] | (word[1] << 8);
        offset += 2;
    }
    for (i = 0; i < 0x200; i++) {
        word = data + offset;
        value = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
        if (value == -1) {
            value = 0;
        }
        dst->frames[i] = (sprite_frame_t*)(g_sprite_shp_frame_data_cursor + value);
        offset += 4;
    }
    word = data + offset;
    offset += 2;
    count = word[0] + (word[1] << 8);
    for (j = 0; j < count; j++) {
        g_sprite_shp_frame_data_cursor[j] = ((const u8*)src)[offset++];
    }
    g_sprite_shp_frame_data_cursor += j;
}

/* Load a unit's SPR file into spritesheet VRAM slot `index`.
 *
 * The file's 16 palettes go to the slot's CLUT copy with the STP bit set on
 * colors 1..15 (color 0 stays transparent). The 256x256 4-bit sprite image
 * goes to VRAM x 0x100 + 64 * index, y 0x100, and the 256x32 portrait and
 * body strip to the SPRITE_SHEET_STRIP_VRAM_X/Y area (column
 * index / 8, row index % 8). The
 * compressed attack image is copied into the slot for
 * evtchr_unit_load to expand later; its fixed length
 * overruns most files (QUIRKS.md). A null file loads nothing, and `unused`
 * and `unused_palette` are not read.
 *
 * Indexed byte copies preserve the original loop scheduling. Three RECT
 * locals preserve the 0x50-byte frame and its otherwise unused x/y stores. */
static void vram_sheet_load(
    const unit_spr_file_t* source, u32 index, s32 unused, unit_runtime_t* unit, s16 unused_palette) {
    const u8* data;
    u32 column;
    u8* clut_bytes;
    u8* compressed_attack_image;
    s32 byte_index;
    RECT unused_rect;
    RECT rect;
    RECT unused_rect2;

    /* Byte stores preserve the palette loop and shared image cursor. */
    data = (const u8*)source;
    column = index;
    clut_bytes = (u8*)g_sprite_sheet_slots[unit->spritesheet_vram_slot].clut[0];
    compressed_attack_image = g_sprite_sheet_slots[unit->spritesheet_vram_slot].compressed_attack_image;
    if (data != 0) {
        for (byte_index = 0; byte_index < 0x200; byte_index++) {
            if (byte_index % 2 != 0 && byte_index % 32 != 1) {
                clut_bytes[byte_index] = data[byte_index] | 0x80;
            } else {
                clut_bytes[byte_index] = data[byte_index];
            }
        }
        data += sizeof(source->clut);
        rect.x = column * 64 + 0x100;
        rect.y = 0x100;
        LoadTPage((u32*)data, 0, 0, rect.x, 0x100, 0x100, 0x100);
        rect.x = (column / 8) * SPRITE_SHEET_STRIP_WIDTH + SPRITE_SHEET_STRIP_VRAM_X;
        rect.y = (column % 8) * SPRITE_SHEET_STRIP_HEIGHT + SPRITE_SHEET_STRIP_VRAM_Y;
        LoadTPage((u32*)(data + sizeof(source->pixels)), 0, 0, rect.x, rect.y, 0x100, 0x20);
        for (byte_index = 0; byte_index < 0x30d4; byte_index++) {
            compressed_attack_image[byte_index]
                = data[byte_index + sizeof(source->pixels) + sizeof(source->portrait_and_body_pixels)];
        }
    }
}

/* Upload WEP.SPR's weapon, trail and particle images. Keep weapon and trail
 * CLUTs in their CPU tables; the particle CLUTs use the map palette manager. */
static void wep_spr_data_load(weapon_spr_file_t* archive) {
    s32 i;
    u8* cursor = (u8*)archive->weapon_clut;

    for (i = 0; i < (s32)sizeof(archive->weapon_clut); i++) {
        ((u8*)g_sprite_weapon_clut)[i] = *cursor++;
    }
    LoadTPage((u32*)archive->weapon_pixels, 0, 0, 0x240, 0, 0x100, 0x100);
    for (i = 0, cursor = (u8*)archive->trail_clut; i < (s32)sizeof(archive->trail_clut); i++) {
        ((u8*)g_sprite_trail_clut)[i] = *cursor++;
    }
    LoadTPage((u32*)archive->trail_pixels, 0, 0, 0x200, 0, 0x100, 0x100);
    battle_map_palette_load_data(archive->particle_clut, BATTLE_MAP_CLUT_ROW_WEAPONS, 0, 1);
    LoadTPage((u32*)archive->particle_pixels, 0, 0, 0x200, 0x70, 0x100, 0x90);
}

/* Copy the first 21 OTHER.SPR palettes, setting STP on all but color zero. */
static void other_spr_palette_copy(const other_spr_file_t* archive) {
    s32 i = 0;
    s32 one = 1; /* held in a local: a literal changes the setup schedule */
    u8* dst = (u8*)&g_sprite_other_palettes;
    /* Byte transfers preserve the palette loop and STP store sequence. */
    const u8* src = (const u8*)archive->palettes;
    do {
        u8 color_byte;
        if ((i & 1) && i % 32 != one) {
            color_byte = *src++ | 0x80;
        } else {
            color_byte = *src++;
        }
        *dst++ = color_byte;
        i++;
    } while (i < (s32)sizeof(g_sprite_other_palettes));
}

/* Create a unit record on a map tile: claim a runtime slot and a spritesheet VRAM slot (0 when
 * either is exhausted), load its sheet and palette, reset its motion, display and animation state,
 * set up its weapon and effect blocks and link it at the head of the unit list. A job sheet with
 * stats uses the stats' palette; palettes past 5 become 0, and nonzero `flags` starts the unit
 * hidden. */
static unit_runtime_t* runtime_unit_init(s32 map_x, s32 map_y, s32 map_level, s16 facing, s32 spritesheet_id,
    s16 palette, s16 requested_runtime_id, unit_stats_t* stats, u32 flags, const unit_spr_file_t* data) {
    unit_runtime_t* unit;
    union {
        animation_state_t* animation;
        sprite_block_t* block;
    } cursor;
    battle_map_tile_t* tile;
    s32 runtime_id;
    s32 slot;
    u16 id;
    s32 i;
    unit_runtime_t** head;
    u8 tile_x;
    u8 tile_y;
    u8 tile_layer;

    id = spritesheet_id;
    unit = runtime_slot_claim(requested_runtime_id);
    if (unit == 0) {
        return 0;
    }
    runtime_id = unit->runtime_id;
    unit->spritesheet_id = id;
    /* slot reads the claim back through the u8 field, so a failed claim
     * (0xffff) arrives as 0xff and this test never holds (QUIRKS.md). */
    slot = unit->spritesheet_vram_slot = sprite_sheet_claim_slot(spritesheet_id);
    if (slot == 0xffff) {
        g_unit_slot_states[unit->runtime_id].in_use = 0;
        return 0;
    }

    if (id >= SPRITE_SHEET_ID_JOB_FIRST && id < SPRITE_SHEET_ID_CHOCOBO && stats != 0) {
        palette = stats->sprite_palette;
    }
    if (palette >= 6) {
        palette = 0;
    }
    unit->stored_palette = palette;

    unit->statuses_to_remove_5_6 = 0;
    unit->statuses_to_remove_1_4 = 0;
    unit->statuses_to_add_5_6 = 0;
    unit->statuses_to_add_1_4 = 0;
    unit->status_flags_5_6 = 0;
    unit->status_flags_1_4 = 0;

    vram_sheet_load(data, slot, runtime_id, unit, palette);
    tile = battle_map_tile_get_pointer(map_x, map_y, map_level);
    /* The tint runs before mount_state is reset below (QUIRKS.md). */
    sprite_palette_check_tile_status_mod(unit, tile->flags.shading, 1);
    battle_map_palette_load_data(&g_sprite_shadow_palette, BATTLE_MAP_CLUT_ROW_UNIT_SHADOWS, runtime_id, 0);
    unit->clut = GetClut(runtime_id << 4, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_UNITS);
    unit->tpage = slot + SPRITE_TPAGE_SPRITESHEET_SLOT_0;

    if (flags == 0) {
        unit->draw_enabled = 1;
        unit->animation.trigger = 1;
    } else {
        unit->draw_enabled = 0;
        unit->animation.trigger = 0;
    }

    unit->standing_on_battle_id_plus_one = 0;
    unit->stats = stats;
    unit->requested_animation = 0;
    unit->facing = facing * UNIT_FACING_QUARTER_TURN;
    unit->camera_facing_sixteenth = unit->camera_facing_quadrant = unit->attack_facing = -1;
    unit->mount_state = UNIT_MOUNT_STATE_NONE;
    unit->horizontal_flip_flag = 0;
    unit->depth_height_offset = 0;
    unit->float_bob_offset = 0;
    unit->float_bob_phase = 0;
    unit->step_phase = MOVE_STEP_PHASE_NONE;
    unit->path.count = 0;
    unit->sprite_display_flags = 0;
    unit->layer_priority = 0;
    unit->turn_action_flags = 0;
    unit->special_graphic_y_offset = 0;
    unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
    unit->distortion_phase = 0;

    tile_x = map_x;
    unit->map_x = tile_x;
    unit->movement.suppress_facing_update = 0;
    tile_y = map_y;
    unit->map_y = tile_y;
    unit->movement.suppress_palette_update = 0;
    tile_layer = map_level;
    unit->map_layer = tile_layer;
    unit->movement.alternate_short_hop = 0;
    unit->movement.floating = 0;
    unit->movement.force_animation_update = 0;
    unit->movement.destination_x = tile_x;
    unit->movement.destination_y = tile_y;
    unit->movement.destination_layer = tile_layer;
    unit_set_move_and_world_coords(unit);

    cursor.animation = &unit->animation;
    unit->velocity.vz = 0;
    unit->velocity.vy = 0;
    unit->velocity.vx = 0;
    unit->walk_speed = 0x2000;
    unit->seq_offset.vz = 0;
    unit->seq_offset.vy = 0;
    unit->seq_offset.vx = 0;
    unit->screen_offset.vz = 0;
    unit->screen_offset.vy = 0;
    unit->screen_offset.vx = 0;
    unit->event_offset.vz = 0;
    unit->event_offset.vy = 0;
    unit->event_offset.vx = 0;
    unit->fade_out_phase = UNIT_FADE_OUT_NONE;
    unit->shadow_graphic_trigger = 1;
    unit->shadow_dirty = 1;
    unit->shadow_counter = 0;

    unit->numeric_display.active = 0;
    unit->numeric_display.selector = RESULT_DISPLAY_HP_DAMAGE;
    unit->numeric_display.progress = 0;
    unit->numeric_display.displays[0] = sprite_display_init_numeric(runtime_id, 0);
    unit->numeric_display.displays[1] = sprite_display_init_numeric(runtime_id, 1);
    unit->numeric_display.displays[2] = sprite_display_init_numeric(runtime_id, 2);
    unit->numeric_display.displays[0]->scale_x = unit->numeric_display.displays[0]->scale_y
        = unit->numeric_display.displays[1]->scale_x = unit->numeric_display.displays[1]->scale_y
        = unit->numeric_display.displays[2]->scale_x = unit->numeric_display.displays[2]->scale_y = ONE;
    unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
        = unit->numeric_display.displays[2]->tpage = SPRITE_TPAGE_FRAME;
    unit->numeric_display.displays[0]->clut = unit->numeric_display.displays[1]->clut
        = unit->numeric_display.displays[2]->clut
        = unit->clut + 0x100; /* the unit's CLUT of BATTLE_MAP_CLUT_ROW_ACTION_RESULTS */
    unit->numeric_display.displays[0]->part_count = unit->numeric_display.displays[1]->part_count
        = unit->numeric_display.displays[2]->part_count = 1;

    unit->status_bubble_active = 0;
    unit->status_bubble_id = 0;
    unit->status_bubble_timer = 0;
    unit->status_bubble_display = sprite_display_init_status_bubble(runtime_id);
    unit->item_display_active = 0;
    unit->item_display = sprite_display_init_item(runtime_id);
    unit->position_copies_active = 0;

    unit->animation.display = sprite_display_init_unit(runtime_id);
    cursor.animation->graphic_type = SPRITE_GRAPHIC_UNIT;
    unit->animation.shp = unit->animation.saved_shp = sprite_sheet_get_unit_shp(id);
    unit->animation.seq = unit->animation.saved_seq = sprite_sheet_get_unit_seq(id);
    cursor.animation->wait = 1;
    cursor.animation->restart_count = 0;
    cursor.animation->script_pos = 0;
    cursor.animation->sequence_id = ANIMATION_IDLE << 1;
    cursor.animation->frame = 0;
    cursor.animation->flags = 0;
    cursor.animation->wait_bias = 0;
    cursor.animation->frame_bias = 0;
    cursor.animation->repeat_count = 0;
    unit->animation.display->scale_x = ONE;
    unit->animation.display->scale_y = ONE;
    unit->animation.display->rotation = 0;
    animation_status_set(unit);
    unit_seq_run(unit, cursor.animation, 1);

    /* Block 0 draws the weapon sheet, blocks 1 and 2 the effect sheet. */
    for (i = 0; i < 3; i++) {
        cursor.block = &unit->sprite_blocks[i];
        cursor.block->display = sprite_display_init_weapon(runtime_id, i);
        if (i == 0) {
            cursor.block->graphic_type = SPRITE_GRAPHIC_WEAPON;
            cursor.block->shp = sprite_sheet_get_weapon_shp(id);
            cursor.block->seq = sprite_sheet_get_weapon_seq(id);
        } else {
            cursor.block->graphic_type = SPRITE_GRAPHIC_EFFECT;
            cursor.block->shp = sprite_sheet_get_effect_shp(id);
            cursor.block->seq = sprite_sheet_get_effect_seq(id);
        }
        cursor.block->trigger = 0;
        cursor.block->wait = 1;
        cursor.block->script_pos = 0;
        cursor.block->sequence_id = 0;
        cursor.block->frame = 0;
        cursor.block->flags = 0;
        cursor.block->wait_bias = 0;
        cursor.block->frame_bias = 0;
        cursor.block->repeat_count = 0;
        cursor.block->display->scale_x = ONE;
        cursor.block->display->scale_y = ONE;
        cursor.block->display->rotation = 0;
        /* No id is both below VELIUS and from BOY up, so this never runs and the
         * blocks wait for their first sequence command. */
        if ((s16)id < SPRITE_SHEET_ID_CHOCOBO && (s16)id < SPRITE_SHEET_ID_VELIUS && (s16)id >= SPRITE_SHEET_ID_BOY
            && (s16)id != SPRITE_SHEET_ID_DAMI) {
            weapon_seq_run(unit, &unit->sprite_blocks[i], i, 1);
        }
    }

    head = &g_unit_list_head;
    unit->previous = *head;
    *head = unit;
    sprite_thrown_item_palette_set(unit->stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON], unit);
    status_init_display_flags(unit->stats->battle_id);
    /* Settle the new unit's display and pose without landing sounds or effects. */
    unit->movement.suppress_sfx_and_landing_effects = 1;
    unit_runtime_display_update(unit->runtime_id);
    animation_status_set(unit);
    unit->movement.suppress_sfx_and_landing_effects = 0;
    return unit;
}

/* Reset the unit graphics for a new battle: empty the unit list, free the
 * runtime slots, the nine spritesheet slots and both EVTCHR slots (whose
 * image rectangles are 64x200 at 0x100,0 and 0x140,0), rewind the SEQ and SHP
 * data cursors and restart the shared-file loader.
 *
 * The two clearing loops walk their arrays by byte offset. A struct-subscript
 * walk leaves GCC with both the index and the scaled address as induction
 * variables, adding an `addiu`/`li` pair to each loop; the byte offset is what
 * reproduces the target's single `bgez` on the address offset. */
void sprite_load_state_reset(void) {
    s32 i;
    s32 offset;

    g_unit_list_head = 0;
    for (offset = (UNIT_RUNTIME_SLOT_COUNT - 1) * sizeof(unit_slot_state_t); offset >= 0;
        offset -= sizeof(unit_slot_state_t)) {
        ((unit_slot_state_t*)((u8*)g_unit_slot_states + offset))->in_use = 0;
    }
    for (offset = (SPRITE_SHEET_SLOT_COUNT - 1) * sizeof(sprite_sheet_slot_t); offset >= 0;
        offset -= sizeof(sprite_sheet_slot_t)) {
        ((sprite_sheet_slot_t*)((u8*)g_sprite_sheet_slots + offset))->in_use = 0;
    }
    for (i = 0; i < SPRITE_EVTCHR_SLOT_COUNT; i++) {
        g_sprite_evtchr_slots[i].owner = SPRITE_EVTCHR_SLOT_OWNER_NONE;
        g_sprite_evtchr_slots[i].image_buffer_state = SPRITE_EVTCHR_SLOT_BUFFER_FREE;
        g_sprite_evtchr_slots[i].evtchr_load_state = 0;
    }
    setRECT(&g_sprite_evtchr_slots[0].image_rect, 0x100, 0, 0x40, 0xc8);
    setRECT(&g_sprite_evtchr_slots[1].image_rect, 0x140, 0, 0x40, 0xc8);
    g_sprite_seq_data_cursor = g_sprite_seq_data_buffer;
    g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
    g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
    g_sprite_load_state.index = 0;
    g_sprite_load_state.buffer = 0;
    g_sprite_shp_frame_data_cursor = g_sprite_shp_frame_data_buffer;
}

/* Advances the unit SHP/SEQ loader by one step and returns the file kind index.
 *
 * g_sprite_load_state.file_kind selects the file kind (sprite SHP/SEQ, WEP SHP/SEQ, EFF SHP/SEQ,
 * WEP.SPR, OTHER.SPR), g_sprite_load_state.phase the phase (0 allocate and start the read,
 * 1 wait, 2 copy into the RAM tables, 3 free and advance) and g_sprite_load_state.index the
 * file index within the kind.
 *
 * The target keeps several of these globals' addresses in registers; only a
 * single-assignment pointer local reproduces that (a plain `g++` folds %lo into
 * each access), and a block-scoped one per case also lets the scheduler hoist
 * the g_sprite_load_state.file_kind load above the counter store, as the target does. */
s32 sprite_shp_seq_load_update(void) {
    s32* phase;
    s32* index;
    void** slot;
    void* buffer;

    phase = &g_sprite_load_state.phase;
    switch (*phase) {
    case SPRITE_LOAD_PHASE_START_READ:
        switch (g_sprite_load_state.file_kind) {
        case SPRITE_LOAD_SHARED_SPRITE_SHP:
            index = &g_sprite_load_state.index;
            /* A failed allocation or read leaves the phase unchanged, so the next step retries. */
            buffer = heap_alloc(g_shp_files[*index].size);
            g_sprite_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (file_bytes_read_request(g_shp_files[*index].sector, g_shp_files[*index].size, buffer) == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_block_release(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_SHARED_SPRITE_SEQ:
            index = &g_sprite_load_state.index;
            buffer = heap_alloc(g_seq_files[*index].size);
            g_sprite_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (file_bytes_read_request(g_seq_files[*index].sector, g_seq_files[*index].size, buffer) == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_block_release(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_SHARED_WEAPON_SHP:
            index = &g_sprite_load_state.index;
            buffer = heap_alloc(g_weapon_shp_files[*index].size);
            g_sprite_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (file_bytes_read_request(g_weapon_shp_files[*index].sector, g_weapon_shp_files[*index].size, buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_block_release(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_SHARED_WEAPON_SEQ:
            index = &g_sprite_load_state.index;
            buffer = heap_alloc(g_weapon_seq_files[*index].size);
            g_sprite_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (file_bytes_read_request(g_weapon_seq_files[*index].sector, g_weapon_seq_files[*index].size, buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_block_release(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_SHARED_EFFECT_SHP:
            index = &g_sprite_load_state.index;
            buffer = heap_alloc(g_effect_shp_files[*index].size);
            g_sprite_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (file_bytes_read_request(g_effect_shp_files[*index].sector, g_effect_shp_files[*index].size, buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_block_release(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_SHARED_EFFECT_SEQ:
            index = &g_sprite_load_state.index;
            buffer = heap_alloc(g_effect_seq_files[*index].size);
            g_sprite_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (file_bytes_read_request(g_effect_seq_files[*index].sector, g_effect_seq_files[*index].size, buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_block_release(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_SHARED_WEAPON_SPR:
            *(slot = &g_sprite_load_state.buffer) = heap_alloc(0x15000);
            if (*slot == 0) {
                break;
            }
            if (file_bytes_read_request(DISC_SECTOR_BATTLE_WEP_SPR, 0x15000, *slot) == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_block_release(*slot);
            }
            break;
        case SPRITE_LOAD_SHARED_OTHER_SPR:
            *(slot = &g_sprite_load_state.buffer) = heap_alloc(0x8800);
            if (*slot == 0) {
                break;
            }
            if (file_bytes_read_request(DISC_SECTOR_BATTLE_OTHER_SPR, 0x8800, *slot) == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_block_release(*slot);
            }
            break;
        }
        break;
    case SPRITE_LOAD_PHASE_WAIT:
        if (file_load_state_get() != 0) {
            break;
        }
        (*phase)++;
        break;
    case SPRITE_LOAD_PHASE_INSTALL:
        switch (g_sprite_load_state.file_kind) {
        case SPRITE_LOAD_SHARED_SPRITE_SHP:
            unit_shp_data_unpack(
                &g_sprite_unit_shp_frame_tables[g_sprite_load_state.index], g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_SPRITE_SEQ:
            unit_seq_data_unpack(&g_sprite_unit_seq_tables[g_sprite_load_state.index], g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_WEAPON_SHP:
            shp_frames_copy(&g_sprite_weapon_graphics[g_sprite_load_state.index].shp, g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_WEAPON_SEQ:
            packed_offset_table_resolve(
                g_sprite_weapon_graphics[g_sprite_load_state.index].seq, g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_EFFECT_SHP:
            shp_frames_copy(&g_sprite_effect_graphics[g_sprite_load_state.index].shp, g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_EFFECT_SEQ:
            packed_offset_table_resolve(
                g_sprite_effect_graphics[g_sprite_load_state.index].seq, g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_WEAPON_SPR:
            wep_spr_data_load(g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_OTHER_SPR:
            other_spr_palette_copy(g_sprite_load_state.buffer);
            break;
        }
        {
            s32* next_phase = &g_sprite_load_state.phase;

            (*next_phase)++;
        }
        break;
    case SPRITE_LOAD_PHASE_FINISH:
        heap_block_release(g_sprite_load_state.buffer);
        *phase = SPRITE_LOAD_PHASE_START_READ;
        /* Five sprite SHP and SEQ files, two weapon, one effect, then each SPR file once. */
        switch (g_sprite_load_state.file_kind) {
        case SPRITE_LOAD_SHARED_SPRITE_SHP: {
            s32* counter = &g_sprite_load_state.index;

            if (++*counter >= 5) {
                *counter = 0;
                g_sprite_load_state.file_kind++;
            }
            break;
        }
        case SPRITE_LOAD_SHARED_SPRITE_SEQ: {
            s32* counter = &g_sprite_load_state.index;

            if (++*counter >= 5) {
                *counter = 0;
                g_sprite_load_state.file_kind++;
            }
            break;
        }
        case SPRITE_LOAD_SHARED_WEAPON_SHP:
        case SPRITE_LOAD_SHARED_WEAPON_SEQ: {
            s32* counter = &g_sprite_load_state.index;

            if (++*counter >= 2) {
                *counter = 0;
                g_sprite_load_state.file_kind++;
            }
            break;
        }
        case SPRITE_LOAD_SHARED_EFFECT_SHP:
        case SPRITE_LOAD_SHARED_EFFECT_SEQ: {
            s32* counter = &g_sprite_load_state.index;

            if (++*counter >= 1) {
                *counter = 0;
                g_sprite_load_state.file_kind++;
            }
            break;
        }
        case SPRITE_LOAD_SHARED_WEAPON_SPR:
        case SPRITE_LOAD_SHARED_OTHER_SPR: {
            s32* kind = &g_sprite_load_state.file_kind;

            g_sprite_load_state.index = 0;
            (*kind)++;
            break;
        }
        }
        break;
    }

    return g_sprite_load_state.file_kind;
}

/* Run the per-frame unit update three times; the queued graphics loader calls it right after it
 * creates a unit. */
static void initial_frames_run(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        unit_animations_update();
    }
}

/* Rewinds the unit graphics loader (g_sprite_load_state) to its
 * first file kind, the start-read phase, index 0 and no buffer. The argument is
 * not read. */
void sprite_load_state_clear(s32 unused_map_id) {
    g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
    g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
    g_sprite_load_state.index = 0;
    g_sprite_load_state.buffer = 0;
}

/* Builds the deployed units' data when a map starts and rewinds the unit graphics
 * loader.
 *
 * A battle whose deployment screen is skipped places the units from
 * g_deploy_deployed_unit_map_coordinates; otherwise the coordinate slots of the
 * deployment screen are used, and in the debug battle style the red team is set
 * up from them first. */
void deploy_sprite_load_init(void) {
    g_unit_graphics_load_queue.count = 0;
    if (script_variable_get(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) == 0) {
        if (g_deploy_skipped != 0) {
            deploy_build_units(g_deploy_deployed_unit_map_coordinates, 0);
            g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
            g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
            g_sprite_load_state.index = 0;
            g_sprite_load_state.buffer = 0;
            return;
        }
    } else {
        deploy_init_units_for_debug_red_team(deploy_get_coordinate_slots(), 0);
    }
    deploy_build_units(deploy_get_coordinate_slots(), 0);
    g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
    g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
    g_sprite_load_state.index = 0;
    g_sprite_load_state.buffer = 0;
}

/* Steps the per-unit graphics loader for the queued load descriptors.
 *
 * g_sprite_load_state holds the file kind (0 SHP, 1 SEQ, 2 SPR), the
 * phase (0 read, 1 wait, 2 install, 3 finish), the descriptor index and the
 * heap buffer. Shared SHP/SEQ ids or the last requested id skip the read, as
 * does an already loaded spritesheet. Returns 0 once every
 * descriptor is done, otherwise 1 inside a step and 2 after a step boundary. */
s32 sprite_queued_load_update(void) {
    s32 result = 1;
    s32 id;

    switch (g_sprite_load_state.phase) {
    case SPRITE_LOAD_PHASE_START_READ:
        switch (g_sprite_load_state.file_kind) {
        case SPRITE_LOAD_QUEUED_SHP:
            id = g_sprite_sheet_data[g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id]
                     .shp_id;
            /* SHP ids up to OTHER are the shared files the startup loader keeps resident. */
            if (id <= SPRITE_SHP_OTHER || id == g_sprite_last_requested_shp_id) {
                result = 2;
                g_sprite_load_state.buffer = 0;
                g_sprite_load_state.phase = SPRITE_LOAD_PHASE_FINISH;
                break;
            }

            g_sprite_last_requested_shp_id = id;
            g_sprite_load_state.buffer = heap_alloc(g_shp_files[id].size);
            if (g_sprite_load_state.buffer == 0) {
                break;
            }
            if (file_bytes_read_request(g_shp_files[id].sector, g_shp_files[id].size, g_sprite_load_state.buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_block_release(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_QUEUED_SEQ:
            id = g_sprite_sheet_data[g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id]
                     .seq_id;
            /* SEQ ids up to OTHER are likewise resident. */
            if (id < SPRITE_SEQ_RUKA || id == g_sprite_last_requested_seq_id) {
                result = 2;
                g_sprite_load_state.buffer = 0;
                g_sprite_load_state.phase = SPRITE_LOAD_PHASE_FINISH;
                break;
            }

            g_sprite_last_requested_seq_id = id;
            g_sprite_load_state.buffer = heap_alloc(g_seq_files[id].size);
            if (g_sprite_load_state.buffer == 0) {
                break;
            }
            if (file_bytes_read_request(g_seq_files[id].sector, g_seq_files[id].size, g_sprite_load_state.buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_block_release(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_QUEUED_SPR:
            /* Ids from 0xa0 up load Ramza's chapter 1 sheet instead; id 0x9f also lies past the
             * 159-entry table but slips through (QUIRKS.md). */
            if (g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id >= 0xa0) {
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id
                    = SPRITE_SHEET_ID_RAMZA_CHAPTER_1;
            }

            /* A sheet already in VRAM skips the read but still goes to install, which creates the unit. */
            if (sprite_sheet_is_loaded(g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id)
                != 0) {
                g_sprite_load_state.buffer = 0;
                g_sprite_load_state.phase = SPRITE_LOAD_PHASE_INSTALL;
                result = 2;
                break;
            }

            g_sprite_load_state.buffer = heap_alloc(
                g_sheet_files[g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id].size);
            if (g_sprite_load_state.buffer == 0) {
                break;
            }
            if (file_bytes_read_request(
                    g_sheet_files[g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id]
                        .sector,
                    g_sheet_files[g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id]
                        .size,
                    g_sprite_load_state.buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_block_release(g_sprite_load_state.buffer);
            }
            break;
        }
        break;
    case SPRITE_LOAD_PHASE_WAIT:
        if (!file_load_state_get()) {
            g_sprite_load_state.phase++;
        }
        break;
    case SPRITE_LOAD_PHASE_INSTALL:
        switch (g_sprite_load_state.file_kind) {
        case SPRITE_LOAD_QUEUED_SHP:
            /* Queued SHP and SEQ data share one boss table each, so each load replaces the last. */
            g_sprite_shp_frame_data_cursor = g_sprite_fallback_shp_frame_data;
            unit_shp_data_unpack(&g_sprite_boss_shp_frame_tables, g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_QUEUED_SEQ:
            g_sprite_seq_data_cursor = g_sprite_fallback_seq_data;
            unit_seq_data_unpack(&g_sprite_boss_seq_tables, g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_QUEUED_SPR:
            runtime_unit_init(g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].map_x,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].map_y,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].map_level,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].facing,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].palette_id,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].runtime_id,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].stats,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].flags, g_sprite_load_state.buffer);
            initial_frames_run();
            break;
        }
        g_sprite_load_state.phase++;
        break;
    case SPRITE_LOAD_PHASE_FINISH:
        if (g_sprite_load_state.buffer != 0) {
            heap_block_release(g_sprite_load_state.buffer);
        }
        g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
        switch (g_sprite_load_state.file_kind) {
        case SPRITE_LOAD_QUEUED_SHP:
        case SPRITE_LOAD_QUEUED_SEQ:
            g_sprite_load_state.file_kind++;
            break;
        case SPRITE_LOAD_QUEUED_SPR:
            g_sprite_load_state.file_kind = SPRITE_LOAD_QUEUED_SHP;
            g_sprite_load_state.index++;
            break;
        }
        result = 2;
        break;
    }

    return (g_sprite_load_state.index < g_unit_graphics_load_queue.count) ? result : 0;
}
