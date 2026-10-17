/* LIBGPU 80022fd0-80023288. */
#include "psx/libgpu.h"

int FntOpen(int x, int y, int width, int height, int background, int characters) {
    RECT window;
    SPRT_8* sprite;
    int i;
    if (g_psyq_gpu_font_window_count >= FNT_MAX_ID)
        return -1;
    if (g_psyq_gpu_font_window_count == 0)
        g_psyq_gpu_font_allocated_characters = 0;
    Font[g_psyq_gpu_font_window_count].wrap_disabled = width == 0;
    if (characters + g_psyq_gpu_font_allocated_characters > FNT_MAX_SPRT)
        characters = FNT_MAX_SPRT - g_psyq_gpu_font_allocated_characters;
    setRECT(&window, 0, 0, 256, 256);
    SetDrawMode(&Font[g_psyq_gpu_font_window_count].mode, 0, 0, g_psyq_gpu_font_tpage, &window);
    if (background) {
        SetTile(&Font[g_psyq_gpu_font_window_count].tile);
        setRGB0(&Font[g_psyq_gpu_font_window_count].tile, 0, 0, 0);
        SetSemiTrans(&Font[g_psyq_gpu_font_window_count].tile, background == 2);
    }
    setXY0(&Font[g_psyq_gpu_font_window_count].tile, x, y);
    setWH(&Font[g_psyq_gpu_font_window_count].tile, width, height);
    Font[g_psyq_gpu_font_window_count].character_limit = characters;
    Font[g_psyq_gpu_font_window_count].text_count = 0;
    Font[g_psyq_gpu_font_window_count].text = g_psyq_gpu_font_text_pool + g_psyq_gpu_font_allocated_characters;
    Font[g_psyq_gpu_font_window_count].sprites = g_psyq_gpu_font_sprite_pool + g_psyq_gpu_font_allocated_characters;
    *Font[g_psyq_gpu_font_window_count].text = 0;
    sprite = Font[g_psyq_gpu_font_window_count].sprites;
    for (i = 0; i < characters; i++) {
        SetSprt8(sprite);
        sprite->clut = g_psyq_gpu_font_clut;
        sprite++;
    }
    g_psyq_gpu_font_allocated_characters += characters;
    return g_psyq_gpu_font_window_count++;
}
