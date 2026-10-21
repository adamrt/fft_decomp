/* The hidden Music Test runs a WORLD list menu from OPEN and can open a text
 * description for the selected track while the title controller waits. */

#include "fft/controller_stack.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_input.h"
#include "fft/message.h"
#include "fft/music.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/title.h"
#include "psx/pad.h"
#include "psx/types.h"

extern menu_entry_t g_music_test_list_params;
extern menu_text_layout_t g_music_test_list_state;
static s16 s_music_test_list_entries[96];
extern s16 g_music_test_list_entry_flags[96];
extern s16 g_music_test_selected_option;
extern struct suzuki_music* g_music_test_sound_handle; /* handle of the playing music test track */

static s16 s_music_test_row_actions[16] = { MENU_ROW_ACTION_NONE, MENU_ROW_ACTION_NONE, MENU_ROW_ACTION_NONE,
    MENU_ROW_ACTION_NONE, MENU_ROW_ACTION_NONE, MENU_ROW_ACTION_NONE, MENU_ROW_ACTION_NONE, MENU_ROW_ACTION_NONE,
    MENU_ROW_ACTION_NONE, MENU_ROW_ACTION_NONE, MENU_ROW_ACTION_NONE, MENU_ROW_ACTION_NONE, MENU_ROW_ACTION_NONE,
    MENU_ROW_ACTION_NONE, MENU_ROW_ACTION_NONE, MENU_ROW_ACTION_NONE };

/* The 96 music-test rows select scenario music IDs 1 through 96. */
#define MUSIC_TEST_TRACK_IDS_16(first)                                                                                 \
    first, first + 1, first + 2, first + 3, first + 4, first + 5, first + 6, first + 7, first + 8, first + 9,          \
        first + 10, first + 11, first + 12, first + 13, first + 14, first + 15
static u8 s_music_test_music_ids[96]
    = { MUSIC_TEST_TRACK_IDS_16(1), MUSIC_TEST_TRACK_IDS_16(17), MUSIC_TEST_TRACK_IDS_16(33),
          MUSIC_TEST_TRACK_IDS_16(49), MUSIC_TEST_TRACK_IDS_16(65), MUSIC_TEST_TRACK_IDS_16(81) };
#undef MUSIC_TEST_TRACK_IDS_16

static void controller_start_text_message(s32 parameter, s32 use_world_text);
/* Fills the music-test list's column layout and menu entry, then starts the
 * AT list thread on them. The window is the extent's width plus a 24-pixel
 * margin, rounded down to a multiple of four; its height fields start at 0. */
static void music_test_start_list_thread(s32 thread_id, s32 limit, point32_t* extent, point32_t* origin) {
    s32 width = (extent->x + 0x18) & 0xFFFC;

    g_music_test_list_state.row_count = extent->y;
    g_music_test_list_state.hidden_rows = limit - extent->y;
    g_music_test_list_state.x[1] = 4;
    g_music_test_list_state.x[0] = 4;
    g_music_test_list_state.mode[1] = MENU_TEXT_COLUMN_HIDDEN;
    g_music_test_list_state.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
    g_music_test_list_state.text_ids[0] = s_music_test_list_entries;
    g_music_test_list_state.text_colors[0] = g_music_test_list_entry_flags;
    g_music_test_list_state.mode[0] = MENU_TEXT_COLUMN_TEXT;
    g_music_test_list_params.vram_origin.x = 0x200;
    g_music_test_list_params.vram_origin.y = 0x100;
    g_music_test_list_params.inner_width = width;
    g_music_test_list_params.inner_height = 0;
    g_music_test_list_params.window.x = origin->x - 0x80;
    g_music_test_list_params.window.y = origin->y - 0x78;
    g_music_test_list_params.window.w = width;
    g_music_test_list_params.window.h = 0;
    g_music_test_list_params.text_image.source.x = 0;
    g_music_test_list_params.text_image.source.y = 0;
    g_music_test_list_params.text_image.source.w = width;
    g_music_test_list_params.text_image.source.h = 0;
    g_music_test_list_params.text_image.x_screen_offset = 0;
    g_music_test_list_params.text_image.y_screen_offset = 0;
    g_music_test_list_params.text_id = 0;
    g_music_test_list_params.max_row_index = extent->y;
    g_music_test_list_params.thread_count = 0;
    g_music_test_list_params.row_actions = s_music_test_row_actions;
    g_music_test_list_params.header_id = 1;
    g_music_test_list_params.context = &g_music_test_list_state;
    g_music_test_list_params.value = &g_music_test_selected_option;
    g_music_test_list_params.selected_index = 0;
    g_music_test_list_params.select_text_table = 0;

    thread_start(thread_id, screen_text_list_thread);
    thread_parameters_set(thread_id, (s32)&g_music_test_list_params, 0, 0);
}

