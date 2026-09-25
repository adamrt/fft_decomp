#ifndef FFT_BATTLE_GFX_H
#define FFT_BATTLE_GFX_H

#include "fft/battle_file.h"
#include "fft/menu_types.h"
#include "psx/gpu.h"
#include "psx/types.h"

extern battle_file_extent_t g_battle_gfx_sprite_seq_files[];
extern battle_file_extent_t g_battle_gfx_sprite_shp_files[];
extern battle_file_extent_t g_battle_gfx_weapon_seq_files[];
extern battle_file_extent_t g_battle_gfx_weapon_shp_files[];
extern battle_file_extent_t g_battle_gfx_effect_seq_files[];
extern battle_file_extent_t g_battle_gfx_effect_shp_files[];
extern battle_file_extent_t g_battle_gfx_spritesheet_files[];
extern battle_file_extent_t g_battle_gfx_sp2_files[];
extern battle_file_extent_t g_battle_gfx_worker8_sp2_files[];

typedef struct battle_gfx_sprite_part_display_data {
    s8 x_shift;
    s8 y_shift;
    u8 width;
    u8 height;
    u8 u;
    u8 v;
    u8 flags;
} battle_gfx_sprite_part_display_data_t;

typedef char
    battle_sprite_part_display_data_size_must_be_7[(sizeof(battle_gfx_sprite_part_display_data_t) == 7) ? 1 : -1];

/*
 * Header of a 0x440-byte sprite display slot (unit, weapon, item, numeric and
 * status-bubble displays all share it).  The battle_gfx_sprite_part_display_data_t records
 * start at 0x0e; battle_initialize_*_sprite_display fill in the header.
 */
typedef struct battle_gfx_sprite_display_data {
    u8 red;             /* 0x00 */
    u8 green;           /* 0x01 */
    u8 blue;            /* 0x02 */
    u8 part_count;      /* 0x03; graphics count passed to battle_gfx_construct_polygon_data_for_units */
    s16 spritesheet_id; /* 0x04; 0x1e = ITEM.BIN, 0x1f = FRAME.BIN */
    u16 clut;           /* 0x06 */
    s16 scale_x;        /* 0x08; 1.0 = 0x1000 */
    s16 scale_y;        /* 0x0a; 1.0 = 0x1000 */
    s16 y_rotation;     /* 0x0c; SEQ 0xe5 saves it, 0xdf clears it; the frame header selects it */
    battle_gfx_sprite_part_display_data_t parts[1]; /* 0x0e; count depends on the display */
} battle_gfx_sprite_display_data_t;

/* Render view of the same sprite display header. The first two colour bytes
 * are read as one halfword, and texture/scale words as unsigned halfwords. */
typedef struct battle_gfx_sprite_display {
    u16 rg;
    u8 b;
    u8 unknown_03;
    u16 tpage;
    u16 clut;
    u16 scale_x;
    u16 scale_y;
    u16 angle;
    battle_gfx_sprite_part_display_data_t parts[1];
} battle_gfx_sprite_display_t;

/*
 * This is the HEADER only, not a whole slot: 0x0e + one 7-byte part record,
 * padded to the s16 alignment.  The 0x440 per-unit stride the callers use is
 * NOT a multiple of it (0x440 = 49 * 0x16 + 10), so the display symbols cannot
 * be declared as arrays of this type -- &sym[i * 0x440] would rescale.  The
 * g_battle_gfx_*_sprite_display_data symbols are strided views into one
 * 0x440-byte per-unit block, spaced 0x16 apart at unit 0 (0x800b76d6,
 * 0x800b76ec, 0x800b7702, 0x800b7718, 0x800b772e), which is what pins this
 * size independently of the C layout.
 */
typedef char
    battle_gfx_sprite_display_data_size_must_be_0x16[(sizeof(battle_gfx_sprite_display_data_t) == 0x16) ? 1 : -1];

