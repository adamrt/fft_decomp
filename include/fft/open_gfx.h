#ifndef OPEN_GFX_H
#define OPEN_GFX_H

/* OPEN.BIN graphics: display buffers, render records, the screen fade overlay and the new-game prologue. */

#include "fft/gfx.h"
#include "fft/world.h"
#include "fft/world_sprite.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* display */
/* Double-buffered draw/display environments, one pair per frame buffer (0x70
 * bytes each); gfx_init_screen_environments fills both and the movie
 * presenter flips between them. */
typedef struct gfx_screen_environment {
    DRAWENV draw; /* 0x00 */
    DISPENV disp; /* 0x5c */
} gfx_screen_environment_t;

extern gfx_screen_environment_t g_gfx_screen_environments[2];
extern u32 g_gfx_otags[2][16];

void gfx_init_screen_environments(s32 clear_first);
void gfx_clear_current_otag(void);
void gfx_clear_and_draw_current_frame(void);
void gfx_restore_frame_bin_page(void);

/* render records */
/* Render records. window_dispatch_record_list dispatches on flags
 * (display_object_flag_e): ANIMATED is a 36-byte record, IMAGE a
 * 56-byte record, and a record with BLANK or HIDDEN set is skipped. Each pool
 * holds WINDOW_RECORD_56_CAPACITY entries, and both publish record
 * pointers to one queue, whose leading 32-bit flags select the layout. */
enum { WINDOW_RECORD_56_CAPACITY = 16 };

/* display_object_animated_t.palette of the selected menu entry. */
enum { TITLE_MENU_ENTRY_HIGHLIGHTED_PALETTE = 9 };

/* CLUT position and source rectangle at +0x24 of a 56-byte record.
 * window_init_frame_record takes its address as one pointer. */
typedef struct window_record_56_tail {
    /* 0x00 */ point32_t clut; /* window_draw_image_record_56 passes it to GetClut */
    /* 0x08 */ RECT quad;      /* sprite u, v, w, h: texels within the texture page */
} window_record_56_tail_t;

typedef struct window_record_56 {
    /* 0x00 */ u32 flags;
    /* 0x04 */ s32 vram_x; /* quad.x / 4 + 64 * depth + 0x180 */
    /* 0x08 */ s32 vram_y;
    /* 0x0c */ s32 ot_layer;
    /* 0x10 */ s32 depth;
    /* 0x14 */ s32 pop_in_step;
    /* 0x18 */ s32 palette;
    /* 0x1c */ s32 x; /* quad.x - 0x80 */
    /* 0x20 */ s32 y; /* quad.y - 0x78 */
    /* 0x24 */ window_record_56_tail_t tail;
    /* 0x34: a sibling of `tail`, not part of it. window_init_frame_record
     * computes its address as records + index * 0x38 + 0x34, which a nested
     * `&rec[i].tail.r` cannot give (CSE rewrites that as tail + 0x10). */
    CVECTOR color;
} window_record_56_t;
typedef char open_render_record_56_size_must_be_56[(sizeof(window_record_56_t) == 56) ? 1 : -1];

/* x/y view of the 56-byte render records, based at records_56[0].x; the
 * retail code addresses that pair through its own base register, loaded
 * before the index is scaled, which &records_56[i].x does not reproduce. */
typedef struct window_record_56_position {
    /* 0x00 */ s32 x; /* == g_window_render_records_56[i].x */
    /* 0x04 */ s32 y;
    /* 0x08 */ u8 _unused_08[0x38 - 8];
} window_record_56_position_t;

/* One DR_MODE + SPRT pair of the shared 32-entry per-buffer sprite pool. */
typedef struct window_sprite_prim {
    /* 0x00 */ DR_MODE mode;
    /* 0x0c */ SPRT sprt;
} window_sprite_prim_t;
typedef char open_sprite_prim_size_must_be_32[(sizeof(window_sprite_prim_t) == 32) ? 1 : -1];

extern display_object_animated_t g_window_render_records_36[WINDOW_RECORD_56_CAPACITY];
extern window_record_56_t g_window_render_records_56[WINDOW_RECORD_56_CAPACITY];
extern window_record_56_position_t g_window_record_translation[WINDOW_RECORD_56_CAPACITY];
extern s32 g_window_next_render_record_36;
extern s32 g_window_next_render_record_56;
extern void* g_window_render_record_pointers[]; /* the queue both pools publish to */
extern s32 g_window_render_record_pointer_count;
extern display_object_sequence_t** g_window_record_36_animations;
extern display_object_cel_header_t** g_window_record_36_sprites;
extern window_sprite_prim_t g_window_sprite_primitive_pool[2][32];
extern s32 g_window_primitive_count;

void window_init_vram_record(
    s32 record_index, point16_t vram_position, point16_t image_size, point32_t clut_position, u32 flags);
void window_init_frame_record(s32 record_index, RECT quad, s32 depth, u16* image);
void window_load_record_image(s32 record_index, u32* image);
void window_measure_text_image(s32 combined_text_index, size32_t* dimensions);

