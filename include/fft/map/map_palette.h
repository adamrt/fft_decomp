#ifndef MAP_PALETTE_H
#define MAP_PALETTE_H

/* Battle map palettes: the CLUT rows and their runtime colors, the color modes of fades, and the CLUT cycles. */

#include "fft/gfx/gfx.h"
#include "psx/types.h"

/* color modes */
/* Map color-transition commands: the source color and treatment of RGB offsets.
 * map_palette_modify_from_other honors modes 5-7; every other value adds the
 * offsets to its supplied colors, including RESTORE. */
typedef enum map_color_mode {
    MAP_COLOR_MODE_CURRENT = 0,           /* the color in transition plus the offsets */
    MAP_COLOR_MODE_CURRENT_HALF = 1,      /* half of it plus the offsets */
    MAP_COLOR_MODE_CURRENT_GRAY = 2,      /* its gray (2R + 3G + B) / 6 plus the offsets */
    MAP_COLOR_MODE_CURRENT_GRAY_HALF = 3, /* half that gray plus the offsets */
    MAP_COLOR_MODE_BASE = 4,              /* the last applied color plus the offsets */
    MAP_COLOR_MODE_BASE_HALF = 5,
    MAP_COLOR_MODE_BASE_GRAY = 6,
    MAP_COLOR_MODE_BASE_GRAY_HALF = 7,
    MAP_COLOR_MODE_RESTORE = 8,     /* map transitions restore the last applied color and ignore offsets */
    MAP_COLOR_MODE_BASE_REPEAT = 9, /* as BASE, and record the offsets so that the fade alternates with RESTORE */
    MAP_COLOR_MODE_STOP_REPEAT = 10 /* only clear the repeat record */
} map_color_mode_e;

/* The repeat member of the map color transitions and of the CLUT animations. */
typedef enum map_color_repeat {
    MAP_COLOR_REPEAT_OFF = 0,
    MAP_COLOR_REPEAT_TINTED = 1,  /* fading to the recorded offsets; a fade back to the base follows */
    MAP_COLOR_REPEAT_RESTORED = 2 /* fading back to the base; the offsets follow again */
} map_color_repeat_e;

/* CLUT rows */
/* A CLUT row of the battle map: one VRAM line of sixteen 16-color CLUTs,
 * at y 0x1e0 through 0x1ed (map_palette_upload_clut_rows uploads a 256x14 image). Index into
 * map_palette_state_t.clut_rows and g_map_palette_modified_colors. */
typedef enum map_clut_row {
    MAP_CLUT_ROW_MAP = 0,
    MAP_CLUT_ROW_MARKED_TILES = 1, /* the map's CLUTs for the polygons of marked tiles */
    MAP_CLUT_ROW_SPRITES = 2,      /* weather particles, cursor tile glow, arrow, cursor, shadow, status bubble */
    MAP_CLUT_ROW_UNITS = 3,        /* one CLUT per unit, as in rows 4 to 7 and 10 */
    MAP_CLUT_ROW_UNIT_SHADOWS = 4,
    MAP_CLUT_ROW_ITEM_A = 5, /* the two palettes of the item a unit holds or throws */
    MAP_CLUT_ROW_ITEM_B = 6,
    MAP_CLUT_ROW_ACTION_RESULTS = 7,
    MAP_CLUT_ROW_UNITS_SECOND = 10, /* the second eight palettes of each unit's sheet */
    MAP_CLUT_ROW_WEAPONS = 11,
    MAP_CLUT_ROW_EFFECT = 12,
    MAP_CLUT_ROW_EFFECT_SECOND = 13
} map_clut_row_e;

enum {
    MAP_CLUT_ROW_COUNT = 14,
    MAP_CLUTS_PER_ROW = CLUT_PALETTE_COUNT,
    MAP_CLUT_COLOR_COUNT = PALETTE_COLOR_COUNT,
    MAP_CLUT_ROW_COLOR_COUNT = MAP_CLUTS_PER_ROW * MAP_CLUT_COLOR_COUNT,
    MAP_CLUT_ROWS_Y = 480 /* VRAM line of MAP_CLUT_ROW_MAP; the rows follow one line apart */
};

/* The 16-bit CLUT value of CLUT clut_index of a map CLUT row: the value of
 * GetClut(clut_index * 16, MAP_CLUT_ROWS_Y + clut_row), as a constant. */
#define MAP_CLUT(clut_row, clut_index) (((MAP_CLUT_ROWS_Y + (clut_row)) << 6) | (clut_index))

