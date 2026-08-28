#include "fft/world.h"
#include "psx/types.h"

/* Screen x and texture u of one background tile; two alternating rows of four. */
typedef struct world_background_tile {
    u16 x;
    u16 u;
} world_background_tile_t;

/* Local working set: the tile rectangle, its four corner RGB triples and the
 * per-grid-point shade table (rows 32 pixels apart, columns 64). */
typedef struct world_background_draw_state {
    world_gfx_textured_rect_source_t rect; /* 0x00 */
    u8 rgb[16];                            /* 0x18 */
    u8 shades[16][5];                      /* 0x28 */
} world_background_draw_state_t;

extern world_background_tile_t g_world_formation_background_tiles[2][4];

/*
 * Draw the formation-screen background as 64x32 tiles from `y` downwards,
 * shading each tile corner by its distance from the formation cursor.
 *
 * The target reads the rectangle's y with `lhu` and sign-extends it for each
 * comparison, hence the `(s16)(u16)` reads. `draw_row` is bound to s4 because
 * the target recomputes `draw_row * 5` inside the tile loop; the loop
 * optimizer never treats a hard register as invariant, while a pseudo lets it
 * hoist that product and the shade-row addresses.
 */
void world_formation_draw_background_tiles(s32 y) {
    world_background_draw_state_t st;
    s32 row;
    s32 col;
    register s32 draw_row __asm__("$20");
    s32 shade;
    world_background_tile_t* tile;
    world_background_tile_t(*tiles)[4];

    draw_row = 0;
    st.rect.y = y;
    st.rect.w = 0x40;
    st.rect.clut = 0x40;
    st.rect.h = 0x20;
    st.rect.tpage = 0x20;
    st.rect.v = 0xD8;
    for (row = 0; y < 0x101; row++) {
        for (col = 0; col < 5; col++) {
            shade = world_formation_cursor_distance_falloff(col << 6, y, 0xFF, 0x50);
            st.shades[row][col] = (shade >= 0xDD) ? 0xDC : shade;
        }
        y += 0x20;
    }
    st.rect.clut = g_world_menu_background_clut;
    st.rect.tpage = g_world_menu_icon_tpage;
    for (; (s16)(u16)st.rect.y < 0xF0; draw_row++) {
        tiles = g_world_formation_background_tiles;
        if ((s16)(u16)st.rect.y >= -0x20) {
            col = 0;
            tile = tiles[draw_row % 2];
            for (; col < 4; col++) {
                st.rect.x = tile[col].x;
                st.rect.u = tile[col].u;
                st.rgb[0] = st.rgb[1] = st.rgb[2] = st.shades[draw_row][col];
                st.rgb[3] = st.rgb[4] = st.rgb[5] = st.shades[draw_row][col + 1];
                st.rgb[6] = st.rgb[7] = st.rgb[8] = st.shades[draw_row + 1][col];
                st.rgb[9] = st.rgb[10] = st.rgb[11] = st.shades[draw_row + 1][col + 1];
                world_gfx_append_poly_gt4_to_otag(&st.rect, st.rgb, 0, (s16)st.rect.u == 0x58);
            }
        }
        st.rect.y += 0x20;
    }
}