/* SEQ animation control opcodes, read after a BATTLE_SEQ_OP_PREFIX byte by
 * battle_gfx_run_unit_seq_script and battle_gfx_run_wep_eff_seq_script.
 * UNKNOWN_XX commands have no known effect beyond their operand skip. For
 * 0xf0 (MoveUnitRL) the unit interpreter calls battle_unit_shift_right_left. */
typedef enum battle_seq_opcode {
    BATTLE_SEQ_OP_PREFIX = 0xff,
    BATTLE_SEQ_OP_UNKNOWN_BE = 0xbe,
    BATTLE_SEQ_OP_UNKNOWN_BF = 0xbf,
    BATTLE_SEQ_OP_WAIT_FOR_DISTORT = 0xc0,
    BATTLE_SEQ_OP_QUEUE_DISTORT_ANIM = 0xc1,
    BATTLE_SEQ_OP_UNKNOWN_C2 = 0xc2,
    BATTLE_SEQ_OP_UNLOAD_MF_ITEM = 0xc3,
    BATTLE_SEQ_OP_MF_ITEM_POS = 0xc4,
    BATTLE_SEQ_OP_LOAD_MF_ITEM = 0xc5,
    BATTLE_SEQ_OP_WAIT_FOR_INPUT = 0xc6,
    BATTLE_SEQ_OP_UNKNOWN_C7 = 0xc7,
    BATTLE_SEQ_OP_UNKNOWN_C8 = 0xc8,
    BATTLE_SEQ_OP_UNKNOWN_C9 = 0xc9,
    BATTLE_SEQ_OP_UNKNOWN_CA = 0xca,
    BATTLE_SEQ_OP_MOVE_UP_2 = 0xcb,
    BATTLE_SEQ_OP_MOVE_UP_1 = 0xcc,
    BATTLE_SEQ_OP_MOVE_BACKWARD_2 = 0xcd,
    BATTLE_SEQ_OP_MOVE_BACKWARD_1 = 0xce,
    BATTLE_SEQ_OP_MOVE_DOWN_2 = 0xcf,
    BATTLE_SEQ_OP_MOVE_DOWN_1 = 0xd0,
    BATTLE_SEQ_OP_MOVE_FORWARD_2 = 0xd1,
    BATTLE_SEQ_OP_MOVE_FORWARD_1 = 0xd2,
    BATTLE_SEQ_OP_WEAPON_SHEATHE_CHECK_1 = 0xd3,
    BATTLE_SEQ_OP_PLAY_ATTACK_SOUND = 0xd4,
    BATTLE_SEQ_OP_INCREMENT_LOOP = 0xd5,
    BATTLE_SEQ_OP_WEAPON_SHEATHE_CHECK_2 = 0xd6,
    BATTLE_SEQ_OP_UNKNOWN_D7 = 0xd7,
    BATTLE_SEQ_OP_SET_FRAME_OFFSET = 0xd8,
    BATTLE_SEQ_OP_QUEUE_THROW_ANIMATION = 0xd9,
    BATTLE_SEQ_OP_RETURN_ERROR_FINISH_ANIM = 0xda,
    BATTLE_SEQ_OP_SET_SLOWDOWN = 0xdb,
    BATTLE_SEQ_OP_RELOAD_ANIMATION = 0xdc,
    BATTLE_SEQ_OP_OVERRIDE_ANIMATION = 0xdd,
    BATTLE_SEQ_OP_POST_GENERIC_ATTACK = 0xde,
    BATTLE_SEQ_OP_SET_Y_ROTATION_0 = 0xdf,
    BATTLE_SEQ_OP_CLEAR_SHADOW = 0xe0,
    BATTLE_SEQ_OP_SET_SHADOW = 0xe1,
    BATTLE_SEQ_OP_SET_LAYER_PRIORITY = 0xe2,
    BATTLE_SEQ_OP_UNKNOWN_E3 = 0xe3,
    BATTLE_SEQ_OP_UNKNOWN_E4 = 0xe4,
    BATTLE_SEQ_OP_SAVE_Y_SPIN = 0xe5,
    BATTLE_SEQ_OP_UNKNOWN_E6 = 0xe6,
    BATTLE_SEQ_OP_UNKNOWN_E7 = 0xe7,
    BATTLE_SEQ_OP_UNKNOWN_E8 = 0xe8,
    BATTLE_SEQ_OP_UNKNOWN_E9 = 0xe9,
    BATTLE_SEQ_OP_UNKNOWN_EA = 0xea,
    BATTLE_SEQ_OP_FLIP_VERTICAL = 0xeb,
    BATTLE_SEQ_OP_FLIP_HORIZONTAL = 0xec,
    BATTLE_SEQ_OP_UNKNOWN_ED = 0xed,
    BATTLE_SEQ_OP_MOVE_UNIT_FB = 0xee,
    BATTLE_SEQ_OP_MOVE_UNIT_DU = 0xef,
    BATTLE_SEQ_OP_MOVE_UNIT_RL = 0xf0,
    BATTLE_SEQ_OP_UNKNOWN_F1 = 0xf1,
    BATTLE_SEQ_OP_QUEUE_SPRITE_ANIM = 0xf2,
    BATTLE_SEQ_OP_UNKNOWN_F3 = 0xf3,
    BATTLE_SEQ_OP_UNKNOWN_F4 = 0xf4,
    BATTLE_SEQ_OP_UNKNOWN_F5 = 0xf5,
    BATTLE_SEQ_OP_PLAY_SOUND = 0xf6,
    BATTLE_SEQ_OP_UNKNOWN_F7 = 0xf7,
    BATTLE_SEQ_OP_UNKNOWN_F8 = 0xf8,
    BATTLE_SEQ_OP_UNKNOWN_F9 = 0xf9,
    BATTLE_SEQ_OP_MOVE_UNIT = 0xfa,
    BATTLE_SEQ_OP_UNKNOWN_FB = 0xfb,
    BATTLE_SEQ_OP_WAIT = 0xfc,
    BATTLE_SEQ_OP_HOLD_WEAPON = 0xfd,
    BATTLE_SEQ_OP_END_ANIMATION = 0xfe,
    BATTLE_SEQ_OP_PAUSE_ANIMATION = 0xff,
} battle_seq_opcode_e;

