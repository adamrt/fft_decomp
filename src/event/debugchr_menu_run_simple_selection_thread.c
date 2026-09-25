#include "fft/battle.h"
#include "fft/battle_menu_window.h"
#include "fft/main_heap.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void debugchr_menu_run_simple_selection_thread(void) {
    u8 local_10[8];
    battle_menu_window_record_t frames[2];
    battle_menu_window_record_t* frame_pair;
    battle_menu_window_record_t* frame_base;
    s32* display_x;
    s32 done = 0;
    s32 frame;
    s32 redraw;
    u32* render;
    s32 frame_parity;
    battle_menu_idle_action_entry_t* state = (battle_menu_idle_action_entry_t*)battle_thread_get_current_parameter_1();

    frame_pair = frames;
    battle_menu_build_window_sprites(local_10, state, frame_pair);
    battle_copy_bytes(&frame_pair[1], frame_pair, sizeof(*frame_pair));
    redraw = 1;
    frame = 0;
    display_x = &g_menu_inner_window_width;
    frame_base = frame_pair;
    for (;;) {
        if (frame == (frame / 7) * 7) {
            redraw = 1;
            render
                = battle_menu_build_and_upload_window_frame_image(state->inner_width, state->inner_height, local_10, 1);
            battle_menu_set_text_origin(8, 9);
            *display_x = state->inner_width;
            battle_menu_display_text_entry(state->text_id, render, (u8*)display_x - 8);
            LoadImage((RECT*)local_10, (u32*)render);
        }
        battle_thread_yield();
        if (redraw == 1) {
            battle_menu_free_memory(render);
            redraw = 0;
        }
        if (battle_menu_should_close_thread(&done) != 0)
            break;
        frame_parity = frame & 1;
        frame_pair = &frame_base[frame_parity];
        battle_menu_configure_frame_cluts(frame_pair);
        battle_menu_submit_frame_primitives(frame_pair);
        frame++;
    }
    battle_thread_yield();
    battle_thread_exit_current();
}
