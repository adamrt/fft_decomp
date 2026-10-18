/* Music test, title-demo and new-game controllers, with the prologue and its picture crossfades. */

#include "fft/controller.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/message.h"
#include "fft/movie.h"
#include "fft/music.h"
#include "fft/prologue.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/title.h"
#include "fft/window.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Prologue state and its encoded line records. */
typedef struct prologue_line {
    u8 texture_row;
    u8 width;
    u8 _unused_02[2];
} prologue_line_t;
typedef char prologue_line_size_must_be_4[sizeof(prologue_line_t) == 4 ? 1 : -1];

/* High nibble of the image command byte in prologue_entry_t.header. */
typedef enum prologue_image_operation {
    PROLOGUE_PREPARE_IMAGE = 0x10,
    PROLOGUE_START_CROSSFADE = 0x20
} prologue_image_operation_e;

enum { PROLOGUE_IMAGE_OPERATION_MASK = 0xf0, PROLOGUE_IMAGE_INDEX_MASK = 0x0f };

/* prologue_t.flags. */
typedef enum prologue_flag {
    PROLOGUE_NEXT_ENTRY = 0x01,  /* starts the next entry of the table, or ends at the target */
    PROLOGUE_NEXT_STEP = 0x02,   /* starts the next step of the entry */
    PROLOGUE_WAIT_CIRCLE = 0x08, /* the entry is complete: waits for circle */
    PROLOGUE_DIM = 0x10,         /* the line strips fade out before the next entry */
    PROLOGUE_SKIP = 0x20         /* start was pressed: fades out and ends */
} prologue_flag_e;

/* The new-game prologue's controller (g_prologue). */
typedef struct prologue {
    /* 0x00 */ u16 flags;    /* prologue_flag_e */
    /* 0x02 */ s16 ot_layer; /* text and shadow occupy this layer and the next */
    /* 0x04 */ s16 current_index;
    /* 0x06 */ s16 target_index;
    /* 0x08 */ s16 step;
    /* 0x0a */ s16 step_count;
    /* 0x0c */ s16 tpage_x; /* (header & 0xf00) >> 2; GetTPage x */
    /* 0x0e */ s16 tpage_y; /* (header >> 4) & 0x100; GetTPage y */
    /* 0x10 */ s16 line_texture_rows[7];
    /* 0x1e */ s16 line_widths[7];
    /* 0x2c */ s16 timer;
    /* 0x2e */ s16 intensity;
    /* 0x30 */ prologue_entry_t** entry_table;
    /* 0x34 */ prologue_entry_t* entry;
} prologue_t;
typedef char prologue_size_must_be_56[(sizeof(prologue_t) == 56) ? 1 : -1];

extern prologue_t g_prologue;
extern POLY_GT4 g_prologue_line_primitives[2][14];
extern menu_entry_t g_music_test_list_params;
extern menu_text_layout_t g_music_test_list_state;
extern s16 g_music_test_list_entries[96];
extern s16 g_music_test_list_entry_flags[96];
extern s16 g_music_test_selected_option;
extern struct suzuki_music* g_music_test_sound_handle; /* handle of the playing music test track */

static s16 g_music_test_row_actions[16]
    = { 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000 };

/* The 96 music-test rows select scenario music IDs 1 through 96. */
#define MUSIC_TEST_TRACK_IDS_16(first)                                                                                 \
    first, first + 1, first + 2, first + 3, first + 4, first + 5, first + 6, first + 7, first + 8, first + 9,          \
        first + 10, first + 11, first + 12, first + 13, first + 14, first + 15
static u8 g_music_test_music_ids[96]
    = { MUSIC_TEST_TRACK_IDS_16(1), MUSIC_TEST_TRACK_IDS_16(17), MUSIC_TEST_TRACK_IDS_16(33),
          MUSIC_TEST_TRACK_IDS_16(49), MUSIC_TEST_TRACK_IDS_16(65), MUSIC_TEST_TRACK_IDS_16(81) };
#undef MUSIC_TEST_TRACK_IDS_16

