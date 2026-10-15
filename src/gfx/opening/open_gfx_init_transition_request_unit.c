#include "fft/open.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

void open_gfx_init_transition_request(s32 target, s32 ot_layer, open_gfx_transition_entry_t** entry_table) {
    g_open_gfx_transition.flags = OPEN_GFX_TRANSITION_NEXT_ENTRY;
    g_open_gfx_transition.current_index = 0;
    g_open_gfx_transition.target_index = target;
    g_open_gfx_transition.entry_table = entry_table;
    g_open_gfx_transition.ot_layer = ot_layer;
    g_open_gfx_transition.intensity = 0x80;
}

/* Empty. open_title_step_new_game_start calls it once the transition sequence has finished. */
void open_gfx_transition_finished_stub(void) {
}

/* Advance the new-game prologue by one frame: the picture crossfade and the text
 * of the current transition entry. Returns 1 while it runs and 0 when it has
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
s32 open_gfx_update_transition_sequence(void) {
    u16 flags;
    s32 i;
    open_gfx_transition_entry_t* entry;
    s32 image_command;

    open_gfx_step_image_crossfade();
    if (!(g_open_gfx_transition.flags & OPEN_GFX_TRANSITION_SKIP)) {
        if (g_open_input_new_button_presses & PSX_PAD_START) {
            main_sound_play_sfx(MAIN_SFX_COMPLETE);
            g_open_gfx_transition.flags |= OPEN_GFX_TRANSITION_SKIP;
        }
        flags = g_open_gfx_transition.flags;
        if (!(flags & OPEN_GFX_TRANSITION_SKIP)) {
            if (flags & OPEN_GFX_TRANSITION_DIM) {
                g_open_gfx_transition.intensity -= 2;
                open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
                if (g_open_gfx_transition.intensity == 0) {
                    g_open_gfx_transition.intensity = 0x80;
                    g_open_gfx_transition.flags
                        = (g_open_gfx_transition.flags ^ OPEN_GFX_TRANSITION_DIM) | OPEN_GFX_TRANSITION_NEXT_ENTRY;
                }
                return 1;
            }
            if (flags & OPEN_GFX_TRANSITION_WAIT_CIRCLE) {
                if (g_open_input_new_button_presses & PSX_PAD_CIRCLE) {
                    g_open_gfx_transition.flags = (flags ^ OPEN_GFX_TRANSITION_WAIT_CIRCLE) | OPEN_GFX_TRANSITION_DIM;
                    g_open_gfx_transition.intensity = 0x80;
                }
                open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
                return 1;
            }
            if (flags & OPEN_GFX_TRANSITION_NEXT_ENTRY) {
                flags ^= OPEN_GFX_TRANSITION_NEXT_ENTRY;
                g_open_gfx_transition.flags = flags;
                if (g_open_gfx_transition.current_index == g_open_gfx_transition.target_index) {
                    return 0;
                }
                g_open_gfx_transition.flags = flags | OPEN_GFX_TRANSITION_NEXT_STEP;
                entry = g_open_gfx_transition.entry_table[g_open_gfx_transition.current_index];
                g_open_gfx_transition.entry = entry;
                g_open_gfx_transition.step_count = (u8)entry->header;
                g_open_gfx_transition.tpage_x = (entry->header & 0xf00) >> 2;
                g_open_gfx_transition.tpage_y = (entry->header >> 4) & 0x100;
                /* The high nibble selects the operation; the low nibble selects its image. */
                image_command = (u8)(entry->header >> 16);
                switch (image_command & OPEN_GFX_TRANSITION_IMAGE_OPERATION_MASK) {
                case OPEN_GFX_TRANSITION_PREPARE_IMAGE:
                    open_gfx_copy_file_image_to_210x180_buffer(image_command & OPEN_GFX_TRANSITION_IMAGE_INDEX_MASK);
                    break;
                case OPEN_GFX_TRANSITION_START_CROSSFADE:
                    file_wait_for_pending_variant();
                    g_open_gfx_image_copy_row_flags[0] = 1;
                    break;
                }
                {
                    s16* index = &g_open_gfx_transition.current_index;

                    g_open_gfx_transition.step = 0;
                    (*index)++;
                }
            }
        }
    }

    flags = g_open_gfx_transition.flags;
    if (flags & OPEN_GFX_TRANSITION_NEXT_STEP) {
        g_open_gfx_transition.flags = flags ^ OPEN_GFX_TRANSITION_NEXT_STEP;
        /* Every line is out: the page waits for Circle. */
        if (g_open_gfx_transition.step == g_open_gfx_transition.step_count) {
            g_open_gfx_transition.flags = (flags ^ OPEN_GFX_TRANSITION_NEXT_STEP)
                | (OPEN_GFX_TRANSITION_NEXT_ENTRY | OPEN_GFX_TRANSITION_WAIT_CIRCLE);
            open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
            return 1;
        }
        /* Byte indexing preserves the retail address-add order and entry reloads. */
        if (g_open_gfx_transition.step == 0) {
            for (i = 0; i < g_open_gfx_transition.step_count; i++) {
                g_open_gfx_transition.line_widths[i]
                    = g_open_gfx_transition.entry->line_data[i * sizeof(open_gfx_transition_line_t)
                        + offsetof(open_gfx_transition_line_t, width)];
                g_open_gfx_transition.line_texture_rows[i]
                    = g_open_gfx_transition.entry->line_data[i * sizeof(open_gfx_transition_line_t)
                        + offsetof(open_gfx_transition_line_t, texture_row)];
            }
        }
        {
            s16* step = &g_open_gfx_transition.step;

            g_open_gfx_transition.timer = -0x10;
            (*step)++;
        }
    }
    if (++g_open_gfx_transition.timer == g_open_gfx_transition.line_widths[g_open_gfx_transition.step - 1]) {
        g_open_gfx_transition.flags |= OPEN_GFX_TRANSITION_NEXT_STEP;
    }
    if (g_open_gfx_transition.flags & OPEN_GFX_TRANSITION_SKIP) {
        if (g_open_gfx_transition.intensity != 0) {
            g_open_gfx_transition.intensity -= 2;
        } else if (g_open_gfx_image_copy_row_flags[0] == 0) {
            return 0;
        }
    }
    open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
    return 1;
}

