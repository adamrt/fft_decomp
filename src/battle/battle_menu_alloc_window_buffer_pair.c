#include "fft/battle.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Claims the first free menu window buffer pair for the current thread,
 * initialises the first record's primitives and returns it; when no pair
 * is free the current thread is stopped. The record loop
 * runs once: the target's `blez` exit test only reproduces with a bound
 * of 1, so the second record of the pair is left uninitialised.
 *
 * BATTLE twin of world_menu_alloc_window_buffer_pair. */
void* battle_menu_alloc_window_buffer_pair(void) {
    menu_window_buffer_t* buffer;
    s32 index;
    s32 i;

    for (index = 0; index < 6; index += 2) {
        if (g_battle_menu_buffer_slots[index] == (void*)-1) {
            g_battle_menu_buffer_slots[index] = &g_battle_menu_window_buffers[index];
            g_battle_menu_buffer_slots[index + 1] = &g_battle_menu_window_buffers[index + 1];
            buffer = g_battle_menu_buffer_slots[index];
            for (i = 0; i < 1; i++) {
                buffer->active = 0;
                buffer->thread_id = g_battle_current_thread_id;
                buffer->portrait_code = -1;
                buffer->r = 0x80;
                buffer->g = 0x80;
                buffer->b = 0x80;
                buffer->dialogue_selector = 0;
                buffer->portrait_flipped = 0;
                buffer->is_message_box = 0;
                buffer->icon_flags = 0;
                buffer->scroll_range = 1;
                buffer->scroll_position = 1;
                battle_menu_init_sprite_array(buffer->sprites, 4, 0x7c3c);
                battle_menu_init_sprite_array(buffer->icon_sprites, 3, 0x7e7c);
                SetDrawMode(&buffer->draw_mode, 0, 0, (u16)GetTPage(0, 2, 0x3c0, 0x100),
                    &g_battle_menu_disabled_texture_window);
                SetSemiTrans(&buffer->sprites[0], 0);
                SetSemiTrans(&buffer->sprites[1], 1);
                buffer->sprites[0].u0 = 0xa8;
                buffer->sprites[1].u0 = 0xb8;
                buffer->sprites[0].clut = 0x7d7c;
                buffer->sprites[1].clut = 0x7dbc;
                buffer->sprites[2].clut = 0x7cbc;
                buffer->icon_sprites[0].u0 = 0xd8;
                buffer->sprites[0].w = 0x10;
                buffer->sprites[0].h = 0x10;
                buffer->sprites[1].w = 0x10;
                buffer->sprites[1].h = 0x10;
                buffer->sprites[2].w = 0x10;
                buffer->sprites[2].h = 0x10;
                buffer->sprites[0].v0 = 0;
                buffer->sprites[1].v0 = 0;
                buffer->icon_sprites[0].w = 8;
                buffer->icon_sprites[0].h = 0x10;
                buffer->icon_sprites[1].w = 8;
                buffer->icon_sprites[1].h = 0x10;
                buffer->icon_sprites[2].w = 8;
                buffer->icon_sprites[2].h = 8;
                buffer->icon_sprites[0].v0 = 0;
                buffer->icon_sprites[1].u0 = 0xe0;
                buffer->icon_sprites[1].v0 = 0;
                buffer->icon_sprites[2].u0 = 0x10;
                buffer->icon_sprites[2].v0 = 0x10;
                battle_gfx_init_default_poly_ft4(&buffer->quads[0]);
                battle_gfx_init_default_poly_ft4(&buffer->quads[1]);
                buffer->quads[0].tpage = GetTPage(0, 0, 0x3c0, 0x100);
                buffer->quads[1].tpage = GetTPage(0, 0, 0x340, 0x100);
                buffer++;
            }
            return g_battle_menu_buffer_slots[index];
        }
    }
    battle_thread_exit_current();
}