static void controller_start_text_message(s32 parameter, s32 use_world_text);
static void prologue_prepare_crossfade(s32 file_index);
static void prologue_draw_text_lines(s32 intensity);
static void prologue_step_crossfade(void);
/* The new-game prologue's pictures are 210x180, at rows 43 to 222 of the
 * 256-pixel-wide frame image. */
#define PROLOGUE_IMAGE_WIDTH      210
#define PROLOGUE_IMAGE_HEIGHT     180
#define PROLOGUE_IMAGE_ROW_STRIDE 256

/* Crossfade control bytes and one fade step per picture row. */
typedef struct prologue_crossfade {
    u8 active;
    u8 band_end_row;
    u8 row_steps[PROLOGUE_IMAGE_HEIGHT];
} prologue_crossfade_t;
typedef char prologue_crossfade_size_must_be_182[sizeof(prologue_crossfade_t) == 182 ? 1 : -1];

extern prologue_crossfade_t g_prologue_crossfade;
extern u16* g_prologue_crossfade_frame_pixels;
extern u16* g_prologue_crossfade_old_picture;

/* prologue_step_crossfade reloads the source pointer for every column. */
extern u16* volatile g_prologue_crossfade_new_picture;

/* A 256x240 16-bit frame image in 32-bit words. */
#define PROLOGUE_FRAME_IMAGE_WORDS (256 * 240 / 2)

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
    g_music_test_list_state.text_ids[0] = g_music_test_list_entries;
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
    g_music_test_list_params.row_actions = g_music_test_row_actions;
    g_music_test_list_params.header_id = 1;
    g_music_test_list_params.context = &g_music_test_list_state;
    g_music_test_list_params.value = &g_music_test_selected_option;
    g_music_test_list_params.selected_index = 0;
    g_music_test_list_params.select_text_table = 0;

    thread_start(thread_id, screen_text_list_thread);
    thread_set_parameters(thread_id, (s32)&g_music_test_list_params, 0, 0);
}

/* Open the music test: read OPNBK image 1 as the background, fill the 96-entry
 * track list (text ids 0xc000 + entry, flags clear) and push the music test
 * controller (handler 7) at MUSIC_TEST_STEP_OPEN as the screen is revealed.
 *
 * The list is drawn by the WORLD menu frame, which this turns on with Circle,
 * Cross and Select withheld from it and ordering-table entry 4 as its base. The
 * only caller is step 5 of new_game_update. */
