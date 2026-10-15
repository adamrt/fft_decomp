/* Ending script state, text records, XA audio and opcodes. */

#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Resets the OPEN script/credits subsystem for a new sequence.
 *
 * Sets up both screen environments, clears VRAM, resets the script state and
 * the 16 script records, and initializes the two credit-band and two fade
 * POLY_FT4 primitives.
 *
 * The record-clear loop counts up; loop optimization reverses it into the
 * target's descending 0x164-stride offset with a bgez test. Stores are grouped
 * as the scheduler needs them: each screen environment's offset pair, and the
 * primitives' color, UV and XY fields in that order (the fade loop's XY
 * stores last keep 0x5a from being hoisted into a sixth saved register).
 */
void open_script_init_state(void) {
    RECT rect;
    s32 i;
    s32 j;
    POLY_FT4* prim;

    SetDefDrawEnv(&g_open_gfx_screen_environments[0].draw, 0, 0, 0x200, 0xf0);
    SetDefDispEnv(&g_open_gfx_screen_environments[0].disp, 0, 0xf0, 0x200, 0xf0);
    SetDefDrawEnv(&g_open_gfx_screen_environments[1].draw, 0, 0xf0, 0x200, 0xf0);
    SetDefDispEnv(&g_open_gfx_screen_environments[1].disp, 0, 0, 0x200, 0xf0);
    g_open_gfx_screen_environments[0].draw.ofs[0] = 0x100;
    g_open_gfx_screen_environments[0].draw.ofs[1] = 0x78;
    g_open_gfx_screen_environments[1].draw.ofs[0] = 0x100;
    g_open_gfx_screen_environments[1].draw.ofs[1] = 0x168;
    g_open_gfx_screen_environments[0].draw.isbg = g_open_gfx_screen_environments[1].draw.isbg = 0;
    g_open_gfx_overlay_fade.rect.x = -0x100;
    g_open_gfx_overlay_fade.rect.y = -0x78;
    g_open_gfx_overlay_fade.rect.w = 0x200;
    g_open_gfx_overlay_fade.rect.h = 0xf0;
    g_open_gfx_screen_environments[0].disp.isrgb24 = g_open_gfx_screen_environments[1].disp.isrgb24 = 0;
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x200;
    rect.h = 0x1e0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);

    g_open_script_state.dispatch.flags = OPEN_SCRIPT_DISPATCH_RUNNING;
    g_open_script_state.dispatch.tpage = (s16)GetTPage(0, 2, 0x300, 0);
    g_open_script_state.dispatch.clut = (s16)GetClut(0, 0x1e0);
    g_open_script_state._unknown_1674 = 0;
    g_open_script_state.dispatch.external_counter = 0;
    g_open_script_state.dispatch.timing_fraction = 0;
    g_open_script_state.dispatch.timing_step = 0;
    g_open_script_state.dispatch.font_index = 0;
    g_open_script_state.dispatch.wide_glyphs = 0;
    g_open_script_state.dispatch.skew = 0;
    g_open_script_state.dispatch.cursor.byte_offset = 0;
    g_open_script_state.dispatch.cursor.data_base = g_open_script_initial_data_base;
    for (j = 0; j < 16; j++) {
        g_open_script_state.records[j].flags = 0;
    }
    g_open_script_state.opntex.fade_intensity = 0;

    for (i = 0; i < 2; i++) {
        prim = &g_open_gfx_credit_band_primitives[i];
        SetPolyFT4(prim);
        SetSemiTrans(prim, 1);
        g_open_gfx_credit_band_primitives[i].tpage = GetTPage(0, 2, 0x300, 0);
        g_open_gfx_credit_band_primitives[i].clut = GetClut(0x10, 0x1e0);
        setRGB0(prim, 0x80, 0x80, 0x80);
    }

    setUV4(&g_open_gfx_credit_band_primitives[0], 0xf8, 0, 0xf9, 0, 0xf8, 0x30, 0xf9, 0x30);
    setXY4(&g_open_gfx_credit_band_primitives[0], -0x100, 0x48, 0x100, 0x48, -0x100, 0x78, 0x100, 0x78);
    setUV4(&g_open_gfx_credit_band_primitives[1], 0xfc, 0, 0xfd, 0, 0xfc, 0x30, 0xfd, 0x30);
    setXY4(&g_open_gfx_credit_band_primitives[1], -0x100, -0x78, 0x100, -0x78, -0x100, -0x48, 0x100, -0x48);

    for (i = 0; i < 2; i++) {
        prim = &g_open_gfx_fade_primitives[i];
        SetPolyFT4(prim);
        SetSemiTrans(prim, 1);
        g_open_gfx_fade_primitives[i].tpage = GetTPage(1, 2, 0x200, 0);
        g_open_gfx_fade_primitives[i].clut = GetClut(0, 0x1e1);
        setRGB0(prim, 0, 0, 0);
        setUV4(prim, 0, 0, 0xff, 0, 0, 0xb4, 0xff, 0xb4);
        setXY4(prim, -0x100, -0x5a, 0x100, -0x5a, -0x100, 0x5a, 0x100, 0x5a);
    }

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x100;
    rect.h = 0x100;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}