/* palette state */
/* A signed five-bit channel delta (-31..31), biased for table indexing. */
enum {
    MAP_PALETTE_DELTA_BIAS = 31,
    MAP_PALETTE_DELTA_COUNT = 63,
    MAP_PALETTE_BLEND_STEP_COUNT = 32,
    MAP_PALETTE_FAST_BLEND_STEP_COUNT = 8
};

/* Expanded five-bit RGB components and channel deltas biased by 31;
 * each channel occupies one byte, with the transparency bit stored separately. */
typedef struct map_palette_color_components {
    rgb24_t color;
    u8 semi_transparent;
    rgb24_t delta_biased; /* target - current + MAP_PALETTE_DELTA_BIAS */
} map_palette_color_components_t;
typedef char map_palette_color_components_size_must_be_7[(sizeof(map_palette_color_components_t) == 7) ? 1 : -1];

typedef struct map_palette_animation_state {
    u8 active;
    u8 blend_step;    /* 0x01; index into s_palette_blend_steps_32/_8 (battle/core/map_palettes.c) */
    u8 delay_counter; /* 0x02; one blend step each time it reaches mode >> 2 */
    u8 mode;
    u8 repeat;            /* 0x04; map_color_repeat_e */
    rgb24_t repeat_color; /* 0x05 */
} map_palette_animation_state_t;
typedef char map_palette_animation_state_size_must_be_8[(sizeof(map_palette_animation_state_t) == 8) ? 1 : -1];

/* The 0x982-byte runtime state of one CLUT row (map_clut_row_e) in g_map_palette_state:
 * an animation per CLUT, then the 256 colors of the row. components and
 * packed_colors are the runtime colors, not the separate 0x200-byte-per-row
 * upload staging g_map_palette_modified_colors. Palette extraction fills
 * both; map_palette_upload_clut_rows uploads the staging and clears upload_pending. */
typedef struct map_clut_row_state {
    u8 blend_as_row;  /* nonzero: one animation for all 256 colors; zero: one per 16-color CLUT */
    u8 load_disabled; /* blocks map_palette_load_data; tint and blend operations still run */
    map_palette_animation_state_t animations[MAP_CLUTS_PER_ROW];
    map_palette_color_components_t components[MAP_CLUT_ROW_COLOR_COUNT];
    u16 packed_colors[MAP_CLUT_ROW_COLOR_COUNT];
} map_clut_row_state_t;
typedef char map_clut_row_state_size_must_be_2434[(sizeof(map_clut_row_state_t) == 2434) ? 1 : -1];

/* The palette runtime state g_map_palette_state: map_palette_update_color_animations
 * addresses the CLUT rows from this base. upload_pending and move_pending are
 * the flags map_palette_upload_clut_rows services. */
typedef struct map_palette_state {
    s32 upload_pending;
    s32 move_pending;
    map_clut_row_state_t clut_rows[MAP_CLUT_ROW_COUNT];
} map_palette_state_t;
typedef char map_palette_state_size_must_be_34084[(sizeof(map_palette_state_t) == 34084) ? 1 : -1];

extern map_palette_state_t g_map_palette_state;

void map_palette_init_state(void);
void map_palette_load_data(const palette_t* palettes, s32 clut_row, s32 clut_index, s32 load_all);
void map_palette_set_colors(s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors);
void map_palette_set_from_other(const rgba5551_t* source, s32 clut_row, s32 clut_index, s32 all_colors, s16 red,
    s16 green, s16 blue, s32 update_current_colors);
void map_palette_modify_from_other(s32 mode, const rgba5551_t* source, s32 clut_row, s32 clut_index, s32 all_colors,
    s16 red, s16 green, s16 blue, s32 update_current_colors);
void map_palette_store_modified_colors(s32 clut_row, s32 clut_index, s32 store_all);
void map_palette_modify(
    s32 command, s32 amount, s32 clut_row, s32 clut_index, s32 all_colors, s16 red, s16 green, s16 blue);
void map_palette_color_field(s32 mode, s32 amount, s16 red, s16 green, s16 blue);
void map_palette_update_color_animations(void);
void map_palette_upload_clut_rows(void);

/* CLUT cycles */
s32 map_palette_start_clut_cycle(s32 frame_duration, s32 clut_row, s32 first_color, s32 last_color);
s32 map_palette_stop_clut_cycle(s32 handle);

#endif
