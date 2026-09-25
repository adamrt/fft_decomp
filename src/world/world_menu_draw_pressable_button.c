#include "fft/world.h"

/* Destination rectangle followed by the source rectangle, CLUT and tpage
 * consumed by world_gfx_enqueue_oriented_textured_quad (clut/tpage at 0x10/0x12, as in
 * world_gfx_sprite_desc_t). */
typedef struct world_menu_button_sprite {
    s16 x;     /* 0x00 */
    s16 y;     /* 0x02 */
    s16 w;     /* 0x04 */
    s16 h;     /* 0x06 */
    u16 u;     /* 0x08 */
    u16 v;     /* 0x0a */
    u16 u_w;   /* 0x0c */
    u16 v_h;   /* 0x0e */
    u16 clut;  /* 0x10 */
    u16 tpage; /* 0x12 */
} world_menu_button_sprite_t;

/* 12-byte button piece: source rectangle, then its offset from the button origin. */
typedef struct world_menu_button_piece {
    u16 u;        /* 0x00 */
    u16 v;        /* 0x02 */
    s16 w;        /* 0x04 */
    s16 h;        /* 0x06 */
    u16 x_offset; /* 0x08 */
    u16 y_offset; /* 0x0a */
} world_menu_button_piece_t;

extern world_menu_button_piece_t g_world_menu_button_pieces[];

/*
 * Draw an on-screen button from its sprite pieces and animate its press.
 *
 * g_world_menu_button_press_timers[index] counts press frames (until 6 / vsync mode); a pressed
 * button is drawn one pixel lower with the highlight CLUT. Pieces
 * g_world_menu_button_piece_starts[index]..g_world_menu_button_piece_starts[index + 1] are drawn at OT depth 50 upward.
 * Compiled at -O1 like the other formation/shop sprite drawers.
 */
void world_menu_draw_pressable_button(s16 index, s32 x, s32 y, s16 pressed, u16 disabled, u16 sound_effect_id) {
    world_menu_button_sprite_t sprite;
    world_menu_button_piece_t* piece;
    s32 speed;
    s32 i;
    s32 count;
    s16 piece_index;
    s16 w;
    s32 product;
    s32 h;
    u32 flip;
    u16 clut;

    speed = g_world_gfx_vsync_mode;
    if (speed == 0) {
        speed = 1;
    }
    if (disabled != 0) {
        g_world_menu_button_press_timers[index] = 0;
        clut = g_world_menu_clut_front_alt;
    } else if (pressed != 0) {
        g_world_menu_button_press_timers[index] = 1;
        g_world_menu_sound_effect_id = sound_effect_id;
    } else if (g_world_menu_button_press_timers[index] != 0 && g_world_menu_button_press_timers[index] < 6 / speed) {
        g_world_menu_button_press_timers[index]++;
    } else {
        clut = g_world_menu_clut_front;
        g_world_menu_button_press_timers[index] = 0;
    }
    if (g_world_menu_button_press_timers[index] != 0) {
        y++;
        clut = g_world_menu_button_clut;
    }
    piece_index = g_world_menu_button_piece_starts[index];
    count = g_world_menu_button_piece_starts[index + 1] - piece_index;
    sprite.clut = clut;
    sprite.tpage = g_world_menu_icon_tpage;
    for (i = 0; i < count; i++) {
        piece = &g_world_menu_button_pieces[piece_index];
        sprite.x = x + piece->x_offset;
        sprite.y = y + piece->y_offset;
        product = g_world_menu_button_piece_scales[piece_index].x * piece->w;
        w = ((product < 0) ? -product : product) >> 12;
        sprite.w = w;
        h = g_world_menu_button_piece_scales[piece_index].y * piece->h;
        h = (h < 0) ? -h : h;
        sprite.h = h >> 12;
        sprite.u = piece->u;
        sprite.v = piece->v;
        sprite.u_w = piece->w;
        sprite.v_h = piece->h;
        if (g_world_menu_button_piece_scales[piece_index].x >= 0x1005) {
            sprite.w = w + 2;
        }
        flip = g_world_menu_button_piece_scales[piece_index].x < 0;
        if (g_world_menu_button_piece_scales[piece_index].y < 0) {
            flip += 2;
        }
        world_gfx_enqueue_oriented_textured_quad((const world_oriented_quad_t*)&sprite, 0, flip, 0, i + 50);
        piece_index++;
    }
}
