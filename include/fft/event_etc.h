#ifndef EVENT_ETC_H
#define EVENT_ETC_H

/* EVENT/ETC.OUT: the BATTLE overlay of the full-screen chapter title, Game Over and ending images. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* graphics */
/* Index into g_etc_graphics, named after the image file: 0 to 6 are chapter
 * titles (CHAPTER1.BIN to CHAPTER4.BIN), 7 is GAMEOVER.BIN and 8 to 12 are
 * END1.BIN to END5.BIN. Only the ones etc_graphic_open treats specially are
 * named. */
typedef enum etc_graphic_id {
    ETC_GRAPHIC_END1 = 8, /* g_gfx_screen_fade rises from -0xff; for the others it falls from 0xff */
    ETC_GRAPHIC_END3 = 10,
    ETC_GRAPHIC_END4 = 11,
    ETC_GRAPHIC_END5 = 12,
} etc_graphic_id_e;

/* One image of g_etc_graphics: its file, frame-buffer rectangle, tile
 * dimensions and texture mode. The game-over renderer swaps the screen axes
 * of the chapter-title regions. */
typedef struct etc_graphic {
    const char* path;
    s32 lba;
    s32 size;
    RECT* framebuffer_rect;
    sprite_sheet_frame_t* dimensions;
    void* _unused_14;
    void* _unused_18;
    s32 texture_mode;
} etc_graphic_t;
typedef char etc_graphic_size_check[sizeof(etc_graphic_t) == 32 ? 1 : -1];

extern etc_graphic_t g_etc_graphics[13];
extern POLY_GT4 g_etc_graphic_chapter_primitives_a[2][4];
extern POLY_GT4 g_etc_graphic_chapter_primitives_b[2][4];
extern RECT g_etc_graphic_game_over_palette_rect;
extern const char g_etc_allocation_wait_message[];

void etc_graphic_show_async(void);

#endif