/* Run one frame of the ending script. Returns 0 once the script has ended and 1
 * while it runs.
 *
 * Updates the text scroll, the XA audio, the OPNTEX sequence and the picture
 * fade, then executes script entries. An entry is a row of 16-bit values at
 * data_base + byte_offset whose first value, the opcode, indexes
 * g_open_opcode_handlers. A handler advances byte_offset itself; one that has to
 * wait clears OPEN_SCRIPT_DISPATCH_CONTINUE to end the frame and is entered again
 * on the next, and opcode 0 clears OPEN_SCRIPT_DISPATCH_RUNNING. */
s32 open_script_init_and_dispatch_entries(void) {
    open_script_dispatch_state_t* state;
    open_script_cursor_t* cursor;
    s32 result;
    s16* entry;
    u32 flags;

    open_script_update_timing_and_record_values();
    open_script_update_xa_audio();
    open_gfx_update_opntex_sequence();
    open_script_update_screen_fade();

    state = &g_open_script_state.dispatch;
    result = state->flags | OPEN_SCRIPT_DISPATCH_CONTINUE;
    state->flags = result;
    if (result & OPEN_SCRIPT_DISPATCH_CONTINUE) {
        cursor = &state->cursor;
        do {
            entry = (s16*)open_script_add_pointer_offset(cursor->data_base, cursor->byte_offset);
            g_open_opcode_handlers[*entry](entry);
            /* Keeping the cursor base live matches retail; the direct state view is four bytes shorter. */
            flags = ((open_script_dispatch_state_t*)((u8*)cursor - offsetof(open_script_dispatch_state_t, cursor)))
                        ->flags;
            if ((flags & OPEN_SCRIPT_DISPATCH_RUNNING) == 0) {
                return 0;
            }
        } while (flags & OPEN_SCRIPT_DISPATCH_CONTINUE);
    }
    return 1;
}

u8* open_script_add_pointer_offset(u8* base, s32 byte_offset) {
    return base + byte_offset;
}

/* Queue the ending script's primitives for one frame while the script runs: the
 * OPNTEX picture quad (unless its fade intensity is 0), the two edge bands and
 * one POLY_FT4 per glyph of every active text record.
 *
 * Glyph positions are relative to the center of the 512x240 screen, and skew
 * shifts the top edge of each glyph. The AddPrim calls pass the contents of
 * otag[15] and otag[8], not their addresses: after ClearOTagR those are the
 * addresses of entries 14 and 7, which is where the primitives are linked. */
void open_script_draw_text_records(u32* otag, s32 graphics_buffer_index) {
    s32 i;
    s32 j;
    s32 prim_index;
    s32 x;
    s32 y;

    if (g_open_script_state.dispatch.flags & OPEN_SCRIPT_DISPATCH_RUNNING) {
        if (g_open_script_state.opntex.fade_intensity != 0) {
            AddPrim((void*)otag[15], &g_open_gfx_fade_primitives[graphics_buffer_index]);
        }
        AddPrim((void*)otag[8], &g_open_gfx_credit_band_primitives[0]);
        AddPrim((void*)otag[8], &g_open_gfx_credit_band_primitives[1]);
        for (i = 0; i < 16; i++) {
            if (g_open_script_state.records[i].flags & OPEN_SCRIPT_RECORD_ACTIVE) {
                prim_index = i * 42;
                for (j = 0; j < g_open_script_state.records[i].glyph_count; j++, prim_index++) {
                    x = g_open_script_state.records[i].x + g_open_script_state.records[i].glyphs[j].x - 256;
                    y = g_open_script_state.records[i].y + g_open_script_state.records[i].glyphs[j].y - 120;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].x0
                        = x + g_open_script_state.records[i].skew;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].y0 = y;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].x1
                        = g_open_script_state.records[i].glyphs[j].w + x + g_open_script_state.records[i].skew;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].y1 = y;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].x2 = x;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].y2
                        = g_open_script_state.records[i].glyphs[j].h + y;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].x3
                        = g_open_script_state.records[i].glyphs[j].w + x;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].y3
                        = g_open_script_state.records[i].glyphs[j].h + y;
                    AddPrim((void*)otag[8], &g_open_script_glyph_primitives[graphics_buffer_index][prim_index]);
                }
            }
        }
    }
}