/* fade overlay */
/* gfx_overlay_fade_t.flags. */
typedef enum gfx_overlay_fade_flag {
    GFX_OVERLAY_FADE_OUT = 0x01,    /* the overlay clears */
    GFX_OVERLAY_FADE_IN = 0x02,     /* the overlay darkens the screen */
    GFX_OVERLAY_FADE_ACTIVE = 0x04, /* the overlay is drawn; cleared when either fade ends */
} gfx_overlay_fade_flag_e;

/* The screen overlay fade: one record, since gfx_update_overlay_fade
 * reaches flags..rect and the tiles/modes from a single base. */
typedef struct gfx_overlay_fade {
    /* 0x00 */ u32 flags; /* gfx_overlay_fade_flag_e */
    /* 0x04 */ s32 frame;
    /* 0x08 */ s32 duration;
    /* 0x0c */ s32 level;
    /* 0x10 */ s32 abr;
    /* 0x14 */ s32 ot_index;
    /* 0x18 */ RECT rect;
    /* 0x20 */ TILE tiles[2];
    /* 0x40 */ DR_MODE modes[2];
} gfx_overlay_fade_t;

extern gfx_overlay_fade_t g_gfx_overlay_fade;

void gfx_start_overlay_fade_in(u32 duration);
void gfx_start_overlay_fade_out(u32 duration);
void gfx_update_overlay_fade(u32* otag);

/* new-game prologue */
/* New-game prologue entry: a packed line count, texture-page position and
 * image operation, followed by four-byte texture-row/width records. */
typedef struct prologue_line {
    u8 texture_row;
    u8 width;
    u8 _unused_02[2];
} prologue_line_t;
typedef char open_prologue_line_size_must_be_4[sizeof(prologue_line_t) == 4 ? 1 : -1];

typedef struct prologue_entry {
    u32 header;
    u8 line_data[0]; /* variable number of encoded prologue_line_t records */
} prologue_entry_t;
typedef char open_prologue_entry_header_size_must_be_4[sizeof(prologue_entry_t) == 4 ? 1 : -1];

/* High nibble of the image command byte in prologue_entry_t.header. */
typedef enum prologue_image_operation {
    PROLOGUE_PREPARE_IMAGE = 0x10,
    PROLOGUE_START_CROSSFADE = 0x20,
} prologue_image_operation_e;

enum {
    PROLOGUE_IMAGE_OPERATION_MASK = 0xf0,
    PROLOGUE_IMAGE_INDEX_MASK = 0x0f,
};

/* prologue_t.flags. */
typedef enum prologue_flag {
    PROLOGUE_NEXT_ENTRY = 0x01,  /* starts the next entry of the table, or ends at the target */
    PROLOGUE_NEXT_STEP = 0x02,   /* starts the next step of the entry */
    PROLOGUE_WAIT_CIRCLE = 0x08, /* the entry is complete: waits for circle */
    PROLOGUE_DIM = 0x10,         /* the line strips fade out before the next entry */
    PROLOGUE_SKIP = 0x20,        /* start was pressed: fades out and ends */
} prologue_flag_e;

/* The new-game prologue's controller (g_prologue). */
typedef struct prologue {
    /* 0x00 */ u16 flags;    /* prologue_flag_e */
    /* 0x02 */ s16 ot_layer; /* text and shadow occupy this layer and the next */
    /* 0x04 */ s16 current_index;
    /* 0x06 */ s16 target_index;
    /* 0x08 */ s16 step;
    /* 0x0a */ s16 step_count;
    /* 0x0c */ s16 tpage_x; /* (header & 0xf00) >> 2; GetTPage x */
    /* 0x0e */ s16 tpage_y; /* (header >> 4) & 0x100; GetTPage y */
    /* 0x10 */ s16 line_texture_rows[7];
    /* 0x1e */ s16 line_widths[7];
    /* 0x2c */ s16 timer;
    /* 0x2e */ s16 intensity;
    /* 0x30 */ prologue_entry_t** entry_table;
    /* 0x34 */ prologue_entry_t* entry;
} prologue_t;
typedef char open_prologue_size_must_be_56[(sizeof(prologue_t) == 56) ? 1 : -1];

extern prologue_t g_prologue;
extern prologue_entry_t** g_prologue_entries;
extern POLY_GT4 g_prologue_line_primitives[2][14];
extern u16* g_prologue_crossfade_frame_pixels;
extern u16* g_prologue_crossfade_old_picture;
/* Volatile: prologue_step_crossfade reloads it every column. */
extern u16* volatile g_prologue_crossfade_new_picture;
extern u8 g_prologue_crossfade_progress;
extern u8 g_prologue_crossfade_rows[];

void prologue_init_sequence(s32 target, s32 ot_layer, prologue_entry_t** entry_table);
s32 prologue_update(void);
void prologue_finished_stub(void);

/* OPNTEX */
extern u32* g_gfx_opntex_data;

void gfx_load_opntex(u32* tim);

#endif
