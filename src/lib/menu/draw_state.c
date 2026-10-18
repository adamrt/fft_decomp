/* Menu drawing color, ordering-table index and palette selection. */

#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "psx/types.h"

extern u16 g_menu_active_clut_back;
extern u16 g_menu_active_clut_front;
extern u16 g_menu_text_clut_0_mode0;
extern u16 g_menu_text_clut_0_mode1;
extern u16 g_menu_text_clut_1_mode1;
extern u16 g_menu_text_clut_2_mode1;
extern u16 g_menu_text_clut_3_mode0;
extern u16 g_menu_text_clut_3_mode1;

/* WORLD owns its drawing-color setter; JOBSTTS does not link this accessor. */
#if defined(FFT_EVENT_BUNIT) || defined(FFT_EVENT_EQUIP) || defined(FFT_EVENT_CARD)
/* Set the menu text color. BUNIT accepts a null pointer; CARD and EQUIP do not check it. */
void gfx_set_draw_color(const rgb24_t* color) {
#    ifdef FFT_EVENT_BUNIT
    /* CARD and EQUIP dereference the color without this guard. */
    if (color == 0) {
        return;
    }
#    endif
    g_menu_text_color.r = color->r;
    g_menu_text_color.g = color->g;
    g_menu_text_color.b = color->b;
}
#endif

/* JOBSTTS changes the ordering-table index through its commands. */
#ifndef FFT_EVENT_JOBSTTS
/* Select the ordering-table bucket used by subsequent menu packets. */
void gfx_set_otag_index(s32 value) {
    g_menu_gfx_otag_index = value;
}
#endif

/* WORLD keeps palette selection in its draw-area unit. */
#ifndef FFT_WORLD
/* Select the normal (0) or alternate menu palette: the sprite color and the six CLUT selections
 * the menu commands draw with. */
void menu_set_palette_mode(s32 mode) {
    g_menu_use_alternate_palette = mode;
    if (mode != 0) {
        g_menu_text_color.r = 0x60;
        g_menu_text_color.g = 0x60;
        g_menu_text_color.b = 0x80;
        g_menu_window_clut = g_menu_text_clut_0_mode1;
        g_menu_alternate_number_clut = g_menu_text_clut_1_mode1;
        g_menu_palette_clut = g_menu_text_clut_2_mode1;
        g_menu_disabled_number_clut = g_menu_text_clut_3_mode1;
        g_menu_active_clut_front = g_menu_cursor_mode1_foreground_clut;
        g_menu_active_clut_back = g_menu_cursor_mode1_background_clut;
    } else {
        g_menu_text_color.r = 0x80;
        g_menu_text_color.g = 0x80;
        g_menu_text_color.b = 0x80;
        g_menu_window_clut = g_menu_text_clut_0_mode0;
        g_menu_alternate_number_clut = g_menu_text_clut_1_mode0;
        g_menu_palette_clut = g_menu_text_clut_2_mode0;
        g_menu_disabled_number_clut = g_menu_text_clut_3_mode0;
        g_menu_active_clut_front = g_menu_cursor_mode0_foreground_clut;
        g_menu_active_clut_back = g_menu_cursor_mode0_background_clut;
    }
}
#endif
