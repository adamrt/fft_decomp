#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/open.h"
#include "fft/thread.h"
#include "fft/world.h"

/* The thread-2 controller record reuses the first two words as
 * run_followup (0x00) and result (0x04). */
void open_controller_start_text_message(s32 parameter, s32 run_followup) {
    {
        s32 controller = g_open_current_controller_index;

        g_open_controller_stream_start[controller].stream_start = run_followup;
    }
    if (run_followup != 0) {
        world_text_save_section_pointers();
        world_text_init_format_section_pointers(g_open_text_section_offsets);
    }

    world_thread_start(2, world_text_character_handling_thread);
    world_thread_set_parameters(2, 0x33, parameter, 0);

    {
        s32 controller = g_open_current_controller_index;
        s32 previous_mask = g_open_menu_formation_entry_mask;

        g_open_menu_formation_entry_mask = -1;
        g_open_controller_stream_start[controller].stream_length = previous_mask;
    }
    main_sound_play_sfx(MAIN_SFX_WINDOW_OPEN);

    {
        s32 controller = g_open_current_controller_index;
        s32 next_controller = controller + 1;

        g_open_controller_handler_indices[controller] = 8;
        g_open_current_controller_index = next_controller;
    }
}