/* Lays out one OPEN script text line as textured glyph quads.
 *
 * Claims the first free script record, then walks the text until '@' or 42
 * glyphs. Escape characters followed by one hex digit select the font
 * ('$'), CLUT ('^'), pen adjustments ('<', '>', '+'), the record field at
 * 0x08 ('/') and wide-glyph mode ('*'). Every other character emits a glyph
 * record and a POLY_FT4 in both graphics buffers. Returns the record index
 * plus 1, or 0 when all 16 records are busy.
 *
 * `value` holds both the decoded escape digits and the glyph metrics offset:
 * the extra references give it the target's global-allocation priority
 * (s3, ahead of the loop counter). The second metrics pointer is a separate
 * variable so the first stays in a0.
 */
s32 open_script_build_text_record(u8* text) {
    s32 index;
    s32 i;
    s32 drawn;
    s32 pen;
    s32 width;
    s32 height;
    open_font_metrics_t* font;
    open_font_metrics_t* mt;
    open_font_metrics_t* mt2;
    s32 value;
    s32 slot;
    POLY_FT4* back;
    POLY_FT4* front;
    s32 u0;
    s32 v0;
    u8 ch;
    u16 tpage;

    font = g_open_script_font_metrics;
    for (i = 0; i < 16; i++) {
        if (!(g_open_script_state.records[i].flags & OPEN_SCRIPT_RECORD_ACTIVE)) {
            break;
        }
    }
    if (i == 16) {
        return 0;
    }
    index = i;

    g_open_script_state.records[index].flags = OPEN_SCRIPT_RECORD_ACTIVE;
    pen = 0;
    slot = index * 42;
    drawn = 0;
    for (i = 0; i < 42; i++) {
        ch = *text;
        if (ch == '@') {
            break;
        }
        if (ch == '$') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.font_index = value;
            i--;
        } else if (ch == '^') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.clut = GetClut(value * 16, 0x1e0);
            i--;
        } else if (ch == '<') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            pen -= value;
            i--;
        } else if (ch == '>') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            pen += value;
            i--;
        } else if (ch == '/') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.skew = value;
            i--;
        } else if (ch == '*') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.wide_glyphs = value;
            i--;
        } else if (ch == '+') {
            text++;
            pen = open_text_decode_hex_digit(*text++) * 20;
            i--;
        } else {
            value = (ch - 0x20) * 8 + g_open_script_state.dispatch.font_index * 0x380;
            /* Typed pointer addition reverses this retail addu's operands. */
            mt = (open_font_metrics_t*)(value + (s32)font);
            if (mt->flags & 1) {
                text++;
                i--;
                pen += mt->width;
            } else {
                g_open_script_state.records[index].glyphs[drawn].x = pen - mt->bearing;
                g_open_script_state.records[index].glyphs[drawn].y = -mt->descender;
                width = mt->bearing + mt->width;
                height = mt->height + mt->descender - 1;
                g_open_script_state.records[index].glyphs[drawn].w = width;
                g_open_script_state.records[index].glyphs[drawn].h = height;
                if (g_open_script_state.dispatch.wide_glyphs == 0) {
                    g_open_script_state.records[index].glyphs[drawn].x
                        = g_open_script_state.records[index].glyphs[drawn].x * 2;
                    g_open_script_state.records[index].glyphs[drawn].w
                        = g_open_script_state.records[index].glyphs[drawn].w * 2;
                } else {
                    g_open_script_state.records[index].glyphs[drawn].x
                        = g_open_script_state.records[index].glyphs[drawn].x
                        + g_open_script_state.records[index].glyphs[drawn].x / 2;
                    g_open_script_state.records[index].glyphs[drawn].w
                        = g_open_script_state.records[index].glyphs[drawn].w
                        + g_open_script_state.records[index].glyphs[drawn].w / 2;
                }
                front = &g_open_script_glyph_primitives[0][slot];
                g_open_script_state.records[index].skew = g_open_script_state.dispatch.skew;
                SetPolyFT4(front);
                back = &g_open_script_glyph_primitives[1][slot];
                SetPolyFT4(back);
                SetSemiTrans(front, 1);
                SetSemiTrans(back, 1);
                tpage = g_open_script_state.dispatch.tpage;
                front[16 * 42].tpage = tpage;
                g_open_script_glyph_primitives[0][slot].tpage = tpage;
                drawn++;
                tpage = g_open_script_state.dispatch.clut;
                front[16 * 42].clut = tpage;
                g_open_script_glyph_primitives[0][slot].clut = tpage;
                setRGB0(front, 0x80, 0x80, 0x80);
                setRGB0(back, 0x80, 0x80, 0x80);
                mt2 = font + (value >> 3);
                u0 = mt2->u;
                v0 = mt2->v - mt2->height - mt2->descender + 1;
                setUV4(front, u0, v0, u0 + width, v0, u0, v0 + height, u0 + width, v0 + height);
                setUV4(back, u0, v0, u0 + width, v0, u0, v0 + height, u0 + width, v0 + height);
                text++;
                slot++;
                pen += mt2->advance;
            }
        }
    }
    g_open_script_state.records[index].glyph_count = drawn;
    return index + 1;
}