void music_test_open(void) {
    s32 i;
    s32 four;
    s32 controller;

    file_request_openbk_image(1, g_file_destination);
    file_wait_for_pending();

    for (i = 0; i < 96; i++) {
        g_music_test_list_entries[i] = i + 0xc000;
        g_music_test_list_entry_flags[i] = 0;
    }

    script_set_event_speed(1);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    /* The same 4 is stored twice, as the WORLD menu frame's OT index and as MUSIC_TEST_STEP_OPEN; the local
     * keeps it in a saved register across the fade-out call instead of being rematerialized. */
    four = 4;
    g_music_test_world_menu_otag_index = four;
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
void music_test_update(controller_record_t* record) {
    music_test_state_t* state = &record->music_test;
    point32_t extent;
    point32_t origin;
    s32 buttons;

    if (state->blink_frames != 0 && ++state->blink_frames >= 10) {
        state->blink_frames = 0;
        formation_set_display_mode(0);
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
            g_music_test_sound_handle = music_switch_track(state->music_slot, 0x7F, 0x10);
            state->step = MUSIC_TEST_STEP_IDLE;
        }
    } else if (state->step == MUSIC_TEST_STEP_FADE_OUT) {
        if (!(g_gfx_overlay_fade.flags & GFX_OVERLAY_FADE_ACTIVE)) {
            g_system_runtime_flags ^= SYSTEM_RUNTIME_FLAG_INPUT_DISABLED | SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME;
            file_request_openbk_image(0, g_file_destination);
            file_wait_for_pending();
            g_controller_depth--;
            title_push_menu(0);
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
                    music_unload_scenario_slot(state->music_slot);
                }
                state->music_slot = music_load_into_free_slot(state->music_id);
                state->step = MUSIC_TEST_STEP_START_TRACK;
            } else {
                if (state->music_slot != 0) {
                    music_unload_scenario_slot(state->music_slot);
                }
                state->music_slot = 0;
            }
        }
    } else {
        /* Idle: release a track that has finished on its own, then take input. */
        if (state->music_slot != 0 && suzuki_smd_is_music_playing(g_music_test_sound_handle) == 0) {
            music_unload_scenario_slot(state->music_slot);
            state->music_slot = 0;
            state->music_id = 0;
        }
        buttons = g_input_new_button_presses;
        if (buttons & PSX_PAD_CIRCLE) {
            formation_set_display_mode(1);
            state->blink_frames = 1;
            g_menu_blocked_buttons |= PSX_PAD_UP | PSX_PAD_DOWN;
            if (state->music_id != 0) {
                state->delay = 0x20;
                music_set_target(0, 0x78);
            } else {
                state->delay = 1;
            }
            state->music_id = g_music_test_music_ids[g_music_test_list_params.selected_index];
        } else if (buttons & PSX_PAD_CROSS) {
            if (state->music_id != 0) {
                music_set_target(0, 0x78);
                state->music_id = 0;
                state->delay = 0x20;
            } else {
                state->step = MUSIC_TEST_STEP_CLOSE;
                thread_set_parameters(MUSIC_TEST_THREAD_SLOT_LIST, 0, 0, 1);
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
        text_save_section_pointers();
        text_relocate_pointer_table(g_screen_text_section_offsets);
    }

    thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_dialog_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x33, parameter, 0);

    {
        s32 controller = g_controller_depth;
        s32 saved_blocked_buttons = g_menu_blocked_buttons;

        g_menu_blocked_buttons = -1;
        g_controller_records[controller].text_message.saved_blocked_buttons = saved_blocked_buttons;
    }
    sound_play_sfx(SOUND_SFX_WINDOW_OPEN);

    {
        s32 controller = g_controller_depth;
        s32 next_controller = controller + 1;

        g_controller_handler_indices[controller] = CONTROLLER_TEXT_MESSAGE;
        g_controller_depth = next_controller;
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
        text_restore_section_pointers();
    }
}

/* Start one of the title screen's demo movies (FFTPRE, FFTPR or FFTUNIT.STR) at cd_mix_level with
 * no separate video end frame, and push handler 11 with the stream's start sector and length. */
void movie_stream_title_demo(s32 start_sector, s32 stream_length, s32 end_frame, s32 cd_mix_level) {
    s32 controller;

    movie_start_stream(start_sector, end_frame, -1, cd_mix_level);
    g_system_runtime_flags &= ~SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
    controller = g_controller_depth;
    g_controller_records[controller].movie.start_sector = start_sector;
    g_controller_records[controller].movie.stream_length = stream_length;
    g_controller_handler_indices[controller] = CONTROLLER_TITLE_DEMO_MOVIE;
    g_controller_depth = controller + 1;
}

/* Handler 11, a title demo movie: Start stops the stream. Once the stream has
 * stopped, by itself or on the update after Start, the display is blanked and the
 * controller is replaced by the FFTST.STR logo movie, which leads back to the
 * title menu. */
void movie_update_title_demo(controller_record_t* unused_record) {
    if ((g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING) == 0) {
        SetDispMask(0);
        g_gfx_active_buffer_index = 1;
        g_system_runtime_flags |= SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        g_controller_depth--;
        movie_stream_fftst(DISC_SECTOR_OPEN_FFTST_STR, 0x278a, 0x3df, 0x2de);
    } else if ((g_input_new_button_presses & PSX_PAD_START) != 0) {
        movie_stop_stream(1);
    }
}