/* Sprite lookup result filled in by battle_gfx_get_unit_sprite_frame_and_vram_ids. */
typedef struct {
    s16 vram_palette_id;     /* 0x00 */
    s16 vram_spritesheet_id; /* 0x02 */
    s16 graphic_height;      /* 0x04 */
    s16 graphic_y_offset;    /* 0x06 */
} battle_unit_sprite_query_t;

/* First SHP-style section of a VRAM cache slot: two header words, frame
 * pointers and the frame data they point into (battle_gfx_unpack_evtchr_file_to_vram_slot relocates
 * the file's frame offsets into these pointers). */
typedef struct battle_gfx_shp0 {
    s32 header[2];    /* 0x000 */
    u8* frames[0x40]; /* 0x008 */
    u8 data[0x400];   /* 0x108 */
} battle_gfx_shp0_t;

/* Second section, indexed from frame 0xd2 (battle_gfx_select_unit_shp_frame). Its frame
 * data is stored twice, once for each frame table; deep-water units use the
 * submerged half. */
typedef struct battle_gfx_shp1 {
    s32 header[2];              /* 0x000 */
    u8* frames[0xd0];           /* 0x008 */
    u8* submerged_frames[0xd0]; /* 0x348 */
    u8 data[0x1e0];             /* 0x688 */
    u8 submerged_data[0x1e0];   /* 0x868 */
} battle_gfx_shp1_t;

