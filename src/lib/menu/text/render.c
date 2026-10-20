/* Text-bank setup, input snapshots and drawing support for script-driven displays.
 * Numeric entries use the same glyph-image destinations as the event menus. */

#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/script.h"
#include "fft/text.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Text variables $7E/$7F: the rectangle text_set_substitution_values_7e_7f stores as four 16-bit values. It
 * lies at g_text_substitution_values[0x7e]. */
extern RECT g_text_substitution_rect;
extern glyph_blit_t g_menu_large_number_glyph;

/* BATTLE leaves the last section alone. */
#ifdef FFT_BATTLE
#    define TEXT_RELOCATED_SECTION_COUNT (TEXT_SECTION_COUNT - 1)
#else
#    define TEXT_RELOCATED_SECTION_COUNT TEXT_SECTION_COUNT
#endif

/* Turn the text file's table of section offsets into g_text_section_pointers.
 * The text begins after the table of TEXT_SECTION_COUNT offsets, which are
 * relative to its end. BATTLE leaves the last section alone and, while menu
 * input is disabled, keeps section 0 bound to the event or tutorial text
 * already in use. */
void text_init_section_pointers(void) {
    u32* source;
    u8** destination;
    u32* text;
#ifdef FFT_BATTLE
    s32 menu_input_mode;
#endif
    s32 index;
    u32 offset;

    source = g_text_section_offsets;
    index = 0;
#ifdef FFT_BATTLE
    menu_input_mode = g_menu_input_mode;
#endif
    /* These two assignments fill the target load delay before the loop. */
    text = source + TEXT_SECTION_COUNT;
    destination = g_text_section_pointers;
    do {
#ifdef FFT_BATTLE
        if (menu_input_mode == MENU_INPUT_PLAYER || index != TEXT_SECTION_EVENT_MESSAGES)
#endif
        {
            offset = *source;
            /* Integer addition retains the native addu operand order. */
            offset += (u32)text;
            *destination = (u8*)offset;
        }
        source++;
        index++;
        destination++;
    } while (index < TEXT_RELOCATED_SECTION_COUNT);
}

/* Return a pointer to the event input's newly pressed buttons, or to its held buttons when
 * use_current is nonzero. */
u32* script_get_controller_input_pointer(s32 use_current) {
    u32* result = &g_script_event_input;
    if (use_current != 0) {
        result = &g_script_event_current_input;
    }
    return result;
}

/* Copy a saved input state of six 32-bit values (pressed, held and previous buttons and the
 * three repeat counters) into the event input globals. The BUNIT and CARD
 * overlays call it after computing the state themselves. */
void script_load_event_input_state(const menu_input_state_t* saved) {
    g_script_event_input = saved->new_buttons;
    g_script_event_current_input = saved->current_buttons;
    g_script_event_previous_input = saved->previous_buttons;
    g_script_event_initial_repeat_counter = saved->initial_repeat_counter;
    g_script_event_repeat_counter = saved->repeat_counter;
    g_script_event_secondary_repeat_counter = saved->secondary_repeat_counter;
}

/* Draw a GPU primitive at once with DrawPrim when immediate drawing is on, otherwise link it into
 * the current ordering-table entry. */
void gfx_draw_or_append_gpu_primitive(void* primitive) {
    if (g_gfx_draw_primitives_immediately != 0) {
        DrawPrim(primitive);
        return;
    }
    addPrim(g_gfx_current_otag_entry, primitive);
}

/* Link a GPU primitive into the ordering-table entry after the current one (libgpu addPrim). */
void gfx_append_gpu_primitive_to_secondary_otag(void* prim) {
    u32* entries = g_gfx_current_otag_entry;

    addPrim(&entries[1], prim);
}

/* Store the rectangle text variables $7E and $7F substitute. */
void text_set_substitution_values_7e_7f(s32 x, s32 y, s32 width, s32 height) {
    g_text_substitution_rect.x = x;
    g_text_substitution_rect.y = y;
    g_text_substitution_rect.w = width;
    g_text_substitution_rect.h = height;
}