/* New Game on the title menu. With a memory card in either slot the new-game
 * prologue starts at once. With none the frame image is saved to
 * g_system_work_buffer_0 and blanked, animation 0x12 is shown on the black screen
 * and the title-exit controller (handler 12) is pushed: Cross returns to the
 * title menu, Circle or Start starts the prologue anyway. */
void title_select_new_game(void) {
    s32* source;
    s32* destination;
    s32 index;
    s32 record;
    point32_t* position;
    s32 controller;
    s32 card_present;
    s32 frame_padding[2];

    card_present = title_check_card_slots();
    index = 0;
    if (card_present != 0) {
        prologue_start();
        return;
    }

    source = g_file_destination;
    destination = (s32*)g_system_work_buffer_0;
    for (; index < PROLOGUE_FRAME_IMAGE_WORDS; index++) {
        *destination = *source;
        *source = 0;
        source++;
        destination++;
    }

    gfx_start_overlay_fade_out(0x10);
    record = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_window_records[record].sequence = 0x12;
    g_window_records[record].anim_counter = 0;
    g_window_records[record].frame_index = 0;
    position = &g_window_records[record].position;
    controller = g_controller_depth;
    position->x = -4;
    position->y = -16;
    g_controller_records[controller].card_warning.state = 0;
    g_controller_handler_indices[controller] = CONTROLLER_TITLE_CARD_WARNING;
    g_controller_depth = controller + 1;
}

/* Per-frame handler of the title-exit controller (handler 12), pushed when New
 * Game is chosen with no memory card inserted.
 *
 * Cross, or Circle or Start, records the choice and fades the screen to black.
 * When the fade has ended the notice record and the controller are released: on
 * Cross the frame image saved in g_system_work_buffer_0 is copied back and the
 * title menu is pushed again; on Circle or Start the new-game prologue starts. */
void title_update_card_warning(controller_record_t* record) {
    s32 state;
    u32 buttons;

    if ((g_gfx_overlay_fade.flags & GFX_OVERLAY_FADE_ACTIVE) != 0) {
        return;
    }

    if (record->card_warning.state != TITLE_CARD_WARNING_INPUT) {
        g_window_record_count -= 1;
        g_window_render_object_count -= 1;
        g_controller_depth -= 1;
        state = record->card_warning.state;
        if (state == TITLE_CARD_WARNING_CANCEL) {
            s32* source;
            s32* destination;
            s32 index;

            SetDispMask(0);
            source = (s32*)g_system_work_buffer_0;
            g_system_runtime_flags |= SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
            destination = g_file_destination;
            for (index = 0; index < PROLOGUE_FRAME_IMAGE_WORDS; index++) {
                destination[index] = source[index];
            }
            title_push_menu(0);
        } else if (state == TITLE_CARD_WARNING_CONFIRM) {
            prologue_start();
        }
        return;
    }

    buttons = g_input_new_button_presses;
    if ((buttons & PSX_PAD_CROSS) != 0) {
        sound_play_sfx(SOUND_SFX_CANCEL);
        gfx_start_overlay_fade_in(0x20);
        record->card_warning.state = TITLE_CARD_WARNING_CANCEL;
    } else if ((buttons & (PSX_PAD_CIRCLE | PSX_PAD_START)) != 0) {
        sound_play_sfx(SOUND_SFX_CONFIRM);
        gfx_start_overlay_fade_in(0x20);
        record->card_warning.state = TITLE_CARD_WARNING_CONFIRM;
    }
}

/* Arm the new-game prologue sequence: start before entry 0 of entry_table with NEXT_ENTRY set, end
 * at entry target, draw its text in ordering-table entries ot_layer and ot_layer + 1, at full
 * brightness (0x80). */
void prologue_init_sequence(s32 target, s32 ot_layer, prologue_entry_t** entry_table) {
    g_prologue.flags = PROLOGUE_NEXT_ENTRY;
    g_prologue.current_index = 0;
    g_prologue.target_index = target;
    g_prologue.entry_table = entry_table;
    g_prologue.ot_layer = ot_layer;
    g_prologue.intensity = 0x80;
}

