#include "fft/event_debugchr.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Menu window thread without a cursor: rebuilds the text image every seventh
 * frame and alternates between two sprite records until the menu closes. The
 * DEBUGCHR twin of world_menu_icon_strip_thread. */
void debugchr_menu_icon_strip_thread(void) {
    RECT rect;
    menu_icon_record_t frames[2];
    menu_icon_record_t* frame_pair;
    menu_icon_record_t* frame_base;
    s32* stride;
    u32 input = 0;
    s32 frame;
    s32 redraw;
    u8* render;
    s32 frame_parity;
    menu_entry_t* state = (menu_entry_t*)battle_thread_get_current_parameter_1();

    frame_pair = frames;
    battle_menu_build_window_sprites(&rect, state, frame_pair);
    mem_copy_bytes_nonempty(&frame_pair[1], frame_pair, sizeof(*frame_pair));
    redraw = 1;
    frame = 0;
    stride = &g_menu_text_state.pen.stride;
    frame_base = frame_pair;
    for (;;) {
        if (frame == (frame / 7) * 7) {
            redraw = 1;
            render = battle_menu_build_and_upload_window_frame_image(
                state->inner_width, state->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            battle_menu_set_text_origin(8, 9);
            *stride = state->inner_width;
            /* A pen-base local changes the store base and call argument. */
            battle_menu_display_text_entry(
                state->text_id, render, (glyph_blit_t*)(stride - offsetof(glyph_blit_t, stride) / sizeof(*stride)));
            LoadImage(&rect, (u32*)render);
        }
        thread_yield();
        if (redraw == 1) {
            menu_free_memory(render);
            redraw = 0;
        }
        if (battle_menu_should_close_thread(&input) != 0)
            break;
        frame_parity = frame & 1;
        frame_pair = &frame_base[frame_parity];
        battle_menu_select_icon_cluts(&frame_pair->base);
        battle_menu_submit_icon_primitives(&frame_pair->base);
        frame++;
    }
    thread_yield();
    thread_exit_current();
}