/*
 * Two of these records back the battle EVTCHR/SP2 VRAM cache.  The event
 * SaveEVTCHR and LoadEVTCHR commands establish the header fields and the
 * 0x6400-byte image payload; every target access uses a 0x7564-byte stride.
 */
typedef struct battle_gfx_vram_slot {
    u32 owner;              /* 0x0000; 0xff free, 0xfe saved */
    u32 evtchr_load_marker; /* 0x0004 */
    u32 evtchr_load_state;  /* 0x0008 */
    RECT image_rect;        /* 0x000c */
    battle_gfx_shp0_t shp0; /* 0x0014 */
    battle_gfx_shp1_t shp1; /* 0x051c */
    u16 cluts[16][16];      /* 0x0f64; filled by battle_gfx_set_evtchr_palette_by_misc_id */
    u8 image_data[0x6400];  /* 0x1164 */
} battle_gfx_vram_slot_t;

typedef char battle_vram_slot_size_must_be_0x7564[(sizeof(battle_gfx_vram_slot_t) == 0x7564) ? 1 : -1];

/* 0x32d6-byte spritesheet VRAM slot records at 0x800c7ce8 (nine slots);
 * g_battle_gfx_spritesheet_ids_by_vram_slot aliases the first record's byte 1. */
typedef struct battle_gfx_spritesheet_slot {
    u8 in_use;             /* 0x00; cleared when no live unit uses the slot */
    u8 spritesheet_id;     /* 0x01 */
    u8 _unknown02[0x30d4]; /* 0x02; image data copied from SPR data +0x9200 by 0x80087704 */
    u8 palettes[16][32];   /* 0x30d6; 16 CLUTs with the STP bit set (0x80087704) */
} battle_gfx_spritesheet_slot_t;

typedef char battle_spritesheet_slot_size_must_be_0x32d6[(sizeof(battle_gfx_spritesheet_slot_t) == 0x32d6) ? 1 : -1];

/* Four-byte lookup record for the 0x9f ordinary unit spritesheets. */
typedef struct battle_gfx_spritesheet_data {
    u8 shp_id;
    u8 seq_id;
    u8 flying_flag;
    u8 graphic_height;
} battle_gfx_spritesheet_data_t;

typedef char battle_gfx_spritesheet_data_size_must_be_4[(sizeof(battle_gfx_spritesheet_data_t) == 4) ? 1 : -1];

/* Unit SHP frame tables filled by battle_gfx_unpack_unit_shp_data (0x800873bc):
 * SHP header bytes 4..5 and 6..7, then two 0xd0-entry frame pointer tables.
 * The second table repeats the first when the resource carries a single
 * frame set (header 8). The frame starts are the first frame loaded from the
 * second half of the sheet and the first frame loaded from the SP2 file. */
typedef struct battle_gfx_unit_shp_frame_tables {
    s32 attack_frame_start;
    s32 sp2_frame_start;
    u8* primary[0xd0];   /* 0x008 */
    u8* secondary[0xd0]; /* 0x348 */
} battle_gfx_unit_shp_frame_tables_t;

enum { BATTLE_SPRITESHEET_ID_COUNT = 0x9f };

/* VRAM load / screen placement point used by battle_gfx_init_image_loading. */
typedef struct battle_image_location {
    s16 x;
    u16 y;
} battle_image_location_t;

struct battle_unit_misc_data;
struct battle_stats;

/* Queued unit graphics load (g_unit_graphics_load_descriptors, 16 entries),
 * appended by battle_gfx_append_unit_graphics_load_descriptor and consumed by
 * the per-unit graphics loader at 0x80088904. */
typedef struct battle_unit_graphics_load_descriptor {
    u8 map_x;
    u8 map_y;
    u8 map_level;
    u8 _pad003;
    u16 map_height;
    s16 portrait_id;
    u16 palette_id;
    u16 misc_id;
    struct battle_stats* battle_stats;
    u32 flags;
} battle_unit_graphics_load_descriptor_t;