/* Scroll the ending script's text.
 *
 * timing_step is the speed in 1/256 pixel per frame; a negative step scrolls up.
 * The whole pixels moved are added to the y of every active text record and, as a
 * distance, to the external counter opcode 6 waits on. A record that leaves y
 * 0x10..0x100 is released. */
void open_script_update_timing_and_record_values(void) {
    s32 step;
    s32 work;
    s32 index;

    step = g_open_script_state.dispatch.timing_step;
    if (step == 0) {
        return;
    }

    if (step > 0) {
        work = step + g_open_script_timing_fraction;
        g_open_script_timing_fraction = work & 0xff;
        work >>= 8;
        g_open_script_external_counter += work;
    } else {
        work = g_open_script_timing_fraction - step;
        g_open_script_timing_fraction = work & 0xff;
        work >>= 8;
        g_open_script_external_counter += work;
        work = -work;
    }

    for (index = 0; index < 16; index++) {
        if (g_open_script_state.records[index].flags & OPEN_SCRIPT_RECORD_ACTIVE) {
            g_open_script_state.records[index].y += work;
            if (g_open_script_state.records[index].y < 0x10 || g_open_script_state.records[index].y > 0x100) {
                g_open_script_state.records[index].flags ^= OPEN_SCRIPT_RECORD_ACTIVE;
            }
        }
    }
}

/* The last sector of OPEN/ENDING.XA (DISC_SECTOR_OPEN_ENDING_XA is its first). */
#define ENDING_XA_LAST_SECTOR 0x3586c

/* Runs the XA audio state machine once per frame.
 *
 * Starting: sets the drive mode to 0x68 (real-time ADPCM, sub-header filter,
 * 2340-byte sectors at single speed), seeks to ENDING.XA, sets the filter to
 * file 1 and the channel in control_parameter (command 0xd, CdlSetfilter),
 * starts a streaming read and waits for it to be acknowledged; control_state
 * counts the steps and open_file_get_cd_sync_state_delta advances it past
 * each command and back one step on a disc error. A seek (step 5) still
 * unfinished after 120 frames resets the CD subsystem and starts over.
 * Playing: from two seconds in, restarts the sequence if the drive stopped
 * reading, and stops once the head (command 0x10, CdlGetlocL) has passed the
 * file's last sector.
 * Stopping: pauses the drive, restores double speed and brings the CD
 * volume back for the streams. */
