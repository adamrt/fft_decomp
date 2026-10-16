#ifndef FULLSCREEN_GRAPHIC_H
#define FULLSCREEN_GRAPHIC_H

/* ETC's full-screen graphics: chapter titles (chapter_title_), Game Over (game_over_) and the END images. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Index into g_fullscreen_graphics, named after the image file: 0 to 6 are chapter
 * titles (CHAPTER1.BIN to CHAPTER4.BIN), 7 is GAMEOVER.BIN and 8 to 12 are
 * END1.BIN to END5.BIN. Only the ones fullscreen_graphic_open treats specially are
 * named. */
typedef enum fullscreen_graphic_id {
    FULLSCREEN_GRAPHIC_END1 = 8, /* g_gfx_screen_fade rises from -0xff; for the others it falls from 0xff */
    FULLSCREEN_GRAPHIC_END3 = 10,
    FULLSCREEN_GRAPHIC_END4 = 11,
    FULLSCREEN_GRAPHIC_END5 = 12,
} fullscreen_graphic_id_e;

/* One image of g_fullscreen_graphics: its file, frame-buffer rectangle, tile
 * dimensions and texture mode. The game-over renderer swaps the screen axes
 * of the chapter-title regions. */
typedef struct fullscreen_graphic {
    const char* path;
    s32 lba;
    s32 size;
    RECT* framebuffer_rect;
    sprite_sheet_frame_t* dimensions;
    void* _unused_14;
    void* _unused_18;
    s32 texture_mode;
} fullscreen_graphic_t;
typedef char etc_graphic_size_check[sizeof(fullscreen_graphic_t) == 32 ? 1 : -1];

extern fullscreen_graphic_t g_fullscreen_graphics[13];
extern POLY_GT4 g_fullscreen_graphic_chapter_primitives_a[2][4];
extern POLY_GT4 g_fullscreen_graphic_chapter_primitives_b[2][4];
extern RECT g_fullscreen_graphic_game_over_palette_rect;
extern const char g_fullscreen_graphic_allocation_wait_message[];

void fullscreen_graphic_thread(void);

#endif
