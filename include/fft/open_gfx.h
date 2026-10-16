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
 * bytes each); open_gfx_init_screen_environments fills both and the movie
 * presenter flips between them. */
typedef struct open_screen_environment {
    DRAWENV draw; /* 0x00 */
    DISPENV disp; /* 0x5c */
} open_screen_environment_t;

extern open_screen_environment_t g_open_gfx_screen_environments[2];
extern u32 g_open_gfx_otags[2][16];

void open_gfx_init_screen_environments(s32 clear_first);
void open_gfx_clear_current_otag(void);
void open_gfx_clear_and_draw_current_frame(void);
void open_gfx_restore_frame_bin_page(void);

/* render records */
/* Render records. window_dispatch_record_list dispatches on flags
 * (display_object_flag_e): ANIMATED is a 36-byte record, IMAGE a
 * 56-byte record, and a record with BLANK or HIDDEN set is skipped. Each pool
 * holds OPEN_GFX_RENDER_RECORD_CAPACITY entries, and both publish record
 * pointers to one queue, whose leading 32-bit flags select the layout. */
enum { OPEN_GFX_RENDER_RECORD_CAPACITY = 16 };

/* display_object_animated_t.palette of the selected menu entry. */
enum { OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE = 9 };

/* CLUT position and source rectangle at +0x24 of a 56-byte record.
 * open_gfx_init_window_frame_record takes its address as one pointer. */
typedef struct open_render_record_56_tail {
    /* 0x00 */ point32_t clut; /* open_gfx_draw_window_record passes it to GetClut */
    /* 0x08 */ RECT quad;      /* sprite u, v, w, h: texels within the texture page */
} open_render_record_56_tail_t;

typedef struct open_render_record_56 {
    /* 0x00 */ u32 flags;
    /* 0x04 */ s32 vram_x; /* quad.x / 4 + 64 * depth + 0x180 */
    /* 0x08 */ s32 vram_y;
    /* 0x0c */ s32 ot_layer;
    /* 0x10 */ s32 depth;
    /* 0x14 */ s32 pop_in_step;
    /* 0x18 */ s32 palette;
    /* 0x1c */ s32 x; /* quad.x - 0x80 */
    /* 0x20 */ s32 y; /* quad.y - 0x78 */
    /* 0x24 */ open_render_record_56_tail_t tail;
    /* 0x34: a sibling of `tail`, not part of it. open_gfx_init_window_frame_record
     * computes its address as records + index * 0x38 + 0x34, which a nested
     * `&rec[i].tail.r` cannot give (CSE rewrites that as tail + 0x10). */
    CVECTOR color;
} open_render_record_56_t;
typedef char open_render_record_56_size_must_be_56[(sizeof(open_render_record_56_t) == 56) ? 1 : -1];

/* x/y view of the 56-byte render records, based at records_56[0].x; the
 * retail code addresses that pair through its own base register, loaded
 * before the index is scaled, which &records_56[i].x does not reproduce. */
typedef struct open_render_record_56_position {
    /* 0x00 */ s32 x; /* == g_open_gfx_render_records_56[i].x */
    /* 0x04 */ s32 y;
    /* 0x08 */ u8 _unused_08[0x38 - 8];
} open_render_record_56_position_t;

/* One DR_MODE + SPRT pair of the shared 32-entry per-buffer sprite pool. */
typedef struct open_sprite_prim {
    /* 0x00 */ DR_MODE mode;
    /* 0x0c */ SPRT sprt;
} open_sprite_prim_t;
typedef char open_sprite_prim_size_must_be_32[(sizeof(open_sprite_prim_t) == 32) ? 1 : -1];

extern display_object_animated_t g_open_gfx_render_records_36[OPEN_GFX_RENDER_RECORD_CAPACITY];
extern open_render_record_56_t g_open_gfx_render_records_56[OPEN_GFX_RENDER_RECORD_CAPACITY];
extern open_render_record_56_position_t g_open_gfx_record_translation[OPEN_GFX_RENDER_RECORD_CAPACITY];
extern s32 g_open_gfx_next_render_record_36;
extern s32 g_open_gfx_next_render_record_56;
extern void* g_open_gfx_render_record_pointers[]; /* the queue both pools publish to */
extern s32 g_open_gfx_render_record_pointer_count;
extern display_object_sequence_t** g_open_gfx_record_36_animations;
extern display_object_cel_header_t** g_open_gfx_record_36_sprites;
extern open_sprite_prim_t g_open_gfx_sprite_primitive_pool[2][32];
extern s32 g_open_gfx_primitive_count;

void open_gfx_init_vram_render_record(
    s32 record_index, point16_t vram_position, point16_t image_size, point32_t clut_position, u32 flags);