/* Empty. new_game_update calls it once the prologue has finished. */
void prologue_finished_stub(void) {
}

/* Advance the new-game prologue by one frame: the picture crossfade and the text
 * of the current prologue entry. Returns 1 while it runs and 0 when it has
 * finished.
 *
 * An entry is one page of text lines. NEXT_ENTRY loads the next entry, or
 * finishes at target_index: its line count, texture page and image operation,
 * where 0x10 + n prepares the crossfade to picture n and 0x20 waits for that
 * picture's read and starts the crossfade. NEXT_STEP starts the next line,
 * reading every line's texture row and width from the entry on the first; the
 * timer starts at -16 and the line is revealed one pixel per frame. After the
 * last line the page waits for Circle (WAIT_CIRCLE), dims from 0x80 by 2 per
 * frame (DIM) and the next entry follows.
 *
 * Start sets SKIP: the lines keep advancing while the brightness runs down, and
 * the sequence ends once it is 0 and the crossfade has stopped. */
s32 prologue_update(void) {
    u16 flags;
    s32 i;
    prologue_entry_t* entry;
    s32 image_command;

    prologue_step_crossfade();
    if (!(g_prologue.flags & PROLOGUE_SKIP)) {
        if (g_input_new_button_presses & PSX_PAD_START) {
            sound_play_sfx(SOUND_SFX_COMPLETE);
            g_prologue.flags |= PROLOGUE_SKIP;
        }
        flags = g_prologue.flags;
        if (!(flags & PROLOGUE_SKIP)) {
            if (flags & PROLOGUE_DIM) {
                g_prologue.intensity -= 2;
                prologue_draw_text_lines(g_prologue.intensity);
                if (g_prologue.intensity == 0) {
                    g_prologue.intensity = 0x80;
                    g_prologue.flags = (g_prologue.flags ^ PROLOGUE_DIM) | PROLOGUE_NEXT_ENTRY;
                }
                return 1;
            }
            if (flags & PROLOGUE_WAIT_CIRCLE) {
                if (g_input_new_button_presses & PSX_PAD_CIRCLE) {
                    g_prologue.flags = (flags ^ PROLOGUE_WAIT_CIRCLE) | PROLOGUE_DIM;
                    g_prologue.intensity = 0x80;
                }
                prologue_draw_text_lines(g_prologue.intensity);
                return 1;
            }
            if (flags & PROLOGUE_NEXT_ENTRY) {
                flags ^= PROLOGUE_NEXT_ENTRY;
                g_prologue.flags = flags;
                if (g_prologue.current_index == g_prologue.target_index) {
                    return 0;
                }
                g_prologue.flags = flags | PROLOGUE_NEXT_STEP;
                entry = g_prologue.entry_table[g_prologue.current_index];
                g_prologue.entry = entry;
                g_prologue.step_count = (u8)entry->header;
                g_prologue.tpage_x = (entry->header & 0xf00) >> 2;
                g_prologue.tpage_y = (entry->header >> 4) & 0x100;
                /* The high nibble selects the operation; the low nibble selects its image. */
                image_command = (u8)(entry->header >> 16);
                switch (image_command & PROLOGUE_IMAGE_OPERATION_MASK) {
                case PROLOGUE_PREPARE_IMAGE:
                    prologue_prepare_crossfade(image_command & PROLOGUE_IMAGE_INDEX_MASK);
                    break;
                case PROLOGUE_START_CROSSFADE:
                    file_wait_for_pending();
                    g_prologue_crossfade.active = 1;
                    break;
                }
                {
                    s16* index = &g_prologue.current_index;

                    g_prologue.step = 0;
                    (*index)++;
                }
            }
        }
    }

    flags = g_prologue.flags;
    if (flags & PROLOGUE_NEXT_STEP) {
        g_prologue.flags = flags ^ PROLOGUE_NEXT_STEP;
        /* Every line is out: the page waits for Circle. */
        if (g_prologue.step == g_prologue.step_count) {
            g_prologue.flags = (flags ^ PROLOGUE_NEXT_STEP) | (PROLOGUE_NEXT_ENTRY | PROLOGUE_WAIT_CIRCLE);
            prologue_draw_text_lines(g_prologue.intensity);
            return 1;
        }
        /* Byte indexing preserves the retail address-add order and entry reloads. */
        if (g_prologue.step == 0) {
            for (i = 0; i < g_prologue.step_count; i++) {
                g_prologue.line_widths[i]
                    = g_prologue.entry->line_data[i * sizeof(prologue_line_t) + offsetof(prologue_line_t, width)];
                g_prologue.line_texture_rows[i]
                    = g_prologue.entry->line_data[i * sizeof(prologue_line_t) + offsetof(prologue_line_t, texture_row)];
            }
        }
        {
            s16* step = &g_prologue.step;

            g_prologue.timer = -0x10;
            (*step)++;
        }
    }

    if (++g_prologue.timer == g_prologue.line_widths[g_prologue.step - 1]) {
        g_prologue.flags |= PROLOGUE_NEXT_STEP;
    }

    if (g_prologue.flags & PROLOGUE_SKIP) {
        if (g_prologue.intensity != 0) {
            g_prologue.intensity -= 2;
        } else if (g_prologue_crossfade.active == 0) {
            return 0;
        }
    }

    prologue_draw_text_lines(g_prologue.intensity);
    return 1;
}