/* Set the screen origin used by menu text glyphs. */
void menu_set_text_origin(s32 x, s32 y) {
    g_menu_window_text_pen.rect.x = x;
    g_menu_window_text_pen.rect.y = y;
}

/* Set a draw mode whose texture page is the VRAM area of the given menu texture page
 * (menu_texture_page_e); any page above 3 selects the item icon page at (896, 288). */
void gfx_set_draw_mode_for_texture_page(DR_MODE* mode, s32 page) {
    if (page == MENU_TEXTURE_PAGE_FRAME) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x3c0, 0x100), &g_gfx_texture_window);
    } else if (page == MENU_TEXTURE_PAGE_ALLOCATED_IMAGES) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x1c0, 0), &g_gfx_texture_window);
    } else if (page == MENU_TEXTURE_PAGE_TEXT_IMAGES) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x180, 0), &g_gfx_texture_window);
    } else if (page == MENU_TEXTURE_PAGE_UNIT_SPRITES) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x340, 0x100), &g_gfx_texture_window);
    } else {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x380, 0x120), &g_gfx_texture_window);
    }
}

/* Set a draw mode from aligned texture-page coordinates from the menu text image origin. */
void gfx_set_draw_mode_from_origin(DR_MODE* mode, const point16_t* origin) {
    SetDrawMode(mode, 0, 0, GetTPage(0, 0, origin->x & ~0x3f, origin->y & ~0xff), &g_gfx_texture_window);
}

/* Returns 0 in BATTLE and WORLD; in BATTLE it sits between
 * gfx_set_draw_mode_from_origin and script_noop_10. MAIN calls BATTLE's copy
 * through overlay_call_battle_return_zero while script variable 0xff is
 * set. Nothing else references it; the name is a neutral placeholder until a
 * role turns up. */
s32 script_return_zero_1(void) {
    return 0;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_return_zero_1
 * and script_frame_after_requests_stub. Nothing on the disc calls it or points
 * at it; the name is a neutral placeholder until a reference turns up. */
void script_noop_10(void) {
}

/* Empty. The event frame runner calls it each frame after the pending requests. */
void script_frame_after_requests_stub(void) {
}

/* Point a sprite (SPRT) or textured quad (POLY_FT4) at an image: texture source from parameters
 * relative to base_load, screen rectangle relative to base_screen. A negative source.x keeps the
 * primitive's texture fields and moves it only. */
void gfx_set_image_primitive_layout(
    void* primitive, const point16_t* base_load, const RECT* base_screen, const image_layout_t* parameters) {
    int texture_column;
    SPRT* sprite = primitive;
    POLY_FT4* quad = primitive;

    /* GPU codes 0x64-0x67 are the free-size sprite (SPRT). */
    if ((getcode(primitive) & 0x7c) == 0x64) {
        if (parameters->source.x >= 0) {
            /* A sprite uses the current texture page: u is the 4bpp pixel column within it. */
            texture_column = base_load->x & 0x3f;
            sprite->u0 = (texture_column << 2) + parameters->source.x;
            sprite->v0 = base_load->y + parameters->source.y;
            sprite->w = parameters->source.w;
            sprite->h = parameters->source.h;
        }
        sprite->x0 = base_screen->x + parameters->x_screen_offset;
        sprite->y0 = base_screen->y + parameters->y_screen_offset;
        return;
    }

    if (parameters->source.x >= 0) {
        quad->tpage = GetTPage(0, 0, base_load->x, base_load->y & 0xf00);
        setUV4(quad, parameters->source.x, base_load->y + parameters->source.y,
            parameters->source.x + parameters->source.w, base_load->y + parameters->source.y, parameters->source.x,
            base_load->y + parameters->source.y + parameters->source.h, parameters->source.x + parameters->source.w,
            base_load->y + parameters->source.y + parameters->source.h);
    }

    setXY4(quad, base_screen->x + parameters->x_screen_offset, base_screen->y + parameters->y_screen_offset,
        base_screen->x + parameters->x_screen_offset + parameters->source.w,
        base_screen->y + parameters->y_screen_offset, base_screen->x + parameters->x_screen_offset,
        base_screen->y + parameters->y_screen_offset + parameters->source.h,
        base_screen->x + parameters->x_screen_offset + parameters->source.w,
        base_screen->y + parameters->y_screen_offset + parameters->source.h);
}

/* Draw count positioned number entries into an image buffer: kind 0 in the small glyphs
 * (text_small_number_glyphs_draw), kind 1 in the large ones (menu_draw_large_number_glyphs); other kinds
 * draw nothing. */
void menu_draw_numeric_display_entries(
    u8* buffer, const menu_number_entry_t* entries, glyph_blit_t* position, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        position->rect.x = entries->x;
        position->rect.y = entries->y;
        if (entries->kind == 0) {
            text_small_number_glyphs_draw(*entries->value, entries->format, buffer, position);
        } else if (entries->kind == 1) {
            menu_draw_large_number_glyphs(*entries->value, entries->format, buffer, position);
        }
        entries++;
    }
}

