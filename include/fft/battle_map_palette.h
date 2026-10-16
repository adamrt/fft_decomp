#ifndef BATTLE_MAP_PALETTE_H
#define BATTLE_MAP_PALETTE_H

/* Declarations. */

#include "fft/gfx.h"
#include "psx/types.h"

/* Map color-transition commands: the source color and treatment of RGB offsets.
 * battle_map_palette_modify_from_other honors modes 5-7; every other value adds the
 * offsets to its supplied colors, including RESTORE. */
typedef enum battle_map_color_mode {
    BATTLE_MAP_COLOR_MODE_CURRENT = 0,           /* the color in transition plus the offsets */
    BATTLE_MAP_COLOR_MODE_CURRENT_HALF = 1,      /* half of it plus the offsets */
    BATTLE_MAP_COLOR_MODE_CURRENT_GRAY = 2,      /* its gray (2R + 3G + B) / 6 plus the offsets */
    BATTLE_MAP_COLOR_MODE_CURRENT_GRAY_HALF = 3, /* half that gray plus the offsets */
    BATTLE_MAP_COLOR_MODE_BASE = 4,              /* the last applied color plus the offsets */
    BATTLE_MAP_COLOR_MODE_BASE_HALF = 5,
    BATTLE_MAP_COLOR_MODE_BASE_GRAY = 6,
    BATTLE_MAP_COLOR_MODE_BASE_GRAY_HALF = 7,
    BATTLE_MAP_COLOR_MODE_RESTORE = 8, /* map transitions restore the last applied color and ignore offsets */
    BATTLE_MAP_COLOR_MODE_BASE_REPEAT
    = 9, /* as BASE, and record the offsets so that the fade alternates with RESTORE */
    BATTLE_MAP_COLOR_MODE_STOP_REPEAT = 10, /* only clear the repeat record */
} battle_map_color_mode_e;

/* The repeat member of the map color transitions and of the CLUT animations. */
typedef enum battle_map_color_repeat {
    BATTLE_MAP_COLOR_REPEAT_OFF = 0,
    BATTLE_MAP_COLOR_REPEAT_TINTED = 1,   /* fading to the recorded offsets; a fade back to the base follows */
    BATTLE_MAP_COLOR_REPEAT_RESTORED = 2, /* fading back to the base; the offsets follow again */
} battle_map_color_repeat_e;

/* CLUT rows */
/* A CLUT row of the battle map: one VRAM line of sixteen 16-color CLUTs,
 * at y 0x1e0 through 0x1ed (battle_map_clut_upload_rows uploads a 256x14 image). Index into
 * battle_map_palette_state_t.clut_rows and g_battle_map_palette_modified_colors. */
typedef enum battle_map_clut_row {
    BATTLE_MAP_CLUT_ROW_MAP = 0,
    BATTLE_MAP_CLUT_ROW_MARKED_TILES = 1, /* the map's CLUTs for the polygons of marked tiles */
    BATTLE_MAP_CLUT_ROW_SPRITES = 2, /* weather particles, cursor tile glow, arrow, cursor, shadow, status bubble */
    BATTLE_MAP_CLUT_ROW_UNITS = 3,   /* one CLUT per unit, as in rows 4 to 7 and 10 */
    BATTLE_MAP_CLUT_ROW_UNIT_SHADOWS = 4,
    BATTLE_MAP_CLUT_ROW_ITEM_A = 5, /* the two palettes of the item a unit holds or throws */
    BATTLE_MAP_CLUT_ROW_ITEM_B = 6,
    BATTLE_MAP_CLUT_ROW_ACTION_RESULTS = 7,
    BATTLE_MAP_CLUT_ROW_UNITS_SECOND = 10, /* the second eight palettes of each unit's sheet */
    BATTLE_MAP_CLUT_ROW_WEAPONS = 11,
    BATTLE_MAP_CLUT_ROW_EFFECT = 12,
    BATTLE_MAP_CLUT_ROW_EFFECT_SECOND = 13,
} battle_map_clut_row_e;

enum {
    BATTLE_MAP_CLUT_ROW_COUNT = 14,
    BATTLE_MAP_CLUTS_PER_ROW = CLUT_PALETTE_COUNT,
    BATTLE_MAP_CLUT_COLOR_COUNT = PALETTE_COLOR_COUNT,
    BATTLE_MAP_CLUT_ROW_COLOR_COUNT = BATTLE_MAP_CLUTS_PER_ROW * BATTLE_MAP_CLUT_COLOR_COUNT,
    BATTLE_MAP_CLUT_ROWS_Y = 480, /* VRAM line of BATTLE_MAP_CLUT_ROW_MAP; the rows follow one line apart */
};

/* The 16-bit CLUT value of CLUT clut_index of a map CLUT row: the value of
 * GetClut(clut_index * 16, BATTLE_MAP_CLUT_ROWS_Y + clut_row), as a constant. */
#define BATTLE_MAP_CLUT(clut_row, clut_index) (((BATTLE_MAP_CLUT_ROWS_Y + (clut_row)) << 6) | (clut_index))

/* A signed five-bit channel delta (-31..31), biased for table indexing. */
enum {
    BATTLE_MAP_PALETTE_DELTA_BIAS = 31,
    BATTLE_MAP_PALETTE_DELTA_COUNT = 63,
    BATTLE_MAP_PALETTE_BLEND_STEP_COUNT = 32,
    BATTLE_MAP_PALETTE_FAST_BLEND_STEP_COUNT = 8,
};

