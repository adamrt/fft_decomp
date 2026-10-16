#ifndef PROLOGUE_H
#define PROLOGUE_H

/* The new-game prologue: text pages over crossfading pictures. */

#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

void prologue_start(void);

/* New-game prologue entry: a packed line count, texture-page position and
 * image operation, followed by four-byte texture-row/width records. */
typedef struct prologue_line {
    u8 texture_row;
    u8 width;
    u8 _unused_02[2];
} prologue_line_t;

typedef char prologue_line_size_must_be_4[sizeof(prologue_line_t) == 4 ? 1 : -1];

typedef struct prologue_entry {
    u32 header;
    u8 line_data[0]; /* variable number of encoded prologue_line_t records */
} prologue_entry_t;

typedef char prologue_entry_size_must_be_4[sizeof(prologue_entry_t) == 4 ? 1 : -1];

/* High nibble of the image command byte in prologue_entry_t.header. */
typedef enum prologue_image_operation {
    PROLOGUE_PREPARE_IMAGE = 0x10,
    PROLOGUE_START_CROSSFADE = 0x20,
} prologue_image_operation_e;

enum {
    PROLOGUE_IMAGE_OPERATION_MASK = 0xf0,
    PROLOGUE_IMAGE_INDEX_MASK = 0x0f,
};

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

typedef char prologue_size_must_be_56[(sizeof(prologue_t) == 56) ? 1 : -1];
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

#endif