/* Draw the text lines of the current new-game prologue entry at the given
 * brightness.
 *
 * Each step of the entry is one line of text: line_texture_rows holds its 16-pixel row
 * in the texture page and line_widths its width in pixels. The lines are
 * centerd, 20 pixels apart, as a block around the middle of the screen. A
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
void open_gfx_draw_transition_line_strips(s32 intensity) {
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

    for (i = 0; i < g_open_gfx_transition.step + 1; i++) {
        prim_index = i * 2;
        prims = g_open_gfx_transition_line_primitives[g_active_graphics_buffer_index];
        SetPolyGT4(&prims[prim_index]);
        SetPolyGT4(&prims[prim_index + 1]);

        if (i == g_open_gfx_transition.step) {
            /* Leading edge of the line in progress (line i - 1): black on its right. */
            if (g_open_gfx_transition.line_widths[i - 1] < g_open_gfx_transition.timer) {
                continue;
            }
            width = g_open_gfx_transition.line_widths[i - 1] - g_open_gfx_transition.timer;
            if (g_open_gfx_transition.timer < 0) {
                width = g_open_gfx_transition.timer + 0x10;
            } else if (width >= 0x11) {
                width = 0x10;
            }
            u = g_open_gfx_transition.timer;
            if (u < 0) {
                u = 0;
            }
            v = g_open_gfx_transition.line_texture_rows[i - 1] * 0x10;
            setRGB1(&prims[prim_index], 0, 0, 0);
            setRGB1(&prims[prim_index + 1], 0, 0, 0);
            setRGB3(&prims[prim_index], 0, 0, 0);
            setRGB3(&prims[prim_index + 1], 0, 0, 0);
            left = u - g_open_gfx_transition.line_widths[i - 1] / 2;
            top_y = (i - 1) * 0x14 - g_open_gfx_transition.step_count * 10;
        } else {
            /* Line i: the revealed part of the line in progress, or a whole finished line. */
            if (i == g_open_gfx_transition.step - 1) {
                width = g_open_gfx_transition.timer;
            } else {
                width = g_open_gfx_transition.line_widths[i];
            }
            u = 0;
            if (width <= 0) {
                continue;
            }
            v = g_open_gfx_transition.line_texture_rows[i] * 0x10;
            setRGB1(&prims[prim_index], intensity, intensity, intensity);
            setRGB1(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
            setRGB3(&prims[prim_index], intensity, intensity, intensity);
            setRGB3(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
            left = -(g_open_gfx_transition.line_widths[i] / 2);
            top_y = i * 0x14 - g_open_gfx_transition.step_count * 10;
        }

        setRGB0(&prims[prim_index], intensity, intensity, intensity);
        setRGB0(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
        setRGB2(&prims[prim_index], intensity, intensity, intensity);
        setRGB2(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);

        clut = GetClut(0, 0x1e3);
        prims[prim_index + 1].clut = clut;
        prims[prim_index].clut = clut;
        prims[prim_index].tpage = GetTPage(0, 1, g_open_gfx_transition.tpage_x, g_open_gfx_transition.tpage_y);
        prims[prim_index + 1].tpage = GetTPage(0, 2, g_open_gfx_transition.tpage_x, g_open_gfx_transition.tpage_y);

        setXY4(&prims[prim_index], left, top_y, left + width, top_y, left, top_y + 0x10, left + width, top_y + 0x10);
        width_plus_one = width + 1;
        setXY4(&prims[prim_index + 1], left + 1, top_y + 1, left + width_plus_one, top_y + 1, left + 1, top_y + 0x11,
            left + width_plus_one, top_y + 0x11);
        setUV4(&prims[prim_index], u, v, u + width, v, u, v + 0x10, u + width, v + 0x10);
        setUV4(&prims[prim_index + 1], u, v, u + width, v, u, v + 0x10, u + width, v + 0x10);

        SetSemiTrans(&prims[prim_index], 1);
        SetSemiTrans(&prims[prim_index + 1], 1);
        AddPrim(&g_open_gfx_otags[g_active_graphics_buffer_index][g_open_gfx_transition.ot_layer], &prims[prim_index]);
        AddPrim(&g_open_gfx_otags[g_active_graphics_buffer_index][g_open_gfx_transition.ot_layer + 1],
            &prims[prim_index + 1]);
    }
}

#define OPEN_IMAGE_COPY_WIDTH  210
#define OPEN_IMAGE_COPY_HEIGHT 180

/* Prepare the crossfade of the new-game prologue's picture to OPNBK image
 * file_index + 2.
 *
 * Points the crossfade at its three buffers, starts the asynchronous read of the
 * new image into g_open_work_buffer_0 (the crossfade's source) and copies the
 * 210x180 picture on screen out of the frame image into g_open_work_buffer_1,
 * packed 210 pixels per row. It clears the per-row fade steps, the running flag
 * (row_flags[0]) and the progress row (row_flags[1], the same byte as
 * g_open_gfx_image_copy_progress); image operation 0x20 of a later transition
 * entry waits for the read and sets the running flag. */
void open_gfx_copy_file_image_to_210x180_buffer(s32 file_index) {
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

    copy_source = (u16*)g_open_work_buffer_0;
    copy_destination = (u16*)g_open_work_buffer_1;
    file_image = g_open_file_destination;
    /* The 210x180 picture begins at row 43 of the 256-pixel frame image. */
    file_pixels = file_image[43];
    g_open_gfx_image_copy_source_buffer = copy_source;
    g_open_gfx_image_copy_destination_buffer = copy_destination;
    g_open_gfx_image_copy_file_pixels = file_pixels;
    open_file_build_openbk_header(file_index + 2, g_open_work_buffer_0);

    row = 0;
    source_row = 0;
    for (; row < OPEN_IMAGE_COPY_HEIGHT; row++) {
        column = 0;
        source_offset = row << 8;
        while (column < OPEN_IMAGE_COPY_WIDTH) {
            destination_index = source_row + column;
            source_index = source_offset + column++;
            destination_index <<= 1;
            pixels = g_open_gfx_image_copy_file_pixels;
            value = pixels[source_index];
            /* Destination indexing hoists pointer loads; byte-pointer arithmetic changes registers. */
            *(u16*)(destination_index + (u32)g_open_gfx_image_copy_destination_buffer) = value;
        }
        g_open_gfx_image_copy_row_flags[row + 2] = 0;
        source_row += OPEN_IMAGE_COPY_WIDTH;
    }

    g_open_gfx_image_copy_row_flags[0] = 0;
    g_open_gfx_image_copy_row_flags[1] = 0;
}

#define OPEN_IMAGE_WIDTH             210
#define OPEN_IMAGE_HEIGHT            180
#define OPEN_IMAGE_ROW_STRIDE        256
#define OPEN_IMAGE_BLEND_BAND_HEIGHT 17
#define OPEN_IMAGE_BLEND_END_ROW     (OPEN_IMAGE_HEIGHT + OPEN_IMAGE_BLEND_BAND_HEIGHT)

/*
 * Advance the bottom-up crossfade between the two 210x180 opening images by one row.
 *
 * Each call blends the 17-row band ending at the progress row: every row in the
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
void open_gfx_step_image_crossfade(void) {
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
    u8 fade;

    if (g_open_gfx_image_copy_row_flags[0] != 0) {
        row = g_open_gfx_image_copy_progress - OPEN_IMAGE_BLEND_BAND_HEIGHT;
        if (row < g_open_gfx_image_copy_progress) {
            last_row = OPEN_IMAGE_HEIGHT - 1;
            do {
                if (row >= 0 && row < OPEN_IMAGE_HEIGHT) {
                    reverse_row = last_row - row;
                    column = 0;
                    source_row_start = reverse_row * OPEN_IMAGE_WIDTH;
                    destination_row_start = reverse_row * OPEN_IMAGE_ROW_STRIDE;
                    fade = g_open_gfx_image_copy_row_flags[row + 2]++;
                    do {
                        /* An ordinary source-pointer load is hoisted and adds a saved-register frame. */
                        source_pixel = g_open_gfx_image_copy_source_buffer[source_row_start + column];
                        destination_pixel = g_open_gfx_image_copy_destination_buffer[source_row_start + column];
                        pixel = 0;
                        if (source_pixel != destination_pixel) {
                            for (component = 2; component >= 0; component--) {
                                previous_components = pixel >> 5;
                                source_component = source_pixel & 0x1F;
                                destination_component = destination_pixel & 0x1F;
                                if (source_component != destination_component) {
                                    difference = source_component - destination_component;
                                    pixel = previous_components
                                        | ((destination_component + (s16)((s16)(difference << 8) / 16 * fade) / 256)
                                            << 10);
                                } else {
                                    pixel = previous_components | (source_component << 10);
                                }
                                source_pixel >>= 5;
                                destination_pixel >>= 5;
                            }
                        } else {
                            pixel = source_pixel;
                        }
                        g_open_gfx_image_copy_file_pixels[destination_row_start + column]
                            = pixel | (destination_pixel & 0x8000);
                        column++;
                    } while (column < OPEN_IMAGE_WIDTH);
                }
                row++;
            } while (row < g_open_gfx_image_copy_progress);
        }
        {
            u8* progress = &g_open_gfx_image_copy_progress;

            if (++*progress >= OPEN_IMAGE_BLEND_END_ROW) {
                g_open_gfx_image_copy_row_flags[0] = 0;
            }
        }
    }
}