void open_script_update_xa_audio(void) {
    u8 result[8];
    CdlFILTER filter;
    u8 mode;

    if (g_open_script_state.dispatch.flags & OPEN_SCRIPT_DISPATCH_XA_STARTING) {
        switch (g_open_script_state.control_state) {
        case 0:
            mode = CdlModeRT | CdlModeSF | CdlModeSize1;
            CdControlF(CdlSetmode, &mode);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 4:
            CdIntToPos(DISC_SECTOR_OPEN_ENDING_XA, &g_main_file_cd_state.position);
            CdControlF(CdlSeekL, (u8*)&g_main_file_cd_state.position);
            {
                s32* state = &g_open_script_state.control_state;

                g_main_system_session_frames = 0;
                (*state)++;
            }
            break;
        case 5: {
            s32 delta = open_file_get_cd_sync_state_delta();
            s32* state = &g_open_script_state.control_state;
            s32 value = *state;

            *state = value + delta;
            if (g_main_system_session_frames >= 0x79 && *state == 5) {
                main_file_reset_cd_subsystems();
                *state = 0;
            }
        } break;
        case 6:
            filter.file = 1;
            {
                s32 parameter = g_open_script_state.control_parameter;

                filter.chan = parameter;
            }
            CdControlF(CdlSetfilter, (u8*)&filter);
            g_open_script_state.control_state++;
            break;
        case 1:
        case 7: {
            s32 delta = open_file_get_cd_sync_state_delta();
            s32* state = &g_open_script_state.control_state;
            s32 value = *state;

            *state = value + delta;
        } break;
        case 8:
            CdControlF(CdlReadS, 0);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 9:
            if (open_file_get_cd_sync_state_delta() == 1) {
                u32* control = &g_open_script_state.dispatch.flags;

                g_open_script_state.xa_wait_frames = 0;
                *control = (*control | OPEN_SCRIPT_DISPATCH_XA_PLAYING) ^ OPEN_SCRIPT_DISPATCH_XA_STARTING;
            }
            break;
        default: {
            s32* state = &g_open_script_state.control_state;

            (*state)++;
        } break;
        }
    }

    {
        u32* control = &g_open_script_state.dispatch.flags;

        if (*control & OPEN_SCRIPT_DISPATCH_XA_PLAYING) {
            g_open_script_state.xa_wait_frames++;
            if (g_open_script_state.xa_wait_frames < 0x78) {
                return;
            }
            if (!(CdStatus() & CdlStatRead)) {
                g_open_script_state.control_state = 0;
                *control = (*control ^ OPEN_SCRIPT_DISPATCH_XA_PLAYING) | OPEN_SCRIPT_DISPATCH_XA_STARTING;
                return;
            }
            if (CdControl(CdlGetlocL, 0, result) != 0 && CdPosToInt((CdlLOC*)result) > ENDING_XA_LAST_SECTOR) {
                open_sound_set_cd_mix_and_volume(0, 1);
                g_open_script_state.control_state = 0;
                *control = (*control ^ OPEN_SCRIPT_DISPATCH_XA_PLAYING) | OPEN_SCRIPT_DISPATCH_XA_STOPPING;
                return;
            }
        }
    }

    if (g_open_script_state.dispatch.flags & OPEN_SCRIPT_DISPATCH_XA_STOPPING) {
        switch (g_open_script_state.control_state) {
        case 0:
            CdControlF(CdlPause, 0);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 2:
            mode = CdlModeSpeed;
            CdControlF(CdlSetmode, &mode);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 1:
        case 3: {
            s32 delta = open_file_get_cd_sync_state_delta();
            s32* state = &g_open_script_state.control_state;
            s32 value = *state;

            *state = value + delta;
        } break;
        case 6: {
            u32* control = &g_open_script_state.dispatch.flags;

            *control ^= OPEN_SCRIPT_DISPATCH_XA_STOPPING;
            open_sound_set_cd_mix_and_volume(0xc0, 1);
        } break;
        default: {
            s32* state = &g_open_script_state.control_state;

            (*state)++;
        } break;
        }
    }
}

/* Value of a hexadecimal digit character, or 0 for any other character. */
s32 open_text_decode_hex_digit(s32 character) {
    u32 value;

    value = character - '0';
    if (value < 10) {
        return value;
    }

    if (character >= 'A' && character < 'G') {
        return character - ('A' - 10);
    }

    if (character >= 'a' && character < 'g') {
        return character - ('a' - 10);
    }

    return 0;
}

/*
 * Advance the script-controlled screen fade and tint the active fade quad.
 * Block-local control pointers retain the original materialized flag addresses.
 */
void open_script_update_screen_fade(void) {
    s32 intensity;

    {
        u32* control = &g_open_script_state.dispatch.flags;
        u32 flags;

        flags = *control;
        if (flags & OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY) {
            s32 elapsed_frames = g_open_script_state.opntex.fade_elapsed_frames + 1;
            s32 duration_frames = g_open_script_state.opntex.fade_duration_frames;

            intensity = (elapsed_frames << 7) / duration_frames;
            g_open_script_state.opntex.fade_elapsed_frames = elapsed_frames;
            if (intensity >= 0x81) {
                intensity = 0x80;
            }
            g_open_script_state.opntex.fade_intensity = intensity;
            if (elapsed_frames >= duration_frames) {
                g_open_script_state.opntex.fade_intensity = 0x80;
                *control = flags ^ OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY;
            }
        }
    }
    {
        u32* control = &g_open_script_state.dispatch.flags;
        u32 flags;

        flags = *control;
        if (flags & OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY) {
            s32 elapsed_frames = g_open_script_state.opntex.fade_elapsed_frames + 1;
            s32 duration_frames = g_open_script_state.opntex.fade_duration_frames;

            g_open_script_state.opntex.fade_intensity = 0x80;
            g_open_script_state.opntex.fade_elapsed_frames = elapsed_frames;
            intensity = 0x80 - (elapsed_frames << 7) / duration_frames;
            if (intensity < 0) {
                intensity = 0;
            }
            g_open_script_state.opntex.fade_intensity = intensity;
            if (elapsed_frames >= duration_frames) {
                g_open_script_state.opntex.fade_intensity = 0;
                *control = flags ^ OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY;
            }
        }
    }

    intensity = g_open_script_state.opntex.fade_intensity;
    setRGB0(&g_open_gfx_fade_primitives[g_active_graphics_buffer_index], intensity, intensity, intensity);
}