/* Expanded five-bit RGB components and channel deltas biased by 31;
 * each channel occupies one byte, with the transparency bit stored separately. */
typedef struct battle_map_palette_color_components {
    rgb24_t color;
    u8 semi_transparent;
    rgb24_t delta_biased; /* target - current + BATTLE_MAP_PALETTE_DELTA_BIAS */
} battle_map_palette_color_components_t;

typedef char
    battle_map_palette_color_components_size_must_be_7[(sizeof(battle_map_palette_color_components_t) == 7) ? 1 : -1];

typedef struct battle_map_palette_animation_state {
    u8 active;
    u8 blend_step;    /* 0x01; index into g_battle_map_palette_blend_steps_32/_8 */
    u8 delay_counter; /* 0x02; one blend step each time it reaches mode >> 2 */
    u8 mode;
    u8 repeat;            /* 0x04; battle_map_color_repeat_e */
    rgb24_t repeat_color; /* 0x05 */
} battle_map_palette_animation_state_t;

typedef char
    battle_map_palette_animation_state_size_must_be_8[(sizeof(battle_map_palette_animation_state_t) == 8) ? 1 : -1];

/* The 0x982-byte runtime state of one CLUT row (battle_map_clut_row_e) in g_battle_map_palette_state:
 * an animation per CLUT, then the 256 colors of the row. components and
 * packed_colors are the runtime colors, not the separate 0x200-byte-per-row
 * upload staging g_battle_map_palette_modified_colors. Palette extraction fills
 * both; battle_map_clut_upload_rows uploads the staging and clears upload_pending. */
typedef struct battle_map_clut_row_state {
    u8 enabled;
    u8 updates_disabled;
    battle_map_palette_animation_state_t animations[BATTLE_MAP_CLUTS_PER_ROW];
    battle_map_palette_color_components_t components[BATTLE_MAP_CLUT_ROW_COLOR_COUNT];
    u16 packed_colors[BATTLE_MAP_CLUT_ROW_COLOR_COUNT];
} battle_map_clut_row_state_t;

typedef char battle_map_clut_row_state_size_must_be_2434[(sizeof(battle_map_clut_row_state_t) == 2434) ? 1 : -1];

/* The palette runtime state g_battle_map_palette_state: battle_map_update_color_animations
 * addresses the CLUT rows from this base. upload_pending and move_pending are
 * the flags battle_map_clut_upload_rows services. */
typedef struct battle_map_palette_state {
    s32 upload_pending;
    s32 move_pending;
    battle_map_clut_row_state_t clut_rows[BATTLE_MAP_CLUT_ROW_COUNT];
} battle_map_palette_state_t;

typedef char battle_map_palette_state_size_must_be_34084[(sizeof(battle_map_palette_state_t) == 34084) ? 1 : -1];
extern u16 g_battle_map_palette_modified_colors[][BATTLE_MAP_CLUT_ROW_COLOR_COUNT]; /* [battle_map_clut_row_e][color] */
extern battle_map_palette_state_t g_battle_map_palette_state;
extern clut_t g_battle_map_palette_animation_frames;
void battle_map_palette_init_state(void);
void battle_map_update_color_animations(void);
void battle_map_palette_store_modified_colors(s32 clut_row, s32 clut_index, s32 store_all);

void battle_map_palette_modify(
    s32 command, s32 amount, s32 clut_row, s32 clut_index, s32 all_colors, s16 red, s16 green, s16 blue);

void battle_map_clut_upload_rows(void);
void battle_map_palette_update_all_from_color_data(s32 mode, const clut_t color_data);
void battle_map_palette_update_colors_by_index(s32 mode, s32 clut_index, const palette_t* color_data);

void battle_map_palette_update_and_store_colors(
    s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors);

void battle_map_palette_load_data(const palette_t* palettes, s32 clut_row, s32 clut_index, s32 load_all);
void battle_map_palette_update_colors(s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors);

void battle_map_palette_update_from_other(const rgba5551_t* source, s32 clut_row, s32 clut_index, s32 all_colors,
    s16 red, s16 green, s16 blue, s32 final_value);

/* CLUT cycles */
/* Eight runtime slots rotate a range of colors of a CLUT row for map texture animations.
 * They are separate from the 32 serialized 0x14-byte animation instructions
 * a map's mesh file carries. */
enum { BATTLE_MAP_CLUT_CYCLE_SLOT_COUNT = 8 };

/* One slot of g_battle_map_clut_cycle_states: the CLUT row and the range of its colors it
 * rotates, and its frame timing. */
typedef struct battle_map_clut_cycle_state {
    u8 active;
    u8 elapsed_frames;
    u8 frame_duration;
    u8 _padding_03;  /* aligns clut_row */
    u16 clut_row;    /* 0x04; battle_map_clut_row_e */
    u16 first_color; /* 0x06; color index in the row: the colors first..last rotate one place */
    u16 last_color;  /* 0x08; every frame_duration frames, towards last */
} battle_map_clut_cycle_state_t;

typedef char battle_map_clut_cycle_state_size_must_be_10[(sizeof(battle_map_clut_cycle_state_t) == 10) ? 1 : -1];
extern battle_map_clut_cycle_state_t g_battle_map_clut_cycle_states[BATTLE_MAP_CLUT_CYCLE_SLOT_COUNT];
s32 battle_map_clut_start_cycle(s32 frame_duration, s32 clut_row, s32 first_color, s32 last_color);
s32 battle_map_clut_stop_cycle(s32 handle);

#endif