void open_gfx_init_window_frame_record(s32 record_index, RECT quad, s32 depth, u16* image);
void open_gfx_load_window_record_image(s32 record_index, u32* image);
void open_text_measure_menu_image(s32 combined_text_index, size32_t* dimensions);

/* fade overlay */
/* open_overlay_fade_t.flags. */
typedef enum open_overlay_fade_flag {
    OPEN_OVERLAY_FADE_OUT = 0x01,    /* the overlay clears */
    OPEN_OVERLAY_FADE_IN = 0x02,     /* the overlay darkens the screen */
    OPEN_OVERLAY_FADE_ACTIVE = 0x04, /* the overlay is drawn; cleared when either fade ends */
} open_overlay_fade_flag_e;

/* The screen overlay fade: one record, since open_gfx_update_fade_overlay
 * reaches flags..rect and the tiles/modes from a single base. */
typedef struct open_overlay_fade {
    /* 0x00 */ u32 flags; /* open_overlay_fade_flag_e */
    /* 0x04 */ s32 frame;
    /* 0x08 */ s32 duration;
    /* 0x0c */ s32 level;
    /* 0x10 */ s32 abr;
    /* 0x14 */ s32 ot_index;
    /* 0x18 */ RECT rect;
    /* 0x20 */ TILE tiles[2];
    /* 0x40 */ DR_MODE modes[2];
} open_overlay_fade_t;

extern open_overlay_fade_t g_open_gfx_overlay_fade;

void open_gfx_start_overlay_fade_in(u32 duration);
void open_gfx_start_overlay_fade_out(u32 duration);
void open_gfx_update_fade_overlay(u32* otag);

/* new-game prologue */
/* New-game prologue entry: a packed line count, texture-page position and
 * image operation, followed by four-byte texture-row/width records. */
typedef struct open_prologue_line {
    u8 texture_row;
    u8 width;
    u8 _unused_02[2];
} open_prologue_line_t;
typedef char open_prologue_line_size_must_be_4[sizeof(open_prologue_line_t) == 4 ? 1 : -1];

typedef struct open_prologue_entry {
    u32 header;
    u8 line_data[0]; /* variable number of encoded open_prologue_line_t records */
} open_prologue_entry_t;
typedef char open_prologue_entry_header_size_must_be_4[sizeof(open_prologue_entry_t) == 4 ? 1 : -1];

/* High nibble of the image command byte in open_prologue_entry_t.header. */
typedef enum open_prologue_image_operation {
    OPEN_PROLOGUE_PREPARE_IMAGE = 0x10,
    OPEN_PROLOGUE_START_CROSSFADE = 0x20,
} open_prologue_image_operation_e;

enum {
    OPEN_PROLOGUE_IMAGE_OPERATION_MASK = 0xf0,
    OPEN_PROLOGUE_IMAGE_INDEX_MASK = 0x0f,
};

/* open_prologue_t.flags. */
typedef enum open_prologue_flag {
    OPEN_PROLOGUE_NEXT_ENTRY = 0x01,  /* starts the next entry of the table, or ends at the target */
    OPEN_PROLOGUE_NEXT_STEP = 0x02,   /* starts the next step of the entry */
    OPEN_PROLOGUE_WAIT_CIRCLE = 0x08, /* the entry is complete: waits for circle */
    OPEN_PROLOGUE_DIM = 0x10,         /* the line strips fade out before the next entry */
    OPEN_PROLOGUE_SKIP = 0x20,        /* start was pressed: fades out and ends */
} open_prologue_flag_e;

/* The new-game prologue's controller (g_open_prologue). */
typedef struct open_prologue {
    /* 0x00 */ u16 flags;    /* open_prologue_flag_e */
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
    /* 0x30 */ open_prologue_entry_t** entry_table;
    /* 0x34 */ open_prologue_entry_t* entry;
} open_prologue_t;
typedef char open_prologue_size_must_be_56[(sizeof(open_prologue_t) == 56) ? 1 : -1];

extern open_prologue_t g_open_prologue;
extern open_prologue_entry_t** g_open_prologue_entries;
extern POLY_GT4 g_open_prologue_line_primitives[2][14];
extern u16* g_open_prologue_crossfade_frame_pixels;
extern u16* g_open_prologue_crossfade_old_picture;
/* Volatile: open_prologue_step_crossfade reloads it every column. */
extern u16* volatile g_open_prologue_crossfade_new_picture;
extern u8 g_open_prologue_crossfade_progress;
extern u8 g_open_prologue_crossfade_rows[];

void open_prologue_init_sequence(s32 target, s32 ot_layer, open_prologue_entry_t** entry_table);
s32 open_prologue_update(void);
void open_prologue_finished_stub(void);

/* OPNTEX */
extern u32* g_open_gfx_opntex_data;

void open_gfx_load_opntex_into_frame_buffer(u32* tim);

#endif
