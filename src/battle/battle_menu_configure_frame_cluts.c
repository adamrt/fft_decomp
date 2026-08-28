#include "fft/battle.h"

/* Menu frame primitive block: four SPRT packets follow a 0x18-byte header.
 * Only the palette selectors are touched here, so the header stays opaque. */
typedef struct menu_frame_sprites {
    u8 unknown_00[0x18];
    SPRT sprites[4];
} menu_frame_sprites_t;

typedef char assert_menu_frame_sprites_size[sizeof(menu_frame_sprites_t) == 0x68 ? 1 : -1];

/* Select the menu-frame palette rows for the active thread context. */
void battle_menu_configure_frame_cluts(menu_frame_sprites_t* frame) {
    if (battle_thread_is_previous_running() != 0 || g_event_mode == 1) {
        frame->sprites[0].clut = 0x7d3c;
        frame->sprites[1].clut = 0x7dfc;
        frame->sprites[2].clut = 0x7e3c;
        frame->sprites[3].clut = 0x7c7c;
    } else {
        frame->sprites[0].clut = 0x7c3c;
        frame->sprites[1].clut = 0x7d7c;
        frame->sprites[2].clut = 0x7dbc;
        frame->sprites[3].clut = 0x7cbc;
    }
}
