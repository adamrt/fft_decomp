/* The new-game prologue steps encoded text and picture commands, fades its
 * text strips and crossfades the 210-by-180 illustrations in the frame image. */

#include "fft/prologue.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Prologue state and its encoded line records. */
typedef struct prologue_line {
    u8 texture_row;
    u8 width;
    u8 _unused_02[2];
} prologue_line_t;
typedef char prologue_line_size_must_be_4[sizeof(prologue_line_t) == 4 ? 1 : -1];

/* High nibble of the image command byte in prologue_entry_t.header. */
typedef enum prologue_image_operation {
    PROLOGUE_PREPARE_IMAGE = 0x10,
    PROLOGUE_START_CROSSFADE = 0x20
} prologue_image_operation_e;

enum { PROLOGUE_IMAGE_OPERATION_MASK = 0xf0, PROLOGUE_IMAGE_INDEX_MASK = 0x0f };

/* prologue_t.flags. */
typedef enum prologue_flag {
    PROLOGUE_NEXT_ENTRY = 0x01,  /* starts the next entry of the table, or ends at the target */
    PROLOGUE_NEXT_STEP = 0x02,   /* starts the next step of the entry */
    PROLOGUE_WAIT_CIRCLE = 0x08, /* the entry is complete: waits for circle */
    PROLOGUE_DIM = 0x10,         /* the line strips fade out before the next entry */
    PROLOGUE_SKIP = 0x20         /* start was pressed: fades out and ends */
} prologue_flag_e;

/* The new-game prologue's controller (g_prologue). */
typedef struct prologue {
    /* 0x00 */ u16 flags;
    /* 0x02 */ s16 ot_layer;
    /* 0x04 */ s16 current_index;
    /* 0x06 */ s16 target_index;
    /* 0x08 */ s16 step;
    /* 0x0a */ s16 step_count;
    /* 0x0c */ s16 tpage_x;
    /* 0x0e */ s16 tpage_y;
    /* 0x10 */ s16 line_texture_rows[7];
    /* 0x1e */ s16 line_widths[7];
    /* 0x2c */ s16 timer;
    /* 0x2e */ s16 intensity;
    /* 0x30 */ prologue_entry_t** entry_table;
    /* 0x34 */ prologue_entry_t* entry;
} prologue_t;
typedef char prologue_size_must_be_56[(sizeof(prologue_t) == 56) ? 1 : -1];

extern prologue_t g_prologue;
extern POLY_GT4 g_prologue_line_primitives[2][14];
static void prologue_prepare_crossfade(s32 file_index);
static void prologue_draw_text_lines(s32 intensity);
static void prologue_step_crossfade(void);
/* The new-game prologue's pictures are 210x180, at rows 43 to 222 of the
 * 256-pixel-wide frame image. */
#define PROLOGUE_IMAGE_WIDTH      210
#define PROLOGUE_IMAGE_HEIGHT     180
#define PROLOGUE_IMAGE_ROW_STRIDE 256

/* Crossfade control bytes and one fade step per picture row. */
typedef struct prologue_crossfade {
    u8 active;
    u8 band_end_row;
    u8 row_steps[PROLOGUE_IMAGE_HEIGHT];
} prologue_crossfade_t;
typedef char prologue_crossfade_size_must_be_182[sizeof(prologue_crossfade_t) == 182 ? 1 : -1];

extern prologue_crossfade_t g_prologue_crossfade;
static u16* volatile g_prologue_crossfade_new_picture;
static u16* g_prologue_crossfade_old_picture;
static u16* g_prologue_crossfade_frame_pixels;

/* prologue_step_crossfade reloads the source pointer for every column. */

/* Arm the new-game prologue sequence: start before entry 0 of entry_table with NEXT_ENTRY set, end
 * at entry target, draw its text in ordering-table entries ot_layer and ot_layer + 1, at full
 * brightness (0x80). */
void prologue_init_sequence(s32 target, s32 ot_layer, prologue_entry_t** entry_table) {
    g_prologue.flags = PROLOGUE_NEXT_ENTRY;
    g_prologue.current_index = 0;
    g_prologue.target_index = target;
    g_prologue.entry_table = entry_table;
    g_prologue.ot_layer = ot_layer;
    g_prologue.intensity = 0x80;
}