typedef char battle_unit_graphics_load_descriptor_size_must_be_0x14
    [(sizeof(battle_unit_graphics_load_descriptor_t) == 0x14) ? 1 : -1];

/* Full-screen fade primitive pair: a TILE followed by the DR_MODE that
 * selects its texture page and window. */
typedef struct battle_gfx_fade_overlay {
    TILE tile;    /* 0x00 */
    DR_MODE mode; /* 0x10 */
} battle_gfx_fade_overlay_t;

typedef char battle_gfx_fade_overlay_size_must_be_0x1c[(sizeof(battle_gfx_fade_overlay_t) == 0x1c) ? 1 : -1];

/* Draw-area pair: two DR_AREA packets followed by their rectangles; the
 * BATTLE twin of world_gfx_scaled_draw_area_pair_t. */
typedef struct battle_gfx_scaled_draw_area_pair {
    DR_AREA areas[2]; /* 0x00 */
    RECT rects[2];    /* 0x18 */
} battle_gfx_scaled_draw_area_pair_t;

typedef char
    battle_gfx_scaled_draw_area_pair_size_must_be_0x28[(sizeof(battle_gfx_scaled_draw_area_pair_t) == 0x28) ? 1 : -1];

/* Weather particle positions at 0x800fc358: weather 0x8f (snow) uses all 64,
 * 0x90 (rain) fills two sets of 32. */
typedef struct battle_weather_particle {
    s16 x;
    s16 y;
    s16 z;
    /* g_battle_map_weather_fall_speed, plus the sprite index for 0x8f; rain
     * reuses it as splash state (0/1/2) and the splash set as a countdown. */
    s16 fall_speed;
} battle_weather_particle_t;

/* Three weather sprite UV rectangles (g_battle_map_weather_drop_uv_rects). */
typedef struct battle_weather_sprite_rects {
    RECT rects[3];
} battle_weather_sprite_rects_t;

/* Sprite part dimensions in 8-pixel tiles, indexed by attribute bits 10-13. */
typedef struct battle_gfx_part_size {
    s32 width;
    s32 height;
} battle_gfx_part_size_t;

extern battle_gfx_part_size_t g_battle_gfx_part_sizes[];
extern u8 g_battle_gfx_body_part_seq_data[];
extern u8 g_battle_gfx_body_part_sprite_data[];
extern u8* g_battle_gfx_compressed_data;
extern s32 g_battle_gfx_compressed_nibble_phase;
extern s32 g_battle_gfx_compressed_offset;
extern u8 g_battle_gfx_item_sprite_display_data[];
extern u8 g_battle_gfx_numeric_sprite_display_0[];
extern u8 g_battle_gfx_numeric_sprite_display_1[];
extern u8 g_battle_gfx_numeric_sprite_display_2[];
extern u8 g_battle_gfx_screen_color_modulation[3];
extern u8 g_battle_gfx_screen_color_modulation_backup[3];
extern u8 g_battle_gfx_spritesheet_alt_data[];
extern u8 g_battle_gfx_spritesheet_alt_fallback[];
extern battle_gfx_spritesheet_data_t g_battle_gfx_spritesheet_data[BATTLE_SPRITESHEET_ID_COUNT];
extern battle_gfx_unit_shp_frame_tables_t g_battle_gfx_spritesheet_fallback;
extern u8 g_battle_gfx_spritesheet_ids_by_vram_slot[];
extern battle_gfx_unit_shp_frame_tables_t g_battle_gfx_spritesheet_record_data[5];
extern u8 g_battle_gfx_spritesheet_seq_data[];
extern u8 g_battle_gfx_spritesheet_shp_data[];
extern battle_gfx_spritesheet_slot_t g_battle_gfx_spritesheet_slots[];
extern u8 g_battle_gfx_status_bubble_sprite_display_data[];
extern u8 g_battle_gfx_tpage7_vram_allocation_grid[15][16];
extern u8 g_battle_gfx_unit_sprite_display_data[];
extern battle_gfx_vram_slot_t g_battle_gfx_vram_slots[];
extern u8 g_battle_gfx_weapon_sprite_display_data[];
extern battle_image_location_t g_battle_menu_texture_location;
extern battle_unit_graphics_load_descriptor_t g_unit_graphics_load_descriptors[];