/* Draw the text lines of the current new-game prologue entry at the given
 * brightness.
 *
 * Each step of the entry is one line of text: line_texture_rows holds its 16-pixel row
 * in the texture page and line_widths its width in pixels. The lines are
 * centered, 20 pixels apart, as a block around the middle of the screen. A
 * finished line is drawn at full width and the line in progress up to timer
 * pixels; the pass with i == step adds that line's leading edge, at most 16
 * pixels whose right-hand corners are black, so the text fades in as it is
 * revealed.
 *
 * Every piece is two POLY_GT4s: the text, added to the picture at intensity in
 * ordering-table entry ot_layer, and a copy one pixel down and right, subtracted at
 * half intensity in entry ot_layer + 1.
 *
 * The coordinates are written in setXY4 order (x0, y0, x1, y1, ...) per
 * primitive, and width + 1 goes through its own variable (the target adds left
 * afterwards). */
static void prologue_draw_text_lines(s32 intensity) {
    POLY_GT4* prims;
    s32 i;
    s32 prim_index;
    s32 width;
    s32 width_plus_one;
    s32 left;
    s32 top_y;
    s32 u;
    s32 v;
    u16 clut;

    for (i = 0; i < g_prologue.step + 1; i++) {
        prim_index = i * 2;
        prims = g_prologue_line_primitives[g_gfx_active_buffer_index];
        SetPolyGT4(&prims[prim_index]);
        SetPolyGT4(&prims[prim_index + 1]);

        if (i == g_prologue.step) {
            /* Leading edge of the line in progress (line i - 1): black on its right. */
            if (g_prologue.line_widths[i - 1] < g_prologue.timer) {
                continue;
            }
            width = g_prologue.line_widths[i - 1] - g_prologue.timer;
            if (g_prologue.timer < 0) {
                width = g_prologue.timer + 0x10;
            } else if (width >= 0x11) {
                width = 0x10;
            }
            u = g_prologue.timer;
            if (u < 0) {
                u = 0;
            }
            v = g_prologue.line_texture_rows[i - 1] * 0x10;
            setRGB1(&prims[prim_index], 0, 0, 0);
            setRGB1(&prims[prim_index + 1], 0, 0, 0);
            setRGB3(&prims[prim_index], 0, 0, 0);
            setRGB3(&prims[prim_index + 1], 0, 0, 0);
            left = u - g_prologue.line_widths[i - 1] / 2;
            top_y = (i - 1) * 0x14 - g_prologue.step_count * 10;
        } else {
            /* Line i: the revealed part of the line in progress, or a whole finished line. */
            if (i == g_prologue.step - 1) {
                width = g_prologue.timer;
            } else {
                width = g_prologue.line_widths[i];
            }
            u = 0;
            if (width <= 0) {
                continue;
            }
            v = g_prologue.line_texture_rows[i] * 0x10;
            setRGB1(&prims[prim_index], intensity, intensity, intensity);
            setRGB1(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
            setRGB3(&prims[prim_index], intensity, intensity, intensity);
            setRGB3(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
            left = -(g_prologue.line_widths[i] / 2);
            top_y = i * 0x14 - g_prologue.step_count * 10;
        }

        setRGB0(&prims[prim_index], intensity, intensity, intensity);
        setRGB0(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
        setRGB2(&prims[prim_index], intensity, intensity, intensity);
        setRGB2(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);

        clut = GetClut(0, 0x1e3);
        prims[prim_index + 1].clut = clut;
        prims[prim_index].clut = clut;
        prims[prim_index].tpage = GetTPage(0, 1, g_prologue.tpage_x, g_prologue.tpage_y);
        prims[prim_index + 1].tpage = GetTPage(0, 2, g_prologue.tpage_x, g_prologue.tpage_y);

        setXY4(&prims[prim_index], left, top_y, left + width, top_y, left, top_y + 0x10, left + width, top_y + 0x10);
        width_plus_one = width + 1;
        setXY4(&prims[prim_index + 1], left + 1, top_y + 1, left + width_plus_one, top_y + 1, left + 1, top_y + 0x11,
            left + width_plus_one, top_y + 0x11);
        setUV4(&prims[prim_index], u, v, u + width, v, u, v + 0x10, u + width, v + 0x10);
        setUV4(&prims[prim_index + 1], u, v, u + width, v, u, v + 0x10, u + width, v + 0x10);

        SetSemiTrans(&prims[prim_index], 1);
        SetSemiTrans(&prims[prim_index + 1], 1);
        AddPrim(&g_gfx_otags[g_gfx_active_buffer_index][g_prologue.ot_layer], &prims[prim_index]);
        AddPrim(&g_gfx_otags[g_gfx_active_buffer_index][g_prologue.ot_layer + 1], &prims[prim_index + 1]);
    }
}

/* Prepare the crossfade of the new-game prologue's picture to OPNBK image
 * file_index + 2.
 *
 * Points the crossfade at its three buffers, starts the asynchronous read of the
 * new image into g_system_work_buffer_0 (the crossfade's source) and copies the
 * 210x180 picture on screen out of the frame image into g_system_work_buffer_1,
 * packed 210 pixels per row. It clears the per-row fade steps, the running flag
 * and the blend band's end row; image operation 0x20 of a later prologue entry waits for
 * the read and sets the running flag. */
static void prologue_prepare_crossfade(s32 file_index) {
    u16* copy_source;
    u16* copy_destination;
    u16* file_pixels;
    s32 row;
    s32 source_row;
    s32 column;
    s32 source_offset;
    s32 destination_index;
    s32 source_index;
    u16* pixels;
    u16 value;
    u16(*file_image)[256];

    copy_source = (u16*)g_system_work_buffer_0;
    copy_destination = (u16*)g_system_work_buffer_1;
    file_image = g_file_destination;
    /* The 210x180 picture begins at row 43 of the 256-pixel frame image. */
    file_pixels = file_image[43];
    g_prologue_crossfade_new_picture = copy_source;
    g_prologue_crossfade_old_picture = copy_destination;
    g_prologue_crossfade_frame_pixels = file_pixels;
    file_request_openbk_image(file_index + 2, g_system_work_buffer_0);

    row = 0;
    source_row = 0;
    for (; row < PROLOGUE_IMAGE_HEIGHT; row++) {
        column = 0;
        source_offset = row * PROLOGUE_IMAGE_ROW_STRIDE;
        while (column < PROLOGUE_IMAGE_WIDTH) {
            destination_index = source_row + column;
            source_index = source_offset + column++;
            destination_index <<= 1;
            pixels = g_prologue_crossfade_frame_pixels;
            value = pixels[source_index];
            /* Destination indexing hoists pointer loads; byte-pointer arithmetic changes registers. */
            *(u16*)(destination_index + (u32)g_prologue_crossfade_old_picture) = value;
        }
        g_prologue_crossfade.row_steps[row] = 0;
        source_row += PROLOGUE_IMAGE_WIDTH;
    }

    g_prologue_crossfade.active = 0;
    g_prologue_crossfade.band_end_row = 0;
}

#define PROLOGUE_IMAGE_BLEND_BAND_HEIGHT 17
#define PROLOGUE_IMAGE_BLEND_END_ROW     (PROLOGUE_IMAGE_HEIGHT + PROLOGUE_IMAGE_BLEND_BAND_HEIGHT)

/*
 * Advance the bottom-up crossfade between the two 210x180 opening images by one row.
 *
 * Each call blends the 17-row band immediately before band_end_row: every row in the
 * band steps its own fade counter and writes the per-component mix of the source
 * and destination pixels into the file image.
 *
 * The reverse row is computed inside the row test, which makes it local to that
 * block. GCC 2.7.2's local-alloc orders exactly three block quantities with a
 * fixed-index comparison that would allocate the first half of the *210 multiply
 * first; with four it sorts by priority and the second half gets $v0, as in the
 * target. The s16 components are copied for the compare and the difference,
 * while the sums use the originals.
 */
static void prologue_step_crossfade(void) {
    s32 row;
    s32 last_row;
    s32 reverse_row;
    s32 column;
    s32 source_row_start;
    s32 destination_row_start;
    u16 source_pixel;
    u16 destination_pixel;
    u16 pixel;
    s32 component;
    s16 source_component;
    s16 destination_component;
    s32 previous_components;
    s32 difference;
    u8 fade;

    if (g_prologue_crossfade.active != 0) {
        row = g_prologue_crossfade.band_end_row - PROLOGUE_IMAGE_BLEND_BAND_HEIGHT;
        if (row < g_prologue_crossfade.band_end_row) {
            last_row = PROLOGUE_IMAGE_HEIGHT - 1;
            do {
                if (row >= 0 && row < PROLOGUE_IMAGE_HEIGHT) {
                    reverse_row = last_row - row;
                    column = 0;
                    source_row_start = reverse_row * PROLOGUE_IMAGE_WIDTH;
                    destination_row_start = reverse_row * PROLOGUE_IMAGE_ROW_STRIDE;
                    fade = g_prologue_crossfade.row_steps[row]++;
                    do {
                        /* An ordinary source-pointer load is hoisted and adds a saved-register frame. */
                        source_pixel = g_prologue_crossfade_new_picture[source_row_start + column];
                        destination_pixel = g_prologue_crossfade_old_picture[source_row_start + column];
                        pixel = 0;
                        if (source_pixel != destination_pixel) {
                            for (component = 2; component >= 0; component--) {
                                previous_components = pixel >> 5;
                                source_component = source_pixel & 0x1F;
                                destination_component = destination_pixel & 0x1F;
                                if (source_component != destination_component) {
                                    difference = source_component - destination_component;
                                    pixel = previous_components
                                        | ((destination_component + (s16)((s16)(difference << 8) / 16 * fade) / 256)
                                            << 10);
                                } else {
                                    pixel = previous_components | (source_component << 10);
                                }
                                source_pixel >>= 5;
                                destination_pixel >>= 5;
                            }
                        } else {
                            pixel = source_pixel;
                        }
                        g_prologue_crossfade_frame_pixels[destination_row_start + column]
                            = pixel | (destination_pixel & 0x8000);
                        column++;
                    } while (column < PROLOGUE_IMAGE_WIDTH);
                }
                row++;
            } while (row < g_prologue_crossfade.band_end_row);
        }
        {
            u8* band_end_row = &g_prologue_crossfade.band_end_row;

            if (++*band_end_row >= PROLOGUE_IMAGE_BLEND_END_ROW) {
                g_prologue_crossfade.active = 0;
            }
        }
    }
}