/* Open the music test: read OPNBK image 1 as the background, fill the 96-entry
 * track list (text ids 0xc000 + entry, flags clear) and push the music test
 * controller (handler 7) at MUSIC_TEST_STEP_OPEN as the screen is revealed.
 *
 * The list is drawn by the WORLD menu frame, which this turns on with Circle,
 * Cross and Select withheld from it and ordering-table entry 4 as its base. The
 * only caller is step 5 of new_game_update. */
void music_test_menu_open(void) {
    s32 i;
    s32 four;
    s32 controller;

    file_request_openbk_image(1, g_file_destination);
    file_pending_wait();

    for (i = 0; i < 96; i++) {
        s_music_test_list_entries[i] = i + 0xc000;
        g_music_test_list_entry_flags[i] = 0;
    }

    script_event_speed_set(1);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    /* The same 4 is stored twice, as the WORLD menu frame's OT index and as MUSIC_TEST_STEP_OPEN; the local
     * keeps it in a saved register across the fade-out call instead of being rematerialized. */
    four = 4;
    g_music_test_menu_otag_index = four;
    g_system_runtime_flags |= SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME;
    gfx_start_overlay_fade_out(0x20);

    controller = g_controller_depth;
    g_controller_records[controller].music_test.music_id = 0;
    g_controller_records[controller].music_test.delay = 0;
    g_controller_records[controller].music_test.music_slot = 0;
    g_controller_records[controller].music_test.step = four;
    g_controller_records[controller].music_test.blink_frames = 0;
    g_controller_handler_indices[controller] = CONTROLLER_MUSIC_TEST;
    g_controller_depth = controller + 1;
}

/* Runs the hidden Music Test menu.
 *
 * Circle fades out any current track and queues the selected entry's music.
 * Cross stops playback or, when idle, closes the menu, reloads OPNBK image 0
 * and returns to the title menu. Select opens the thread-2 text window
 * on text id 0xC800 + entry: the list shows text 0xC000 + entry, section 24
 * of OPEN.LZW (the 96 track titles), and Select the same entry of section 25
 * (the composers' comments); both are still the Japanese text in this
 * release. Up and Down are withheld from the list for ten frames after each
 * selection.
 */