void battle_gfx_append_gpu_primitive_to_secondary_otag(u32* primitive);
void battle_gfx_append_unit_graphics_load_descriptor(u8 map_x, u8 map_y, u8 map_level, u16 map_height, s32 portrait_id,
    u16 palette_id, u16 misc_id, struct battle_stats* g_battle_unit_stats, u32 flags);
s32 battle_gfx_calculate_screen_z_from_misc_map_data(struct battle_unit_misc_data* unit);
void battle_gfx_check_tile_status_palette_mod(
    struct battle_unit_misc_data* unit, s32 tile_effect_level, s32 final_value);
void battle_gfx_clear_four_state_words(void);
s32 battle_gfx_clear_shadow_graphic_trigger_by_misc_id(u32 misc_id);
void battle_gfx_clear_tpage7_vram_allocation_grid(void);
void battle_gfx_configure_misc_unit_palette_modulation_1f(s32 unit_id);
void battle_gfx_construct_polygon_data_for_units(
    battle_gfx_sprite_display_t* display, s32 end, s32 start, s16* position, s16 angle, u16 mode, s16* scale, u32* ot);
void battle_gfx_decompress_attack_spritesheet(u8* source, u8* destination);
void battle_gfx_draw_or_append_gpu_primitive(void* primitive);
void battle_gfx_draw_screen_color_modulation_overlay(void);
void battle_gfx_draw_wait_direction_unit(void);
void battle_gfx_free_tpage7_vram(RECT* rect);
u32 battle_gfx_get_spritesheet_flying_flag(u32 spritesheet_id);
u8* battle_gfx_get_spritesheet_seq_data_address(u32 index);
battle_gfx_unit_shp_frame_tables_t* battle_gfx_get_spritesheet_shp_data_address(u32 index);
s32 battle_gfx_get_spritesheet_vram_by_battle_id(u32 battle_id);
void battle_gfx_tint_all_units_by_team(void);
void battle_gfx_init_default_poly_ft4(POLY_FT4* primitive);
void battle_gfx_init_earned_exp_jp_display(struct battle_unit_misc_data* unit);
s32 battle_gfx_init_evtchr_vram_slots(void);
void battle_gfx_init_misc_unit_palette_modulation(s32 misc_id, s32 mode, s16 red, s16 green, s16 blue);
void battle_gfx_init_position_vector_copies(struct battle_unit_misc_data* unit);
void battle_gfx_init_render_frame(void);
void battle_gfx_init_render_state(void);
void battle_gfx_load_casting_unit_graphics(void);
void battle_gfx_load_spritesheet_into_vram_slot(u8* data, u32 index, s32 unused, struct battle_unit_misc_data* unit);
s32 battle_gfx_load_unit_graphics_by_battle_id(s32, s32);
void battle_gfx_modify_palette_based_on_team(struct battle_unit_misc_data* unit, s32 amount);
u32 battle_gfx_read_compressed_nibble(void);
void battle_gfx_reset_jumping_unit_graphic_triggers(void);
s32 battle_gfx_reset_unit_graphic_trigger(u32 misc_id);
void battle_gfx_set_evtchr_palette_by_misc_id(s32 palette_row_index, s32 slot_index, s32 clut_index);
s32 battle_gfx_set_shadow_graphic_trigger_by_misc_id(u32 misc_id);
void battle_gfx_set_thrown_item_palette_by_misc_id(u32 item_id, u32 misc_id);
void battle_gfx_submit_screen_fade_overlay(battle_gfx_fade_overlay_t* overlay, s32 level);
void battle_gfx_update_screen_color_modulation_fade(void);
void battle_gfx_update_sprite_transparency_flag(struct battle_unit_misc_data* unit);
void battle_gfx_update_status_bubble_display(struct battle_unit_misc_data* unit);
void battle_gfx_update_status_bubble_graphic_trigger(struct battle_unit_misc_data* unit);
void battle_gfx_update_status_bubbles_and_graphics(void);
void battle_gfx_update_unit_palette_animation(void);
void battle_gfx_update_unit_palettes(void);
/* One of the two 0xee28-byte battle render buffers. The primitive pools are
 * pre-initialised once here; the swap helper at 0x8005a0cc reads the ordering
 * table pointer at +0xe5b4. */