/* Empty. new_game_update calls it once the prologue has finished. */
void prologue_finished_stub(void) {
}

/* Advance the new-game prologue by one frame: the picture crossfade and the text
 * of the current prologue entry. Returns 1 while it runs and 0 when it has
 * finished.
 *
 * An entry is one page of text lines. NEXT_ENTRY loads the next entry, or
 * finishes at target_index: its line count, texture page and image operation,
 * where 0x10 + n prepares the crossfade to picture n and 0x20 waits for that
 * picture's read and starts the crossfade. NEXT_STEP starts the next line,
 * reading every line's texture row and width from the entry on the first; the
 * timer starts at -16 and the line is revealed one pixel per frame. After the
 * last line the page waits for Circle (WAIT_CIRCLE), dims from 0x80 by 2 per
 * frame (DIM) and the next entry follows.
 *
 * Start sets SKIP: the lines keep advancing while the brightness runs down, and
 * the sequence ends once it is 0 and the crossfade has stopped. */
s32 prologue_update(void) {
    u16 flags;
    s32 i;
    prologue_entry_t* entry;
    s32 image_command;

    prologue_step_crossfade();
    if (!(g_prologue.flags & PROLOGUE_SKIP)) {
        if (g_input_new_button_presses & PSX_PAD_START) {
            sound_sfx_play(SOUND_SFX_COMPLETE);
            g_prologue.flags |= PROLOGUE_SKIP;
        }
        flags = g_prologue.flags;
        if (!(flags & PROLOGUE_SKIP)) {
            if (flags & PROLOGUE_DIM) {
                g_prologue.intensity -= 2;
                prologue_draw_text_lines(g_prologue.intensity);
                if (g_prologue.intensity == 0) {
                    g_prologue.intensity = 0x80;
                    g_prologue.flags = (g_prologue.flags ^ PROLOGUE_DIM) | PROLOGUE_NEXT_ENTRY;
                }
                return 1;
            }
            if (flags & PROLOGUE_WAIT_CIRCLE) {
                if (g_input_new_button_presses & PSX_PAD_CIRCLE) {
                    g_prologue.flags = (flags ^ PROLOGUE_WAIT_CIRCLE) | PROLOGUE_DIM;
                    g_prologue.intensity = 0x80;
                }
                prologue_draw_text_lines(g_prologue.intensity);
                return 1;
            }
            if (flags & PROLOGUE_NEXT_ENTRY) {
                flags ^= PROLOGUE_NEXT_ENTRY;
                g_prologue.flags = flags;
                if (g_prologue.current_index == g_prologue.target_index) {
                    return 0;
                }
                g_prologue.flags = flags | PROLOGUE_NEXT_STEP;
                entry = g_prologue.entry_table[g_prologue.current_index];
                g_prologue.entry = entry;
                g_prologue.step_count = (u8)entry->header;
                g_prologue.tpage_x = (entry->header & 0xf00) >> 2;
                g_prologue.tpage_y = (entry->header >> 4) & 0x100;
                /* The high nibble selects the operation; the low nibble selects its image. */
                image_command = (u8)(entry->header >> 16);
                switch (image_command & PROLOGUE_IMAGE_OPERATION_MASK) {
                case PROLOGUE_PREPARE_IMAGE:
                    prologue_prepare_crossfade(image_command & PROLOGUE_IMAGE_INDEX_MASK);
                    break;
                case PROLOGUE_START_CROSSFADE:
                    file_pending_wait();
                    g_prologue_crossfade.active = 1;
                    break;
                }
                {
                    s16* index = &g_prologue.current_index;

                    g_prologue.step = 0;
                    (*index)++;
                }
            }
        }
    }

    flags = g_prologue.flags;
    if (flags & PROLOGUE_NEXT_STEP) {
        g_prologue.flags = flags ^ PROLOGUE_NEXT_STEP;
        /* Every line is out: the page waits for Circle. */
        if (g_prologue.step == g_prologue.step_count) {
            g_prologue.flags = (flags ^ PROLOGUE_NEXT_STEP) | (PROLOGUE_NEXT_ENTRY | PROLOGUE_WAIT_CIRCLE);
            prologue_draw_text_lines(g_prologue.intensity);
            return 1;
        }
        /* Byte indexing preserves the retail address-add order and entry reloads. */
        if (g_prologue.step == 0) {
            for (i = 0; i < g_prologue.step_count; i++) {
                g_prologue.line_widths[i]
                    = g_prologue.entry->line_data[i * sizeof(prologue_line_t) + offsetof(prologue_line_t, width)];
                g_prologue.line_texture_rows[i]
                    = g_prologue.entry->line_data[i * sizeof(prologue_line_t) + offsetof(prologue_line_t, texture_row)];
            }
        }
        {
            s16* step = &g_prologue.step;

            g_prologue.timer = -0x10;
            (*step)++;
        }
    }

    if (++g_prologue.timer == g_prologue.line_widths[g_prologue.step - 1]) {
        g_prologue.flags |= PROLOGUE_NEXT_STEP;
    }

    if (g_prologue.flags & PROLOGUE_SKIP) {
        if (g_prologue.intensity != 0) {
            g_prologue.intensity -= 2;
        } else if (g_prologue_crossfade.active == 0) {
            return 0;
        }
    }

    prologue_draw_text_lines(g_prologue.intensity);
    return 1;
}