/* Draw value in decimal text glyphs, right-aligned in a field of digit_count 6-pixel cells at
 * origin, without zero padding (a longer number runs on to the left). Moves origin and
 * clears the text palette offset afterward. */
void menu_display_number_entry(s32 value, u8 digit_count, u8* image, glyph_blit_t* origin) {
    s16 cell;

    origin->rect.x += (digit_count - 1) * 6;
    do {
        text_render_glyph_to_4bpp_image(
            g_text_glyph_bitmap_data + (value % 10) * TEXT_GLYPH_BYTES, image, origin, g_menu_text_palette_offset);
        value = value / 10;
        cell = origin->rect.x;
        origin->rect.x = cell - 6;
    } while (value != 0);
    g_menu_text_palette_offset = 0;
}

/* Draw value as exactly (digits & 0xff) digits, zero-padded and right to left, 7 pixels apart from
 * the menu glyph sheet. The high byte adds marks: 0x1000 a leading glyph, MENU_DECIMAL_SIGN_PLUS
 * (0x400) or MENU_DECIMAL_SIGN_MINUS (0x800) a sign glyph before the digits, and 0xc00 two
 * dashes in place of the number. Hidden numeric values draw
 * a placeholder per digit; the pen ends after the last digit. */
void menu_draw_large_number_glyphs(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos) {
    s32 i;
    s32 cursor;
    glyph_blit_t* src;

    /* One variable holds the sign-glyph selector first and the x to leave the
     * cursor at afterwards; the target keeps both roles in $s6. */
    src = &g_menu_large_number_glyph;
    g_menu_large_number_glyph.rect.h = 0xF;
    cursor = digits & 0xFF00;
    if (cursor == 0xC00) {
        pos->rect.x += 2;
        src->rect.x = 0xBA;
        g_menu_large_number_glyph.rect.y = 0xD;
        g_menu_large_number_glyph.rect.w = 6;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
        g_menu_large_number_glyph.rect.w = 8;
        return;
    }
    if (digits & 0x1000) {
        src->rect.x = 0xC8;
        g_menu_large_number_glyph.rect.y = 0;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
    }
    if ((digits & 0x800) || (digits & 0x400)) {
        if (digits & 0x400) {
            src->rect.x = 0xC8;
        } else {
            src->rect.x = 0xBA;
        }
        src->rect.y = 0xD;
        src->rect.w = 6;
        pos->rect.x -= 6;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_large_number_glyph, pos);
        pos->rect.x += 6;
        src->rect.w = 8;
    }
    /* Not a variable: the target recomputes the digit count at the loop test
     * rather than keeping it live, so each use spells the mask out. */
    pos->rect.x += ((digits & 0xFF) - 1) * 7;
    cursor = pos->rect.x + 7;
    for (i = 0; i < (digits & 0xFF); i++) {
        src->rect.y = 0;
        src->rect.x = (value % 10) * 8 + 0x78;
        if (g_unit_panel_hide_numeric_values != 0) {
            src->rect.x = 0x48;
            src->rect.y = 0x10;
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_large_number_glyph, pos);
        /* After the call: the quotient stays live past the argument setup, so
         * it takes a callee-saved register and the update lands in the delay
         * slot, as the target does at 0x238. */
        value /= 10;
        pos->rect.x -= 7;
    }
    pos->rect.x = cursor;
}