typedef struct battle_render_buffer {
    POLY_GT3 gt3[360]; /* 0x00000 */
    POLY_GT4 gt4[710]; /* 0x03840 */
    POLY_F3 f3[64];    /* 0x0c878 */
    POLY_F4 f4[256];   /* 0x0cd78 */
    POLY_G4 overlay;   /* 0x0e578 */
    DR_MODE overlay_modes[2] /* 0x0e59c */;
    u32* otag;             /* 0x0e5b4 */
    u8 unknown_e5b8[0x70]; /* 0x0e5b8 */
    POLY_FT3 ft3[64];      /* 0x0e628 */
} battle_render_buffer_t;

typedef char battle_render_buffer_size_must_be_0xee28[(sizeof(battle_render_buffer_t) == 0xee28) ? 1 : -1];

extern battle_render_buffer_t g_battle_render_buffers[2];
/* The buffer being built this frame; main_gfx_swap_and_clear_otag alternates
 * it between the two g_battle_render_buffers entries. */
extern battle_render_buffer_t* g_battle_data;

union battle_texture_prim;

extern u32* g_current_otag_entry;

void battle_gfx_alloc_tpage7_vram(RECT* rect, union battle_texture_prim* prim, u32* image);
u8* battle_gfx_get_unit_sprite_frame_and_vram_ids(u32 misc_id, battle_unit_sprite_query_t* out);
void battle_gfx_apply_weather_time_tile_palette_mod_by_misc_id(u32 misc_id);
s32 battle_gfx_clear_loaded_evtchr_slot(s32 slot_index);
s32 battle_gfx_clear_saved_evtchr_slot(s32 slot_index);
void battle_gfx_configure_misc_unit_palette_modulation(s32 misc_id, s32 mode, s16 red, s16 green, s16 blue);
void battle_gfx_extract_deployed_unit_palettes(void);
void battle_gfx_flip_poly_ft4_uv_if_negative(POLY_FT4* primitive, s32 direction);
u8* battle_gfx_get_body_part_seq_data(void);
u8* battle_gfx_get_body_part_sprite_data(void);
u8* battle_gfx_get_spritesheet_seq_data(u32 spritesheet_id);
u8* battle_gfx_get_spritesheet_shp_data(u32 spritesheet_id);
u32 battle_gfx_get_unit_spritesheet_height_by_misc_id(u32 misc_id);
s32 battle_gfx_get_unit_spritesheet_height_by_unit_id(u32 unit_id);
void battle_gfx_highlight_all_units_blue_or_red(s32 mode);
s16 battle_gfx_increment_counter(void);
void battle_gfx_init_deployment_and_reset_unit_graphics_state(void);
u8* battle_gfx_init_unit_sprite_display(s32 unit_index);
s32 battle_gfx_is_spritesheet_id_loaded(s32 spritesheet_id);
s32 battle_gfx_load_misc_unit_into_evtchr_slot(s32 misc_id, s32 slot);
void battle_gfx_modify_vram_palette(s32 mode, const u16* source, s32 section_id, s32 unit_misc_id, s32 preset, s16 red,
    s16 green, s16 blue, s32 final_value);