/* Draw the text lines of the current new-game prologue entry at the given
 * brightness.
 *
 * Each step of the entry is one line of text: line_texture_rows holds its 16-pixel row
 * in the texture page and line_widths its width in pixels. The lines are
 * centered, 20 pixels apart, as a block around the middle of the screen. A
 * finished line is drawn at full width and the line in progress up to timer
 * pixels; the pass with i == step adds that line's leading edge, at most 16
 * pixels whose right-hand corners are black, so the text fades in as it is
 * revealed.
 *
 * Every piece is two POLY_GT4s: the text, added to the picture at intensity in
 * ordering-table entry ot_layer, and a copy one pixel down and right, subtracted at
 * half intensity in entry ot_layer + 1.
 *
 * The coordinates are written in setXY4 order (x0, y0, x1, y1, ...) per
 * primitive, and width + 1 goes through its own variable (the target adds left
 * afterwards). */
static void prologue_draw_text_lines(s32 intensity) {
    POLY_GT4* prims;
    s32 i;
    s32 prim_index;
    s32 width;
    s32 width_plus_one;
    s32 left;
    s32 top_y;
    s32 u;
    s32 v;
    u16 clut;

    for (i = 0; i < g_prologue.step + 1; i++) {
        prim_index = i * 2;
        prims = g_prologue_line_primitives[g_gfx_active_buffer_index];
        SetPolyGT4(&prims[prim_index]);
        SetPolyGT4(&prims[prim_index + 1]);

        if (i == g_prologue.step) {
            /* Leading edge of the line in progress (line i - 1): black on its right. */
            if (g_prologue.line_widths[i - 1] < g_prologue.timer) {
                continue;
            }
            width = g_prologue.line_widths[i - 1] - g_prologue.timer;
            if (g_prologue.timer < 0) {
                width = g_prologue.timer + 0x10;
            } else if (width >= 0x11) {
                width = 0x10;
            }
            u = g_prologue.timer;
            if (u < 0) {
                u = 0;
            }
            v = g_prologue.line_texture_rows[i - 1] * 0x10;
            setRGB1(&prims[prim_index], 0, 0, 0);
            setRGB1(&prims[prim_index + 1], 0, 0, 0);
            setRGB3(&prims[prim_index], 0, 0, 0);
            setRGB3(&prims[prim_index + 1], 0, 0, 0);
            left = u - g_prologue.line_widths[i - 1] / 2;
            top_y = (i - 1) * 0x14 - g_prologue.step_count * 10;
        } else {
            /* Line i: the revealed part of the line in progress, or a whole finished line. */
            if (i == g_prologue.step - 1) {
                width = g_prologue.timer;
            } else {
                width = g_prologue.line_widths[i];
            }
            u = 0;
            if (width <= 0) {
                continue;
            }
            v = g_prologue.line_texture_rows[i] * 0x10;
            setRGB1(&prims[prim_index], intensity, intensity, intensity);
            setRGB1(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
            setRGB3(&prims[prim_index], intensity, intensity, intensity);
            setRGB3(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
            left = -(g_prologue.line_widths[i] / 2);
            top_y = i * 0x14 - g_prologue.step_count * 10;
        }

        setRGB0(&prims[prim_index], intensity, intensity, intensity);
        setRGB0(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
        setRGB2(&prims[prim_index], intensity, intensity, intensity);
        setRGB2(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);

        clut = GetClut(0, 0x1e3);
        prims[prim_index + 1].clut = clut;
        prims[prim_index].clut = clut;
        prims[prim_index].tpage = GetTPage(0, 1, g_prologue.tpage_x, g_prologue.tpage_y);
        prims[prim_index + 1].tpage = GetTPage(0, 2, g_prologue.tpage_x, g_prologue.tpage_y);

        setXY4(&prims[prim_index], left, top_y, left + width, top_y, left, top_y + 0x10, left + width, top_y + 0x10);
        width_plus_one = width + 1;
        setXY4(&prims[prim_index + 1], left + 1, top_y + 1, left + width_plus_one, top_y + 1, left + 1, top_y + 0x11,
            left + width_plus_one, top_y + 0x11);
        setUV4(&prims[prim_index], u, v, u + width, v, u, v + 0x10, u + width, v + 0x10);
        setUV4(&prims[prim_index + 1], u, v, u + width, v, u, v + 0x10, u + width, v + 0x10);

        SetSemiTrans(&prims[prim_index], 1);
        SetSemiTrans(&prims[prim_index + 1], 1);
        AddPrim(&g_gfx_otags[g_gfx_active_buffer_index][g_prologue.ot_layer], &prims[prim_index]);
        AddPrim(&g_gfx_otags[g_gfx_active_buffer_index][g_prologue.ot_layer + 1], &prims[prim_index + 1]);
    }
}

/* Prepare the crossfade of the new-game prologue's picture to OPNBK image
 * file_index + 2.
 *
 * Points the crossfade at its three buffers, starts the asynchronous read of the
 * new image into g_system_work_buffer_0 (the crossfade's source) and copies the
 * 210x180 picture on screen out of the frame image into g_system_work_buffer_1,
 * packed 210 pixels per row. It clears the per-row fade steps, the running flag
 * and the blend band's end row; image operation 0x20 of a later prologue entry waits for
 * the read and sets the running flag. */
static void prologue_prepare_crossfade(s32 file_index) {
    u16* copy_source;
    u16* copy_destination;
    u16* file_pixels;
    s32 row;
    s32 source_row;
    s32 column;
    s32 source_offset;
    s32 destination_index;
    s32 source_index;
    u16* pixels;
    u16 value;
    u16(*file_image)[256];

    copy_source = (u16*)g_system_work_buffer_0;
    copy_destination = (u16*)g_system_work_buffer_1;
    file_image = g_file_destination;
    /* The 210x180 picture begins at row 43 of the 256-pixel frame image. */
    file_pixels = file_image[43];
    g_prologue_crossfade_new_picture = copy_source;
    g_prologue_crossfade_old_picture = copy_destination;
    g_prologue_crossfade_frame_pixels = file_pixels;
    file_request_openbk_image(file_index + 2, g_system_work_buffer_0);

    row = 0;
    source_row = 0;
    for (; row < PROLOGUE_IMAGE_HEIGHT; row++) {
        column = 0;
        source_offset = row * PROLOGUE_IMAGE_ROW_STRIDE;
        while (column < PROLOGUE_IMAGE_WIDTH) {
            destination_index = source_row + column;
            source_index = source_offset + column++;
            destination_index <<= 1;
            pixels = g_prologue_crossfade_frame_pixels;
            value = pixels[source_index];
            /* Destination indexing hoists pointer loads; byte-pointer arithmetic changes registers. */
            *(u16*)(destination_index + (u32)g_prologue_crossfade_old_picture) = value;
        }
        g_prologue_crossfade.row_steps[row] = 0;
        source_row += PROLOGUE_IMAGE_WIDTH;
    }

    g_prologue_crossfade.active = 0;
    g_prologue_crossfade.band_end_row = 0;
}

#define PROLOGUE_IMAGE_BLEND_BAND_HEIGHT 17
#define PROLOGUE_IMAGE_BLEND_END_ROW     (PROLOGUE_IMAGE_HEIGHT + PROLOGUE_IMAGE_BLEND_BAND_HEIGHT)

/*
 * Advance the bottom-up crossfade between the two 210x180 opening images by one row.
 *
 * Each call blends the 17-row band immediately before band_end_row: every row in the
 * band steps its own fade counter and writes the per-component mix of the source
 * and destination pixels into the file image.
 *
 * The reverse row is computed inside the row test, which makes it local to that
 * block. GCC 2.7.2's local-alloc orders exactly three block quantities with a
 * fixed-index comparison that would allocate the first half of the *210 multiply
 * first; with four it sorts by priority and the second half gets $v0, as in the
 * target. The s16 components are copied for the compare and the difference,
 * while the sums use the originals.
 */
static void prologue_step_crossfade(void) {
    s32 row;
    s32 last_row;
    s32 reverse_row;
    s32 column;
    s32 source_row_start;
    s32 destination_row_start;
    u16 source_pixel;
    u16 destination_pixel;
    u16 pixel;
    s32 component;
    s16 source_component;
    s16 destination_component;
    s32 previous_components;
    s32 difference;
    s16 component_delta_q8;
    s16 weighted_delta_q8;
    u8 fade;

    if (g_prologue_crossfade.active != 0) {
        row = g_prologue_crossfade.band_end_row - PROLOGUE_IMAGE_BLEND_BAND_HEIGHT;
        if (row < g_prologue_crossfade.band_end_row) {
            last_row = PROLOGUE_IMAGE_HEIGHT - 1;
            do {
                if (row >= 0 && row < PROLOGUE_IMAGE_HEIGHT) {
                    reverse_row = last_row - row;
                    column = 0;
                    source_row_start = reverse_row * PROLOGUE_IMAGE_WIDTH;
                    destination_row_start = reverse_row * PROLOGUE_IMAGE_ROW_STRIDE;
                    fade = g_prologue_crossfade.row_steps[row]++;
                    do {
                        /* An ordinary source-pointer load is hoisted and adds a saved-register frame. */
                        source_pixel = g_prologue_crossfade_new_picture[source_row_start + column];
                        destination_pixel = g_prologue_crossfade_old_picture[source_row_start + column];
                        pixel = 0;
                        if (source_pixel != destination_pixel) {
                            for (component = 2; component >= 0; component--) {
                                previous_components = pixel >> 5;
                                source_component = source_pixel & 0x1F;
                                destination_component = destination_pixel & 0x1F;
                                if (source_component != destination_component) {
                                    difference = source_component - destination_component;
                                    component_delta_q8 = difference << 8;
                                    weighted_delta_q8 = component_delta_q8 / 16 * fade;
                                    pixel = previous_components
                                        | ((destination_component + weighted_delta_q8 / 256) << 10);
                                } else {
                                    pixel = previous_components | (source_component << 10);
                                }
                                source_pixel >>= 5;
                                destination_pixel >>= 5;
                            }
                        } else {
                            pixel = source_pixel;
                        }
                        g_prologue_crossfade_frame_pixels[destination_row_start + column]
                            = pixel | (destination_pixel & 0x8000);
                        column++;
                    } while (column < PROLOGUE_IMAGE_WIDTH);
                }
                row++;
            } while (row < g_prologue_crossfade.band_end_row);
        }
        {
            u8* band_end_row = &g_prologue_crossfade.band_end_row;

            if (++*band_end_row >= PROLOGUE_IMAGE_BLEND_END_ROW) {
                g_prologue_crossfade.active = 0;
            }
        }
    }
}