/* Advance the OPNTEX sequence: run down the fade delay of the frame on
 * screen, and when the frame countdown expires load the next frame and start
 * its fade (or stop the sequence after the last frame). */
void open_gfx_update_opntex_sequence(void) {
    s32 frame;
    u32* source;

    if (g_open_script_state.opntex.frame_duration == 0) {
        return;
    }

    if (g_open_script_state.opntex.delay != 0) {
        g_open_script_state.opntex.delay--;
        if (g_open_script_state.opntex.delay == 0) {
            g_open_script_state.dispatch.flags |= OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY;
            g_open_script_state.opntex.fade_intensity = 0x80;
            g_open_script_state.opntex.fade_elapsed_frames = 0;
            g_open_script_state.opntex.fade_duration_frames = g_open_script_state.opntex_control_value;
        }
    }

    if (--g_open_script_state.opntex.frame_countdown != 0) {
        return;
    }

    frame = g_open_script_state.opntex.first_frame;
    if (g_open_script_state.opntex.last_frame < frame) {
        g_open_script_state.opntex.frame_duration = 0;
        return;
    }

    source = g_open_gfx_opntex_data;
    source += frame * 23 * 512;
    open_gfx_load_opntex_into_frame_buffer(source);
    g_open_script_state.dispatch.flags |= OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY;
    g_open_script_state.opntex.fade_elapsed_frames = 0;
    g_open_script_state.opntex.fade_intensity = 0;
    g_open_script_state.opntex.fade_duration_frames = g_open_script_state.opntex_control_value;
    g_open_script_state.opntex.first_frame++;
    g_open_script_state.opntex.frame_countdown = g_open_script_state.opntex.frame_duration;
    g_open_script_state.opntex.delay
        = g_open_script_state.opntex.frame_duration - g_open_script_state.opntex_control_value;
}

/* Start FFTEND.STR (sector 0x21908, ending at frame 0x571) as the first
 * controller. */
void open_movie_play_end(void) {
    g_open_current_controller_index = 0;
    g_active_graphics_buffer_index = 0;
    open_movie_stream_fftend(DISC_SECTOR_OPEN_FFTEND_STR, 0x3858, 0x571);
}

/* Start the ending movie at CD mix level 0x7e and push handler 9. The second
 * argument, the stream's length in sectors, is not read. */
void open_movie_stream_fftend(s32 start_sector, s32 unused, s32 end_frame) {
    s32 controller;

    (void)unused;
    open_movie_start_stream(start_sector, end_frame, -1, 0x7e);
    g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
    controller = g_open_current_controller_index;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_FFTEND_MOVIE;
    g_open_current_controller_index = controller + 1;
}

/* Handler 9, the FFTEND.STR ending movie: Start stops the stream. Once the stream
 * has stopped, by itself or on the update after Start, the display is blanked and
 * the controller is replaced by the ending script. */
void open_movie_finish_fftend_controller_if_ready_or_skipped(open_controller_record_t* unused_record) {
    u32 flags;
    s32 controller;

    if (!(g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING)) {
        SetDispMask(0);
        flags = g_open_system_runtime_flags;
        controller = g_open_current_controller_index;
        flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        controller--;
        g_open_system_runtime_flags = flags;
        g_open_current_controller_index = controller;
        open_script_push_controller();
        return;
    }

    if (g_open_input_new_button_presses & PSX_PAD_START) {
        open_movie_stop_stream(1);
    }
}

/* Start the ending script: leave the 320-pixel and 24-bit modes, make each
 * frame's background the picture copied from VRAM
 * (OPEN_SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM), reset the script state, which sets
 * up the 512-pixel screen, select VSync mode 2 and push handler 10. */
void open_script_push_controller(void) {
    s32 controller;

    g_open_system_runtime_flags &= ~(OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN | OPEN_SYSTEM_RUNTIME_FLAG_RGB24);
    open_gfx_init_screen_environments(1);
    g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM;
    open_script_init_state();

    controller = g_open_current_controller_index;
    g_open_system_vsync_mode = 2;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_SCRIPT;
    g_open_current_controller_index = controller + 1;
}

/* Handler 10: run the ending script for a frame. When it ends, VSync mode 0 is
 * restored, the controller is popped and OPEN.BIN's loop ends. */
void open_script_update_controller(open_controller_record_t* unused_record) {
    if (open_script_init_and_dispatch_entries() == 0) {
        g_open_system_vsync_mode = 0;
        g_open_current_controller_index--;
        g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_RUNNING;
    }
}