void music_test_menu_update(controller_record_t* record) {
    music_test_state_t* state = &record->music_test;
    point32_t extent;
    point32_t origin;
    s32 buttons;

    if (state->blink_frames != 0 && ++state->blink_frames >= 10) {
        state->blink_frames = 0;
        menu_cursor_bob_hold_set(0);
        g_menu_blocked_buttons ^= PSX_PAD_UP | PSX_PAD_DOWN;
    }

    if (state->step == MUSIC_TEST_STEP_OPEN) {
        if (!(g_gfx_overlay_fade.flags & GFX_OVERLAY_FADE_ACTIVE)) {
            extent.x = 0x15;
            extent.y = 0xB;
            origin.x = 0x8C;
            origin.y = 0x14;
            music_test_start_list_thread(MUSIC_TEST_THREAD_SLOT_LIST, 0x60, &extent, &origin);
            state->step = MUSIC_TEST_STEP_IDLE;
        }
    } else if (state->step == MUSIC_TEST_STEP_START_TRACK) {
        if (music_is_loading_stub() == 0) {
            g_music_test_sound_handle = music_track_switch(state->music_slot, 0x7F, 0x10);
            state->step = MUSIC_TEST_STEP_IDLE;
        }
    } else if (state->step == MUSIC_TEST_STEP_FADE_OUT) {
        if (!(g_gfx_overlay_fade.flags & GFX_OVERLAY_FADE_ACTIVE)) {
            g_system_runtime_flags ^= SYSTEM_RUNTIME_FLAG_INPUT_DISABLED | SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME;
            file_request_openbk_image(0, g_file_destination);
            file_pending_wait();
            g_controller_depth--;
            title_menu_push(0);
        }
    } else if (state->step == MUSIC_TEST_STEP_CLOSE) {
        if (thread_is_running(MUSIC_TEST_THREAD_SLOT_LIST) == 0) {
            state->step = MUSIC_TEST_STEP_FADE_OUT;
            gfx_start_overlay_fade_in(0x20);
        }
    } else if (state->delay != 0) {
        /* A queued change: once the old track has faded, release its slot and
         * open the new one, which starts on a later frame. */
        if (--state->delay == 0) {
            if (state->music_id != 0) {
                if (state->music_slot != 0) {
                    music_scenario_slot_unload(state->music_slot);
                }
                state->music_slot = music_free_slot_load(state->music_id);
                state->step = MUSIC_TEST_STEP_START_TRACK;
            } else {
                if (state->music_slot != 0) {
                    music_scenario_slot_unload(state->music_slot);
                }
                state->music_slot = 0;
            }
        }
    } else {
        /* Idle: release a track that has finished on its own, then take input. */
        if (state->music_slot != 0 && suzuki_smd_is_music_playing(g_music_test_sound_handle) == 0) {
            music_scenario_slot_unload(state->music_slot);
            state->music_slot = 0;
            state->music_id = 0;
        }
        buttons = g_input_new_button_presses;
        if (buttons & PSX_PAD_CIRCLE) {
            menu_cursor_bob_hold_set(1);
            state->blink_frames = 1;
            g_menu_blocked_buttons |= PSX_PAD_UP | PSX_PAD_DOWN;
            if (state->music_id != 0) {
                state->delay = 0x20;
                music_volume_target_set(0, 0x78);
            } else {
                state->delay = 1;
            }
            state->music_id = s_music_test_music_ids[g_music_test_list_params.selected_index];
        } else if (buttons & PSX_PAD_CROSS) {
            if (state->music_id != 0) {
                music_volume_target_set(0, 0x78);
                state->music_id = 0;
                state->delay = 0x20;
            } else {
                state->step = MUSIC_TEST_STEP_CLOSE;
                thread_parameters_set(MUSIC_TEST_THREAD_SLOT_LIST, 0, 0, 1);
                g_system_runtime_flags |= SYSTEM_RUNTIME_FLAG_INPUT_DISABLED;
            }
        } else if (buttons & PSX_PAD_SELECT) {
            controller_start_text_message(g_music_test_list_params.selected_index + 0xC800, 0);
        }
    }
}

/* Start the text message thread and push the controller that waits for it. */
static void controller_start_text_message(s32 parameter, s32 use_world_text) {
    {
        s32 controller = g_controller_depth;

        g_controller_records[controller].text_message.use_world_text = use_world_text;
    }
    if (use_world_text != 0) {
        text_section_pointers_save();
        text_pointer_table_relocate(g_screen_text_section_offsets);
    }

    thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_dialog_thread);
    thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x33, parameter, 0);

    {
        s32 controller = g_controller_depth;
        s32 saved_blocked_buttons = g_menu_blocked_buttons;

        g_menu_blocked_buttons = -1;
        g_controller_records[controller].text_message.saved_blocked_buttons = saved_blocked_buttons;
    }
    sound_sfx_play(SOUND_SFX_WINDOW_OPEN);

    {
        s32 controller = g_controller_depth;

        g_controller_handler_indices[controller] = CONTROLLER_TEXT_MESSAGE;
        g_controller_depth = controller + 1;
    }
}

/* Handler 8: once the message thread has ended, give back the blocked buttons,
 * pop the controller and, with use_world_text, restore the text section pointers
 * controller_start_text_message replaced. */
void controller_wait_text_message(controller_record_t* record) {
    if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) != 0) {
        return;
    }
    g_menu_blocked_buttons = record->text_message.saved_blocked_buttons;
    g_controller_depth--;
    if (record->text_message.use_world_text != 0) {
        text_section_pointers_restore();
    }
}
