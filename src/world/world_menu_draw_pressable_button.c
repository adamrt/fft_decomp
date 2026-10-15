#include "fft/world.h"

/*
 * Draw an on-screen button from its sprite pieces and animate its press.
 *
 * g_world_menu_button_press_timers[index] counts press frames (until 6 / vsync mode); a pressed
 * button is drawn one pixel lower with the highlight CLUT. Pieces
 * g_world_menu_button_piece_starts[index]..g_world_menu_button_piece_starts[index + 1] are drawn at OT depth 50 upward.
 * Compiled at -O1 like the other formation/shop sprite drawers.
 */
void world_menu_draw_pressable_button(s16 index, s32 x, s16 y, s16 pressed, s16 disabled, u16 sound_effect_id) {
    oriented_quad_t sprite;
    menu_button_piece_t* piece;
    s32 speed;
    s32 i;
    s32 count;
    s16 piece_index;
    s16 w;
    s32 product;
    s32 h;
    u32 flip;
    u16 clut;

    speed = g_menu_event_speed;
    if (speed == 0) {
        speed = 1;
    }
    if (disabled != 0) {
        g_world_menu_button_press_timers[index] = 0;
        clut = g_world_menu_clut_front_alt;
    } else if (pressed != 0) {
        g_world_menu_button_press_timers[index] = 1;
        g_sound_queued_effect_id = sound_effect_id;
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
    sprite.source.clut = clut;
    sprite.source.tpage = g_world_menu_icon_tpage;
    for (i = 0; i < count; i++) {
        piece = &g_world_menu_button_pieces[piece_index];
        sprite.position.x = x + piece->x_offset;
        sprite.position.y = y + piece->y_offset;
        product = g_world_menu_button_piece_scales[piece_index].x * piece->w;
        w = ((product < 0) ? -product : product) >> 12;
        sprite.w = w;
        h = g_world_menu_button_piece_scales[piece_index].y * piece->h;
        h = (h < 0) ? -h : h;
        sprite.h = h >> 12;
        sprite.source.u = piece->u;
        sprite.source.v = piece->v;
        sprite.source.w = piece->w;
        sprite.source.h = piece->h;
        if (g_world_menu_button_piece_scales[piece_index].x >= 0x1005) {
            sprite.w = w + 2;
        }
        flip = g_world_menu_button_piece_scales[piece_index].x < 0;
        if (g_world_menu_button_piece_scales[piece_index].y < 0) {
            flip += 2;
        }
        menu_gfx_enqueue_oriented_textured_quad(&sprite, 0, flip, 0, i + 50);
        piece_index++;
    }
}