/* Opcode 0: end the script. It yields for the frame while the XA start or stop
 * sequence runs; after that it clears OPEN_SCRIPT_DISPATCH_RUNNING, which stops
 * the dispatcher. Two bytes. */
void open_opcode_toggle_control(s16* unused_entry) {
    u32 state = g_open_script_state.dispatch.flags;

    if (state & (OPEN_SCRIPT_DISPATCH_XA_STOPPING | OPEN_SCRIPT_DISPATCH_XA_STARTING)) {
        g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
    } else {
        g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_RUNNING;
        g_open_script_state.dispatch.cursor.byte_offset += 2;
    }
}

/* Opcode 1: create and position a text record from an ending-script command
 * (opcode, x, y, text offset). Eight bytes.
 *
 * A coordinate of -1 centers the record on its last glyph's extent. X bit 0x100
 * instead centers the glyph selected by its low byte. */
void open_opcode_create_positioned_text_record(s16* command) {
    s32 x;
    s32 y;
    s32 index;

    command++;
    x = *command++;
    y = *command++;
    index = open_script_build_text_record(
        open_script_add_pointer_offset(g_open_script_state.dispatch.cursor.data_base, *command));
    if (index != 0) {
        index--;
        if (x == -1) {
            s32 last = g_open_script_state.records[index].glyph_count - 1;

            x = 256
                - (g_open_script_state.records[index].glyphs[last].x
                      + g_open_script_state.records[index].glyphs[last].w)
                    / 2;
        }
        if (y == -1) {
            s32 last = g_open_script_state.records[index].glyph_count - 1;

            y = 120
                - (g_open_script_state.records[index].glyphs[last].y
                      + g_open_script_state.records[index].glyphs[last].h)
                    / 2;
        }
        if (x & 0x100) {
            s32 glyph = x & 0xFF;

            x = 0x100 - g_open_script_state.records[index].glyphs[glyph].x
                - g_open_script_state.records[index].glyphs[glyph].w / 2;
        }
        g_open_script_state.records[index].x = x;
        g_open_script_state.records[index].y = y;
    }
    g_open_script_state.dispatch.cursor.byte_offset += 8;
}

/* Opcode 2: set the text scroll speed to command[1] (1/256 pixel per frame,
 * negative for upward) and zero the fraction and the scrolled-distance counter.
 * Four bytes. */
void open_opcode_set_timing_step(s16* entry) {
    const s16* command = entry;
    s32 value = command[1];
    s32 next = g_open_script_byte_offset + 4;

    g_open_script_state.dispatch.external_counter = 0;
    g_open_script_state.dispatch.timing_fraction = 0;
    g_open_script_state.dispatch.timing_step = value;
    g_open_script_byte_offset = next;
}

/* Opcode 3: reveal the screen over command[1] frames, with the overlay in
 * ordering-table entry 2. Four bytes. */
void open_opcode_start_overlay_fade_out(s16* command) {
    command++;
    g_open_gfx_overlay_fade.ot_index = 2;
    open_gfx_start_overlay_fade_out(*command);
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}

/* Opcode 4: fade the screen to black over command[1] frames, with the overlay in
 * ordering-table entry 2. Four bytes. */
void open_opcode_start_overlay_fade_in(s16* command) {
    command++;
    g_open_gfx_overlay_fade.ot_index = 2;
    open_gfx_start_overlay_fade_in(*command);
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}

/* Opcode 5: wait command[1] frames. The first call arms the wait and each later
 * call counts one frame, yielding until the count is reached. Four bytes. */
void open_opcode_wait_counted(s16* entry) {
    u32 flags;
    s32 counter;
    open_script_dispatch_state_t* state;

    state = &g_open_script_state.dispatch;
    flags = state->flags;
    entry++;
    if (flags & OPEN_SCRIPT_DISPATCH_WAITING) {
        counter = state->wait_counter + 1;
        state->wait_counter = counter;
        if (counter >= *entry) {
            state->flags = flags ^ OPEN_SCRIPT_DISPATCH_WAITING;
            state->cursor.byte_offset = state->cursor.byte_offset + 4;
        } else {
            state->flags = flags ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        }
    } else {
        state->flags = (flags | OPEN_SCRIPT_DISPATCH_WAITING) ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        state->wait_counter = 0;
    }
}

/* Opcode 6: wait until the text has scrolled command[1] pixels, counted from the
 * call that arms the wait. Four bytes. */