s32 battle_gfx_release_reserved_vram_slot(s32 slot_index);
void battle_gfx_release_unit_vram_slot(struct battle_unit_misc_data* unit);
void battle_gfx_reset_unit_graphics_load_state(void);
s32 battle_gfx_save_evtchr_slot(s32 slot_index);
void battle_gfx_set_draw_mode_for_texture_page(DR_MODE* mode, s32 page);
void battle_gfx_set_draw_mode_from_rect(void* mode, const u16* rect);
void battle_gfx_set_primitive_brightness(u8* primitive, s32 brightness);
void battle_gfx_start_screen_color_modulation_fade(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration);
s32 battle_gfx_step_load_unit_shp_seq_data(void);
void battle_gfx_store_sprite_display_data(battle_gfx_sprite_display_data_t* display, s32 index, s8 x_shift, s8 y_shift,
    u16 u, u16 v, u16 width, u16 height, u16 flags);
s32 battle_gfx_unpack_evtchr_file_to_vram_slot(s32 index, u8* file);
extern s32 g_battle_gfx_animation_layer_priorities[][4];
/* Fixed chicken palettes; the other three blocks are in fft/battle.h. */
extern u8 g_battle_gfx_chicken_palettes[][32];
extern s16 g_battle_gfx_counter;
extern u8 g_battle_gfx_fallback_seq_data[];
extern u8 g_battle_gfx_fallback_shp_frame_data[];
extern u8 g_battle_gfx_item_graphic_data[];
extern s32 g_battle_gfx_last_loaded_seq_id;
extern s32 g_battle_gfx_last_loaded_shp_id;
/* Byte-granular bump-allocation cursors: the consuming files do raw byte
 * arithmetic on these, so the element type must stay u8. */
extern u8* g_battle_gfx_load_data_cursor;
extern u8* g_battle_gfx_shp_frame_data_cursor;
extern s16 g_battle_gfx_previous_counter;
extern battle_gfx_fade_overlay_t g_battle_gfx_screen_fade_overlays[]; /* one fade overlay per packet buffer */
/* Overlay draw-mode primitives, one per screen polarity. */
extern DR_MODE g_battle_gfx_screen_modulation_draw_modes[2];
/* Frame counter and frame index of the selection cursor's bob animation. */
extern s32 g_battle_gfx_selection_cursor_bob_timer;
extern s32 g_battle_gfx_selection_cursor_bob_frame;
extern void* g_battle_gfx_sp2_data;
extern u16 g_battle_gfx_sprite_y_rotations[];
/* Status flag shown by each status bubble id (0x16 words at 0x80093cb4).
 * Bubbles 0 (KO), 0x14 (Death Sentence) and 0x15 (casting) hold 0 and are
 * tested separately by battle_gfx_determine_status_bubble_parameters. */
extern s32 g_battle_gfx_status_bubble_status_masks[];
extern u8 g_battle_gfx_targeted_frame_back[];
extern u8 g_battle_gfx_targeted_frame_front[];
/* Advanced by g_animation_speed once per call and sampled with bit 5 to blink
 * the highlighted arrow. */
extern u32 g_battle_gfx_wait_arrow_blink_timer;
void battle_gfx_init_image_loading(POLY_FT4* primitive, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters);
extern battle_render_buffer_t* g_battle_map_ambient_restored_buffer;
extern battle_weather_particle_t g_battle_map_rain_splash_particles[32];
extern battle_weather_sprite_rects_t g_battle_map_weather_drop_uv_rects;
extern battle_weather_particle_t g_battle_map_weather_particles[64];
extern battle_render_buffer_t* g_battle_target_tile_color_buffer;

void battle_gfx_copy_other_spr_palette_data_to_ram(u8* src);
void battle_gfx_load_wep_spr_data(u8* data);

#endif
