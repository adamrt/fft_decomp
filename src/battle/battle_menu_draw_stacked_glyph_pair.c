#include "fft/battle.h"
#include "fft/battle_menu_status_panel.h"
#include "fft/battle_text.h"
#include "psx/types.h"

/* Source rectangle in the shared menu glyph image. */
typedef battle_menu_status_panel_glyph_t battle_glyph_t;

/* Destination cursor used by blit_text_glyph. */
typedef struct battle_text_position {
    u16 x;
    u16 y;
    u8 unknown_04[4];
    u16 unknown_08;
    u16 unknown_0a;
    s32 pixel_adjustment;
} battle_text_position_t;

extern battle_glyph_t g_battle_menu_stacked_glyph_lower;
extern battle_glyph_t g_battle_menu_stacked_glyph_upper;

void battle_menu_draw_stacked_glyph_pair(void* pixels, battle_text_position_t* position) {
    position->x -= 2;
    position->y += 2;
    blit_text_glyph(g_battle_menu_glyph_image, pixels, &g_battle_menu_stacked_glyph_upper, position);
    position->y += 4;
    blit_text_glyph(g_battle_menu_glyph_image, pixels, &g_battle_menu_stacked_glyph_lower, position);
}