void open_opcode_wait_for_external_counter(s16* entry) {
    u32 flags;
    open_script_dispatch_state_t* state;

    state = &g_open_script_state.dispatch;
    flags = state->flags;
    entry++;
    if (flags & OPEN_SCRIPT_DISPATCH_WAITING) {
        if (state->external_counter >= *entry) {
            state->flags = flags ^ OPEN_SCRIPT_DISPATCH_WAITING;
            state->cursor.byte_offset = state->cursor.byte_offset + 4;
        } else {
            state->flags = flags ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        }
    } else {
        state->flags = (flags | OPEN_SCRIPT_DISPATCH_WAITING) ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        state->external_counter = 0;
    }
}

/* Opcode 7: start channel command[1] of ENDING.XA and wait until the start
 * sequence of open_script_update_xa_audio has finished. The CD volume is dropped
 * to 0 first. The opcode is skipped when XA audio is already playing, starting or
 * stopping. Four bytes. */
void open_opcode_start_xa_audio(s16* command) {
    u32 flags = g_open_script_state.dispatch.flags;

    if (flags & OPEN_SCRIPT_DISPATCH_WAITING) {
        if (flags & OPEN_SCRIPT_DISPATCH_XA_STARTING) {
            g_open_script_state.dispatch.flags = flags ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
            return;
        }
        g_open_script_state.dispatch.flags = flags ^ OPEN_SCRIPT_DISPATCH_WAITING;
        g_open_script_state.dispatch.cursor.byte_offset += 4;
        return;
    }
    if (flags
        & (OPEN_SCRIPT_DISPATCH_XA_PLAYING | OPEN_SCRIPT_DISPATCH_XA_STOPPING | OPEN_SCRIPT_DISPATCH_XA_STARTING)) {
        g_open_script_state.dispatch.cursor.byte_offset += 4;
        return;
    }
    g_open_script_state.control_parameter = command[1];
    g_open_script_state.control_state = 0;
    g_open_script_state.dispatch.flags = (flags
                                             & ~(OPEN_SCRIPT_DISPATCH_CONTINUE | OPEN_SCRIPT_DISPATCH_XA_PLAYING
                                                 | OPEN_SCRIPT_DISPATCH_XA_STOPPING | OPEN_SCRIPT_DISPATCH_XA_STARTING))
        | (OPEN_SCRIPT_DISPATCH_WAITING | OPEN_SCRIPT_DISPATCH_XA_STARTING);
    open_sound_set_cd_mix_and_volume(0, 1);
}

/* Opcode 8: fade the CD audio in at mix level 0x7a over command[1] * 4 ticks.
 * Four bytes.
 *
 * `command` is not const, matching the script handler type: GCC 2.7.2 marks a
 * load through a const pointer unchanging and would schedule it above the
 * prologue's ra save. */
void open_opcode_start_sound_fade_in(s16* command) {
    open_sound_set_cd_mix_and_volume(0x7a, command[1] * 4);
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}

/* Opcode 9: fade the CD audio out over command[1] * 4 ticks. Four bytes.
 *
 * `command` is not const, matching the script handler type: GCC 2.7.2 marks a
 * load through a const pointer unchanging and would schedule it above the
 * prologue's ra save. */
void open_opcode_start_sound_fade_out(s16* command) {
    open_sound_set_cd_mix_and_volume(0, command[1] * 4);
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}

/* Opcode 10: stop the XA audio and wait until the stop sequence of
 * open_script_update_xa_audio has finished. The opcode is skipped when XA audio
 * is neither playing nor starting. Two bytes. */
void open_opcode_stop_xa_audio(s16* unused_entry) {
    u32 state = g_open_script_state.dispatch.flags;

    if (state & OPEN_SCRIPT_DISPATCH_WAITING) {
        if (state & OPEN_SCRIPT_DISPATCH_XA_STOPPING) {
            g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        } else {
            g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_WAITING;
            g_open_script_state.dispatch.cursor.byte_offset += 2;
        }
    } else if (!(state & (OPEN_SCRIPT_DISPATCH_XA_PLAYING | OPEN_SCRIPT_DISPATCH_XA_STARTING))) {
        g_open_script_state.dispatch.cursor.byte_offset += 2;
    } else {
        g_open_script_state.control_state = 0;
        g_open_script_state.dispatch.flags
            = (state
                  & ~(OPEN_SCRIPT_DISPATCH_CONTINUE | OPEN_SCRIPT_DISPATCH_XA_PLAYING | OPEN_SCRIPT_DISPATCH_XA_STOPPING
                      | OPEN_SCRIPT_DISPATCH_XA_STARTING))
            | (OPEN_SCRIPT_DISPATCH_WAITING | OPEN_SCRIPT_DISPATCH_XA_STOPPING);
        open_sound_set_cd_mix_and_volume(0, 1);
    }
}
