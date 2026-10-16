/* Controller reading and update, input with message state, and the memory card screen, including slot, mode, load and
 * save steps, the save image, slot scans, playtime, descriptions and the slot list. */

#include "fft/bits.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/main.h"
#include "fft/main_card.h"
#include "fft/main_file.h"
#include "fft/main_item.h"
#include "fft/main_save.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/memcard.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_help.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/world.h"
#include "fft/world_card.h"
#include "fft/world_formation.h"
#include "fft/world_gfx.h"
#include "fft/world_menu.h"
#include "fft/world_script.h"
#include "fft/world_text.h"
#include "psx/kernel.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* The memory card title's start in Shift-JIS: full-width "FFT FILE" and three spaces, the first
 * two of which take the save number (bytes 0x10..0x13); the play time follows. */
static u8 g_save_title_template[0x16]
    = { 130, 101, 130, 101, 130, 115, 129, 64, 130, 101, 130, 104, 130, 107, 130, 100, 129, 64, 129, 64, 129, 64 };

static void save_build_file_header(s32 slot, s32 level, save_file_header_t* header);
static s32 save_build_slot_description(s32 mode, u8* destination);
static s32 save_count_selectable_slots(void);
static void save_draw_slot_text(s32 save_slot_index);
static void save_load_step(s32 slot);
static void save_mode_select_step(void);
static void save_write_step(s32 slot);
static void save_card_select_step(void);
static void save_scroll_slot_list(s32 row);
static void save_slot_list_step(s32 input);
static void save_update_slot_playtime(s32 slot);
static void save_scan_slots(void);
static void save_free_work_buffer(void);

/* Return the controller buttons from PadRead(0); the argument is ignored. */
static s32 screen_input_read_pad(s32 unused) {
    return PadRead(0);
}

/*
 * Publish newly pressed buttons and two repeat rates for WORLD menus, reading
 * pad 0 (tutorial_update_input is the tutorial twin).
 *
 * Triangle reports one press and nothing while held. Otherwise one button is
 * reported at a time: of several, the lowest button bit is latched and
 * reported alone until it is released. After a release, a new press counts
 * only once the activation timer reaches 4 (VSyncs), except a D-pad press,
 * which counts at once. Counter slots 4..11 (face buttons, Select, Start)
 * are skipped; those buttons use only the newly pressed mask, and bits 16-31
 * flag a button held past the initial delay.
 */
static void screen_input_update_pad(void) {
    s16 bit;
    s32 index;
    u32 current_input;
    u32 filtered_input;
    s32* counter_base;
    u32 speed;
    u32 initial_delay;
    u32 secondary_repeat_period;
    u32 repeat_period;

    speed = menu_get_event_speed();
    initial_delay = g_input_repeat_initial_delay / speed;
    repeat_period = g_input_repeat_period / speed;
    secondary_repeat_period = g_input_secondary_repeat_period / speed;
    current_input = screen_input_read_pad(0);
    if (current_input & PSX_PAD_TRIANGLE) {
        if (g_screen_input_triangle_latch != 0) {
            menu_input_clear_state();
        } else {
            g_screen_input_triangle_latch = 1;
            g_menu_input_newly_pressed = PSX_PAD_TRIANGLE;
            g_menu_input_secondary_repeat = PSX_PAD_TRIANGLE;
            g_menu_input_primary_repeat = PSX_PAD_TRIANGLE;
        }
    } else {
        g_screen_input_triangle_latch = 0;
        if (current_input & g_screen_input_latched_button) {
            current_input &= g_screen_input_latched_button;
        } else if (current_input == 0) {
            g_screen_input_latched_button = 0;
            g_input_activation_timer += speed;
        } else {
            if (g_input_activation_timer < 4) {
                if (current_input & PSX_PAD_DPAD_MASK) {
                    current_input &= PSX_PAD_DPAD_MASK;
                    g_input_activation_timer = 10;
                }
            }
            if (g_input_activation_timer >= 4) {
                g_input_activation_timer = 0;
                for (index = 0; index < 16; index++) {
                    bit = 1U << index;
                    if (current_input & bit) {
                        current_input &= bit;
                        g_screen_input_latched_button = bit;
                    }
                }
            } else {
                current_input = 0;
                g_screen_input_latched_button = 0;
            }
        }
        filtered_input = current_input;
        if (g_input_activation_timer >= 10) {
            g_input_activation_timer = 10;
        }
        g_menu_input_newly_pressed = filtered_input;
        for (index = 0; index < 16; index++) {
            s32 mask = 1U << index;

            if (mask & g_menu_input_newly_pressed) {
                if (mask & g_screen_input_previous) {
                    g_menu_input_newly_pressed &= ~mask;
                } else {
                    g_screen_input_previous |= mask;
                }
            } else {
                g_screen_input_previous &= ~mask;
            }
        }
        counter_base = g_input_repeat_counters;
        for (index = 0; index < 16; index++) {
            if (index == 4) {
                index = 12;
            }
            if (current_input & (1U << index)) {
                counter_base[index]++;
            } else {
                counter_base[index] = 0;
            }
            if ((counter_base[index] >= initial_delay && counter_base[index] % repeat_period != 0)
                || (counter_base[index] >= 2 && counter_base[index] < initial_delay)) {
                current_input &= ~(1U << index);
            }
            if (counter_base[index] >= initial_delay) {
                current_input |= 1U << (index + 16);
            }
            if ((counter_base[index] >= initial_delay && counter_base[index] % secondary_repeat_period != 0)
                || (counter_base[index] >= 2 && counter_base[index] < initial_delay << 1)) {
                filtered_input &= ~(1U << index);
            }
        }
        current_input &= 0xf00ff00f;
        filtered_input &= 0xf00ff00f;
        current_input |= g_menu_input_newly_pressed & 0x0ff0;
        filtered_input |= g_menu_input_newly_pressed & 0x0ff0;
        g_menu_input_primary_repeat = current_input;
        g_menu_input_secondary_repeat = filtered_input;
    }
    g_screen_input_primary_repeat_latched = g_menu_input_primary_repeat;
    g_screen_input_secondary_repeat_latched = g_menu_input_secondary_repeat;
}

/*
 * Per-frame input update of the memory card screen: the body of
 * menu_input_update_with_message_state written a second time, reading
 * the pad through screen_input_update_pad instead of the tutorial
 * reader.
 *
 * Clears the 32-bit input masks during steps 1..3 of the window scale
 * animation, while the text thread runs or a help grid is open, on the frame
 * the text ends and during a screen fade. The text phase goes 1 on the first
 * frame of a text task (which queues the window-open sound), 2 after it and 0
 * otherwise. When the text thread is not running, the input lock and the
 * held-loop sound release are cleared.
 */
static void save_input_update(void) {
    s32 running = 0;
    s32 step;
    u8 value;

    screen_input_update_pad();
    step = menu_get_window_scale_step();
    if (step >= 1 && step <= 3) {
        menu_input_clear_state();
    } else {
        running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
        if (running != 0 || g_screen_grid_id != MENU_GRID_HELP_NONE) {
            menu_input_clear_state();
            if (running != 0 && g_menu_input_lock_mode != 0) {
                value = 1;
                if (g_screen_input_blocked_state != 0) {
                    value = 2;
                }
                g_screen_input_blocked_state = value;
            } else {
                g_screen_input_blocked_state = 0;
            }
        } else if (g_screen_input_blocked_state != 0 || screen_fade_get_state() != SCREEN_FADE_STATE_NONE) {
            g_screen_input_blocked_state = 0;
            menu_input_clear_state();
        }
    }
    if (running == 0) {
        g_menu_input_lock_mode = 0;
        g_screen_sound_release_held_loop = 0;
    }
    if (g_screen_input_blocked_state == 1) {
        g_sound_queued_effect_id = SOUND_SFX_WINDOW_OPEN;
    }
}

/* Run the memory card save/load screen to completion on its own stack-allocated primitive pools
 * and return its result. mode_select_enabled is 1 when opened from the world map and 0 from the
 * title screen. */
s32 save_run_screen(s32 mode_select_enabled) {
    POLY_FT4 textured_quads[2 * 600];
    TILE tiles[2 * 10];
    DR_MOVE draw_moves[2 * 30];
    DR_MODE draw_modes[2 * 30];
    DR_AREA draw_areas[2 * 10];
    u32 otags[2 * 64];
    menu_gfx_context_t buffers[2];
    u8 unused_packet_gap[0x10]; /* Retains the original unused stack space. */
    DIRENTRY directory_buffer[CARD_SAVE_SLOT_COUNT];
    s16 music_handle;
    u16 input;
    s32 buttons;
    s32 step;

    g_screen_ot_length = 0x40;
    g_screen_textured_quad_capacity = 0x258;
    g_gfx_poly_f3_capacity = 0;
    g_gfx_poly_ft3_capacity = 0;
    g_gfx_poly_f4_capacity = 0;
    g_gfx_poly_g3_capacity = 0;
    g_gfx_poly_gt3_capacity = 0;
    g_screen_gradient_quad_capacity = 0;
    g_screen_textured_gradient_quad_capacity = 0;
    g_screen_flat_line_capacity = 0;
    g_gfx_line_f3_capacity = 0;
    g_gfx_line_f4_capacity = 0;
    g_screen_gradient_line_capacity = 0;
    g_screen_line_g3_capacity = 0;
    g_gfx_line_g4_capacity = 0;
    g_gfx_tile_capacity = 10;
    g_gfx_tile_1_capacity = 0;
    g_gfx_tile_8_capacity = 0;
    g_gfx_tile_16_capacity = 0;
    g_gfx_sprite_capacity = 0;
    g_gfx_sprite_8_capacity = 0;
    g_gfx_sprite_16_capacity = 0;
    g_screen_draw_move_capacity = 30;
    g_screen_draw_area_capacity = 10;
    g_gfx_draw_mode_capacity = 30;
    screen_init_packet_buffers(buffers, otags, 0, 0, 0, textured_quads, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, tiles, 0, 0, 0, 0,
        0, 0, draw_moves, draw_areas, draw_modes);
    formation_init_display();
    g_formation_unit_count = formation_build_record_list(0, g_formation_unit_pointers, FORMATION_RECORD_LIST_ALL);
    g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN] = g_save_slot_text_table;
    screen_text_bind_section(SCREEN_TEXT_DATA_FORMATION_MESSAGES);
    g_save_menu_step = SAVE_STEP_SELECT_CARD;
    /* 1 from the world menu, 0 from the title screen's Continue (load only). */
    g_save_mode_select_enabled = mode_select_enabled;
    music_handle = music_open_into_free_slot(MUSIC_SAVE);
    while (music_is_loading_stub() != 0) { }
    music_switch_track(music_handle, 0x7f, 0);
    memcard_consume_all_events();
    g_save_directory_buffer = directory_buffer;
    save_init_screen();
    thread_start(THREAD_BORROWED_SLOT_CARD_BACKDROP, formation_zodiac_backdrop_thread);
    thread_set_parameters(THREAD_BORROWED_SLOT_CARD_BACKDROP, 0, 0, 0);
    while (screen_fade_update_out() != 0) {
        g_tutorial_id = 0;
        screen_present_frame(0, 0);
        save_input_update();
        screen_fade_update_in();
        gfx_enqueue_tile(&g_save_screen_background_tile, &g_screen_black_tile_color, 0, 0);
        if (g_save_menu_step == SAVE_STEP_SELECT_SAVE && thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) == 0) {
            input = g_menu_input_primary_repeat;
        } else {
            input = 0;
        }
        if (g_save_menu_step >= 3 && g_save_menu_step <= 5) {
            save_slot_list_step(input);
        }
        step = g_save_menu_step;
        if (step == SAVE_STEP_SELECT_CARD) {
            save_card_select_step();
        } else if (step == SAVE_STEP_SCANNING) {
            save_scan_slots();
        } else if (step == SAVE_STEP_SELECT_MODE) {
            save_mode_select_step();
        } else if (step == SAVE_STEP_LOADING) {
            save_load_step(g_save_active_slot);
        } else if (step == SAVE_STEP_SAVING) {
            save_write_step(g_save_active_slot);
        } else if (step == SAVE_STEP_LEAVE) {
            music_set_target(0, 0xf0);
            while (memcard_poll_operation_result() == MEMCARD_RESULT_PENDING) { }
            screen_fade_start_increasing();
            g_save_menu_step = SAVE_STEP_LEFT;
        }
        formation_set_backdrop_otag(g_menu_gfx_context->otag + 1);
        g_gfx_current_otag_entry = g_menu_gfx_context->otag + 60;
        menu_draw_active_window_frames();
        buttons = PadRead(0) & PSX_PAD_DPAD_MASK;
        buttons |= g_menu_input_newly_pressed & 0xfff;
        if (thread_is_running(THREAD_SCREEN_SLOT_TEXT) == 0 && thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) == 0
            && thread_is_running(SCREEN_THREAD_SLOT_UPPER_MENU) == 0) {
            buttons &= 0xff00;
        }
        menu_input_update_event_state(
            g_menu_gfx_context->otag + 60, buttons, g_screen_draw_buffer_clip_y == 0 ? 0xf0 : 0);
        screen_sound_dispatch_effect();
    }
    thread_set_parameters(THREAD_BORROWED_SLOT_CARD_BACKDROP, 0, -1, 0);
    save_free_work_buffer();
    save_close_stub();
    music_unload_scenario_slot(music_handle);
    screen_reset_context();
    formation_build_roster_names();
    return g_save_menu_result;
}

/* Memory card slot selection step: waits 0x28 frames, opens the card choice menu in the screen
 * menu slot, then applies the chosen card slot (g_screen_selection_results[5]) and starts
 * scanning it, or leaves the screen on cancel. */
static void save_card_select_step(void) {
    s16* selected;
    s32 slot;

    if (g_save_slot_select_running == 0) {
        if (g_save_slot_select_timer < 0x28) {
            g_save_slot_select_timer++;
            return;
        }
        g_save_slot_select_running = 1;
        g_save_mode = 0xff;
        g_save_slot_cursor_row = 0;
        g_screen_selection_results[5] = -1;
        g_save_slot_select_menu[1].selected_index = g_save_selected_slot;
        return;
    }
    g_save_slot_select_running = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_save_slot_select_menu);
    if (g_save_slot_select_running == 0) {
        selected = &g_screen_selection_results[5];
        slot = *selected;
        if (slot == -1) {
            g_save_menu_step = SAVE_STEP_LEAVE;
        } else {
            g_text_substitution_values[0] = slot + 1;
            if (g_save_selected_slot != slot) {
                g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
            }
            g_save_selected_slot = *selected;
            memcard_set_selected_slot(slot);
            g_save_menu_step = SAVE_STEP_SCANNING;
        }
    }
}

/*
 * Memory card menu state 1: choose between loading and saving.
 *
 * g_screen_selection_results[5] receives the menu choice (0 save, 1 load, -1 cancel); a zero
 * g_save_mode_select_enabled (the title's Continue passes 0) skips the menu and loads. WORLD
 * reads and writes that flag as 16 bits, while CARD.OUT stores it as a byte (`sb`) and reads it
 * with `lb`/`lbu`. Saving when every slot state is -1,
 * or loading when no slot state is 0, shows a notice thread instead of the slot
 * list (the same predicates save_count_selectable_slots counts). `i` is reused to hold the
 * previous save mode.
 */
static void save_mode_select_step(void) {
    s32 slot;
    s32 i;
    s32 all;

    if (memcard_poll_operation_result() > MEMCARD_RESULT_COMPLETE) {
        g_save_pending_message = 0;
        g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
        menu_stop_thread_and_wait(SCREEN_THREAD_SLOT_UPPER_MENU);
        menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
        g_save_mode_select_running = 0;
        g_save_menu_step = SAVE_STEP_SCANNING;
        return;
    }
    if (g_save_pending_message != 0) {
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, g_save_pending_message);
        if (g_menu_input_primary_repeat & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            g_save_pending_message = 0;
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
            g_save_menu_step = SAVE_STEP_SELECT_CARD;
        }
        return;
    }
    if (g_save_mode_select_running == 0) {
        g_screen_selection_results[5] = -1;
        g_save_mode_select_menu.selected_index = 0;
        g_save_mode_select_running = 1;
    }
    if (g_save_mode_select_enabled == 0) {
        g_screen_selection_results[5] = 1;
        g_save_mode_select_running = 0;
    } else {
        g_save_mode_select_running = menu_start_managed_thread(SCREEN_THREAD_SLOT_UPPER_MENU, &g_save_mode_select_menu);
    }
    if (g_save_mode_select_running != 0) {
        return;
    }
    slot = g_screen_selection_results[5];
    if (slot == -1) {
        g_save_menu_step = SAVE_STEP_SELECT_CARD;
        return;
    }
    if (slot == 0) {
        for (i = 0, all = 1; i < 15; i++) {
            if (g_save_slot_file_states[i] != SAVE_SLOT_UNAVAILABLE) {
                all = 0;
            }
        }
        if (all) {
            g_save_pending_message = &g_save_unavailable_message;
        }
    } else {
        for (i = 0, all = 1; i < 15; i++) {
            if (g_save_slot_file_states[i] == SAVE_SLOT_OCCUPIED) {
                all = 0;
            }
        }
        if (all) {
            g_save_pending_message = &g_save_no_data_message;
        }
    }
    if (g_save_pending_message == 0) {
        i = g_save_mode;
        g_save_mode = g_screen_selection_results[5];
        g_save_menu_step = SAVE_STEP_SELECT_SAVE;
        if (i != g_save_mode) {
            g_save_list_scroll_y = 0x18;
            g_save_list_scroll_velocity = 0;
            g_save_list_first_visible_row = 0;
            g_save_active_slot = 0;
            g_save_slot_cursor_row = 0;
        }
        g_save_selectable_slot_count = save_count_selectable_slots();
        return;
    }
    g_sound_queued_effect_id = SOUND_SFX_ERROR_MESSAGE;
}

/*
 * Advance the selected save slot's read operation and error prompt.
 *
 * The read starts after the loading message has been displayed. A complete
 * read must also pass save_restore_image before the loaded-slot transition.
 */
static void save_load_step(s32 slot) {
    u32 state = g_save_load_step_state;
    s32 input;
    s32 bytes_read;

    if (state == 0) {
        g_save_load_step_state = 1;
        g_save_load_failed = 0;
        g_save_load_unread_flag = 0;
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, &g_save_loading_message);
        memset(g_save_work_buffer, 0xFF, 0x1E80);
        return;
    }
    if (state < 2) {
        g_save_load_step_state = state + 1;
        return;
    }
    if (g_save_load_failed != 0) {
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, &g_save_load_error_message);
        input = g_menu_input_primary_repeat;
        if ((input & PSX_PAD_CIRCLE) || (input & PSX_PAD_CROSS)) {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_save_load_step_state = 0;
            g_save_menu_step = SAVE_STEP_SCANNING;
        }
    } else {
        bytes_read = save_read_file_chunked(g_save_file_names[slot], (u8*)g_save_work_buffer, 0x1E00);
        menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
        if (bytes_read == 0x1E00) {
            if (save_restore_image(0) != 0) {
                g_save_menu_result = 1;
                g_save_load_step_state = 0;
                g_save_menu_step = SAVE_STEP_LEAVE;
                script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
                g_sound_queued_effect_id = SOUND_SFX_COMPLETE;
                g_save_last_used_slot = slot;
                return;
            }
        } else {
            g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
        }
        g_save_load_failed = 1;
        g_sound_queued_effect_id = SOUND_SFX_ERROR_MESSAGE;
    }
}

/*
 * Advance the selected save slot's write operation and error prompt.
 *
 * The 0x1E00-byte save image is written first; on success the slot number
 * is stamped into the header, and the 0x80-byte block at +0x100 is
 * rewritten in place. A complete write refreshes the slot's cached
 * metadata and graphic, otherwise the error prompt is shown.
 */
static void save_write_step(s32 slot) {
    u32 state = g_save_write_step_state;
    s32 input;
    s32 bytes_written;
    s32 header_written;
    s32 descriptor;
    s32 unused[6]; /* unreferenced locals: the target frame is 0x40 */

    if (state == 0) {
        g_save_write_step_state = 1;
        g_save_write_failed = 0;
        g_save_write_unread_flag = 0;
        save_build_image(slot);
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, &g_save_saving_message);
        return;
    }
    if (state < 2) {
        g_save_write_step_state = state + 1;
        return;
    }
    if (g_save_write_failed != 0) {
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, &g_save_write_error_message);
        input = g_menu_input_primary_repeat;
        if ((input & PSX_PAD_CIRCLE) || (input & PSX_PAD_CROSS)) {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_save_write_step_state = 0;
            g_save_menu_step = SAVE_STEP_SCANNING;
        }
    } else {
        bytes_written = memcard_file_write_buffer(
            g_save_file_names[slot], g_save_work_buffer, 0x1E00, g_save_slot_file_states[slot]);
        header_written = 0;
        menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
        if (bytes_written == 0x1E00) {
            g_save_work_buffer->summary.slot = slot;
            descriptor = memcard_file_open_selected_with_retries(g_save_file_names[slot], 2);
            if (descriptor >= 0) {
                header_written = memcard_file_seek_with_retries(descriptor, 0x100, 0);
                if (header_written != -1) {
                    header_written = memcard_file_write_with_retries(descriptor, &g_save_work_buffer->summary, 0x80);
                }
                if (memcard_file_close_with_retries(descriptor) == 0) {
                    header_written = 0;
                }
            }
            if (bytes_written == 0x1E00 && header_written == 0x80) {
                g_save_menu_step = SAVE_STEP_SELECT_SAVE;
                g_save_slot_file_states[slot] = SAVE_SLOT_OCCUPIED;
                save_build_slot_description(0, g_save_slot_descriptions[slot]);
                save_draw_slot_text(slot);
                bcopy(&g_save_work_buffer->summary, &g_save_slot_metadata[slot], sizeof(save_summary_t));
                save_update_slot_playtime(slot);
                g_sound_queued_effect_id = SOUND_SFX_COMPLETE;
                g_save_write_step_state = 0;
                g_save_last_used_slot = slot;
                return;
            }
        }
        g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
        g_save_write_failed = 1;
        g_sound_queued_effect_id = SOUND_SFX_ERROR_MESSAGE;
    }
}

/*
 * WORLD twin of CARD.OUT's save_build_image: serialize the whole
 * 0x1e00-byte save image into g_save_work_buffer, then append one
 * parity bit per 128-byte block through the primary bitstream writer.
 *
 * The descriptor block at +0x100 takes its leader from the first formation
 * unit that is Ramza (sprite set below CHARACTER_IDENTITY_RAMZA_END), or
 * formation unit 0 when there is none (the CARD twin falls back to the last
 * roster slot), the date and location from their script variables and the
 * play time from the MAIN second/minute/hour counters. `slot` -1 (the
 * tutorial's snapshot) marks the descriptor's slot 0 instead of 0xFF.
 * Everything from +0x124 on is a straight bcopy of the SCUS globals, in the
 * same order the CARD twin uses.
 */
void save_build_image(s32 slot) {
    s32 i;
    s32 j;
    s32 day;
    s32 month;
    s32 location;
    s32 slot_marker;
    u8* block;
    u8 parity;
    s32 unused[64]; /* unreferenced locals: the target frame is 0x130 */

    for (i = 0; i < g_formation_unit_count; i++) {
        if (g_formation_unit_pointers[i]->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
            break;
        }
    }
    if (i == g_formation_unit_count) {
        i = 0;
    }

    bcopy(
        g_formation_unit_pointers[i]->name, g_save_work_buffer->summary.name, sizeof(g_save_work_buffer->summary.name));
    g_save_work_buffer->summary.name_terminator = 0xfe;
    g_save_work_buffer->summary.job_id = g_formation_unit_pointers[i]->identity.job_id;
    g_save_work_buffer->summary.level = g_formation_unit_pointers[i]->status.level;

    day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
    month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    g_save_work_buffer->summary.date.day = day;
    g_save_work_buffer->summary.date.month = month;
    location = script_get_variable(EVENT_SCRIPT_VAR_LOCATION);
    slot_marker = (slot >= 0) ? 0xff : 0;
    g_save_work_buffer->summary.location = location;
    g_save_work_buffer->summary.slot = slot_marker;
    g_save_work_buffer->elapsed_seconds
        = g_save_play_time_minutes * 60 + g_save_play_time_seconds + g_save_play_time_hours * 3600;
    g_save_work_buffer->summary.format_version = 4;

    save_build_file_header(slot, g_formation_unit_pointers[i]->status.level, &g_save_work_buffer->header);

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        bcopy(&g_party_units[i], g_save_work_buffer->party_records[i], sizeof(g_save_work_buffer->party_records[i]));
    }

    bcopy(g_item_quantities, g_save_work_buffer->item_quantities, sizeof(g_save_work_buffer->item_quantities));
    bcopy(g_item_poached_quantities, g_save_work_buffer->poached_item_quantities,
        sizeof(g_save_work_buffer->poached_item_quantities));
    bcopy(g_item_location_flags, g_save_work_buffer->item_location_flags,
        sizeof(g_save_work_buffer->item_location_flags));
    g_save_work_buffer->_unknown_1c88 = g_save_unread_byte;
    bcopy(
        g_script_variable_storage, g_save_work_buffer->script_variables, sizeof(g_save_work_buffer->script_variables));
    bcopy(&g_game_options, &g_save_work_buffer->options, sizeof(g_save_work_buffer->options));
    bcopy(g_item_type_order_tables.order_0, g_save_work_buffer->item_type_order_0,
        sizeof(g_save_work_buffer->item_type_order_0));
    bcopy(g_item_type_order_1, g_save_work_buffer->item_type_order_1, sizeof(g_save_work_buffer->item_type_order_1));
    bcopy(g_item_type_order_2, g_save_work_buffer->item_type_order_2, sizeof(g_save_work_buffer->item_type_order_2));
    bcopy(g_item_type_order_3, g_save_work_buffer->item_type_order_3, sizeof(g_save_work_buffer->item_type_order_3));
    bcopy(g_item_type_order_4, g_save_work_buffer->item_type_order_4, sizeof(g_save_work_buffer->item_type_order_4));
    bcopy(g_item_type_order_5, g_save_work_buffer->item_type_order_5, sizeof(g_save_work_buffer->item_type_order_5));
    bcopy(g_item_type_order_6, g_save_work_buffer->item_type_order_6, sizeof(g_save_work_buffer->item_type_order_6));
    bcopy(
        g_item_weapon_page_order, g_save_work_buffer->weapon_page_order, sizeof(g_save_work_buffer->weapon_page_order));
    bcopy(
        g_item_helmet_page_order, g_save_work_buffer->helmet_page_order, sizeof(g_save_work_buffer->helmet_page_order));
    bcopy(g_item_armor_page_order, g_save_work_buffer->armor_page_order, sizeof(g_save_work_buffer->armor_page_order));
    bcopy(g_item_accessory_page_order, g_save_work_buffer->accessory_page_order,
        sizeof(g_save_work_buffer->accessory_page_order));
    bcopy(g_item_page_order, g_save_work_buffer->item_page_order, sizeof(g_save_work_buffer->item_page_order));
    bcopy(g_brave_story_treasure_acquisition_date_bits, g_save_work_buffer->treasure_acquisition_date_bits,
        sizeof(g_save_work_buffer->treasure_acquisition_date_bits));
    bcopy(g_brave_story_land_discovery_date_bits, g_save_work_buffer->land_discovery_date_bits,
        sizeof(g_save_work_buffer->land_discovery_date_bits));
    bcopy(g_proposition_last_attempt_date_bits, g_save_work_buffer->proposition_last_attempt_date_bits,
        sizeof(g_save_work_buffer->proposition_last_attempt_date_bits));
    bcopy(g_proposition_states, g_save_work_buffer->proposition_states, sizeof(g_save_work_buffer->proposition_states));
    bcopy(g_sound_novel_saved_readings, g_save_work_buffer->sound_novel_saved_records,
        sizeof(g_save_work_buffer->sound_novel_saved_records));
    bcopy(g_sound_novel_read_section_bits, g_save_work_buffer->sound_novel_read_section_bits,
        sizeof(g_save_work_buffer->sound_novel_read_section_bits));
    bcopy(g_sound_novel_saved_flags, g_save_work_buffer->sound_novel_saved_flags,
        sizeof(g_save_work_buffer->sound_novel_saved_flags));
    bcopy(g_brave_story_character_ages, g_save_work_buffer->brave_story_character_ages,
        sizeof(g_save_work_buffer->brave_story_character_ages));
    g_save_work_buffer->proposition_count = g_proposition_count;
    bcopy(
        g_proposition_active, g_save_work_buffer->active_propositions, sizeof(g_save_work_buffer->active_propositions));

    bits_init_primary_reader(g_save_work_buffer->parity_bits);
    block = (u8*)g_save_work_buffer;
    for (i = 0; i >= 0 && i < 0x3c; i++) {
        parity = 0;
        bits_init_secondary_reader(block);
        for (j = 0; j < 0x400; j++) {
            parity += bits_read_secondary(1);
        }
        block += 0x80;
        bits_write_primary(parity & 1);
    }
}

/*
 * Exact inverse of save_build_image: restore the SCUS globals from the
 * 0x1e00-byte save image in g_save_work_buffer.
 *
 * Returns 0 when the image is unusable -- the slot byte still reads 0xff
 * (never written), the format version is not 4, or a 128-byte block's parity
 * disagrees with the bit stored for it. Blocks 0..2 hold the header and
 * descriptor and are not checked (the primary cursor skips their three
 * bits): the save step stamps the slot number into the descriptor after the
 * parity was computed.
 *
 * A non-zero partial_load is the tutorial's partial load: it skips both the parity
 * sweep and the play-time restore, keeping the clock the caller already has.
 *
 * Constructs the match depends on:
 *   - `j`, the parity-sweep counter, doubles as the item-order "found" flag.
 *     A separate flag local swaps the $s1/$s2 roles of the flag and `i`.
 *   - `i = 0;` written above `j = 0;` before the suzuki_sound_set_type call,
 *     with the loop spelled `for (; i < 11; i++)`. That run is one basic
 *     block, so the list scheduler orders the two zeroing moves by source
 *     order, and the flag is live across the call in a callee-saved register.
 */
s32 save_restore_image(s32 partial_load) {
    s32 i;
    s32 j;
    u8 parity;
    u8* block;

    if (g_save_work_buffer->summary.slot == 0xff) {
        return 0;
    }
    if (g_save_work_buffer->summary.format_version != 4) {
        return 0;
    }

    if (partial_load == 0) {
        bits_init_primary_reader(g_save_work_buffer->parity_bits);
        /* The sweep reads the image as raw 128-byte blocks; block 3 starts mid-field. */
        block = (u8*)g_save_work_buffer + 3 * 0x80;
        bits_read_primary(3);
        for (i = 3; i >= 0 && i < 0x3c; i++) {
            parity = 0;
            bits_init_secondary_reader(block);
            for (j = 0; j < 0x400; j++) {
                parity += bits_read_secondary(1);
            }
            block += 0x80;
            parity &= 1;
            if (parity != (u8)bits_read_primary(1)) {
                return 0;
            }
        }
    }

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        bcopy(g_save_work_buffer->party_records[i], &g_party_units[i], sizeof(g_save_work_buffer->party_records[i]));
    }

    bcopy(g_save_work_buffer->item_quantities, g_item_quantities, sizeof(g_save_work_buffer->item_quantities));
    bcopy(g_save_work_buffer->poached_item_quantities, g_item_poached_quantities,
        sizeof(g_save_work_buffer->poached_item_quantities));
    bcopy(g_save_work_buffer->item_location_flags, g_item_location_flags,
        sizeof(g_save_work_buffer->item_location_flags));
    g_save_unread_byte = g_save_work_buffer->_unknown_1c88;
    bcopy(
        g_save_work_buffer->script_variables, g_script_variable_storage, sizeof(g_save_work_buffer->script_variables));
    bcopy(&g_save_work_buffer->options, &g_game_options, sizeof(g_save_work_buffer->options));
    i = 0;
    j = 0;
    suzuki_sound_set_type(g_game_options.fields.sound_mode);

    for (; i < 11; i++) {
        if (g_save_work_buffer->item_type_order_0[i] == 11) {
            j = 1;
        }
    }
    if (!j) {
        bcopy(g_save_work_buffer->item_type_order_0, g_item_type_order_tables.order_0,
            sizeof(g_save_work_buffer->item_type_order_0));
    }

    j = 0;
    g_item_type_order_tables.order_0[11] = -1;
    for (i = 0; i < 7; i++) {
        if (g_save_work_buffer->item_type_order_1[i] == 11) {
            j = 1;
        }
    }
    if (!j) {
        bcopy(
            g_save_work_buffer->item_type_order_1, g_item_type_order_1, sizeof(g_save_work_buffer->item_type_order_1));
    }

    g_item_type_order_1[7] = -1;
    bcopy(g_save_work_buffer->item_type_order_2, g_item_type_order_2, sizeof(g_save_work_buffer->item_type_order_2));
    bcopy(g_save_work_buffer->item_type_order_3, g_item_type_order_3, sizeof(g_save_work_buffer->item_type_order_3));
    bcopy(g_save_work_buffer->item_type_order_4, g_item_type_order_4, sizeof(g_save_work_buffer->item_type_order_4));
    bcopy(g_save_work_buffer->item_type_order_5, g_item_type_order_5, sizeof(g_save_work_buffer->item_type_order_5));
    bcopy(g_save_work_buffer->item_type_order_6, g_item_type_order_6, sizeof(g_save_work_buffer->item_type_order_6));
    bcopy(
        g_save_work_buffer->weapon_page_order, g_item_weapon_page_order, sizeof(g_save_work_buffer->weapon_page_order));
    bcopy(
        g_save_work_buffer->helmet_page_order, g_item_helmet_page_order, sizeof(g_save_work_buffer->helmet_page_order));
    bcopy(g_save_work_buffer->armor_page_order, g_item_armor_page_order, sizeof(g_save_work_buffer->armor_page_order));
    bcopy(g_save_work_buffer->accessory_page_order, g_item_accessory_page_order,
        sizeof(g_save_work_buffer->accessory_page_order));
    bcopy(g_save_work_buffer->item_page_order, g_item_page_order, sizeof(g_save_work_buffer->item_page_order));
    bcopy(g_save_work_buffer->treasure_acquisition_date_bits, g_brave_story_treasure_acquisition_date_bits,
        sizeof(g_save_work_buffer->treasure_acquisition_date_bits));
    bcopy(g_save_work_buffer->land_discovery_date_bits, g_brave_story_land_discovery_date_bits,
        sizeof(g_save_work_buffer->land_discovery_date_bits));
    bcopy(g_save_work_buffer->proposition_last_attempt_date_bits, g_proposition_last_attempt_date_bits,
        sizeof(g_save_work_buffer->proposition_last_attempt_date_bits));
    bcopy(g_save_work_buffer->proposition_states, g_proposition_states, sizeof(g_save_work_buffer->proposition_states));
    bcopy(g_save_work_buffer->sound_novel_saved_records, g_sound_novel_saved_readings,
        sizeof(g_save_work_buffer->sound_novel_saved_records));
    bcopy(g_save_work_buffer->sound_novel_read_section_bits, g_sound_novel_read_section_bits,
        sizeof(g_save_work_buffer->sound_novel_read_section_bits));
    bcopy(g_save_work_buffer->sound_novel_saved_flags, g_sound_novel_saved_flags,
        sizeof(g_save_work_buffer->sound_novel_saved_flags));
    bcopy(g_save_work_buffer->brave_story_character_ages, g_brave_story_character_ages,
        sizeof(g_save_work_buffer->brave_story_character_ages));
    g_proposition_count = g_save_work_buffer->proposition_count;
    bcopy(
        g_save_work_buffer->active_propositions, g_proposition_active, sizeof(g_save_work_buffer->active_propositions));

    if (partial_load == 0) {
        i = g_save_work_buffer->elapsed_seconds;
        g_save_play_time_hours = i / 3600;
        i %= 3600;
        g_save_play_time_minutes = i / 60;
        i %= 60;
        g_save_play_time_seconds = i;
    }
    return 1;
}

/* Resets the memory card menu state and allocates the 0x2000-byte load
 * buffer; also used by "Open tutorial files". */
void save_init_screen(void) {
    save_image_t* buffer;

    g_save_list_scroll_y = 0x18;
    g_save_list_scroll_velocity = 0;
    g_save_list_first_visible_row = 0;
    g_save_active_slot = 0;
    g_save_slot_cursor_row = 0;
    g_save_menu_result = 0;
    g_save_slot_select_timer = 0;
    buffer = heap_alloc(0x2000);
    g_save_work_buffer = buffer;
    memset(buffer, 0xFF, 0x1E80);
}

/* Free the memory card menu's load buffer from save_init_screen. */
static void save_free_work_buffer(void) {
    heap_free(g_save_work_buffer);
}

/* WORLD twin of save_update_slot_playtime: splits the loaded save's
 * play time into hours/minutes/seconds for the memory card menu. */
static void save_update_slot_playtime(s32 slot) {
    s32 component;
    s32 elapsed_seconds;

    if (g_save_slot_file_states[slot] != SAVE_SLOT_OCCUPIED) {
        g_save_slot_playtimes[slot].hours = 0;
        g_save_slot_playtimes[slot].minutes = 0;
        g_save_slot_playtimes[slot].seconds = 0;
    } else {
        elapsed_seconds = g_save_work_buffer->elapsed_seconds;

        component = elapsed_seconds / 3600;
        if (component >= 100) {
            component = 99;
        }
        g_save_slot_playtimes[slot].hours = component;

        elapsed_seconds %= 3600;
        component = elapsed_seconds / 60;
        g_save_slot_playtimes[slot].minutes = component;

        elapsed_seconds %= 60;
        g_save_slot_playtimes[slot].seconds = elapsed_seconds;
    }
}

/*
 * Scans the selected memory card and classifies each of the 15 save slots.
 *
 * WORLD twin of CARD's save_update_scan: same state
 * machine, but a NEW_CARD probe result is retried once before use.
 */
static void save_scan_slots(void) {
    s32 result;
    s32 status;
    s32 descriptor;
    s32 entry;
    RECT rect;

    if (g_save_error_message_closing != 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MENU) == 0) {
            g_save_slot_scan_active = 0;
            g_save_menu_step = SAVE_STEP_SELECT_CARD;
            g_save_error_message_closing = 0;
        }
        return;
    }

    if (g_save_format_delay_counter != 0) {
        g_save_format_delay_counter++;
        if (g_save_format_delay_counter == 3) {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_save_slot_scan_active = 0;
            g_save_format_delay_counter = 0;
            memcard_format_selected_slot();
            g_save_menu_step = SAVE_STEP_SELECT_CARD;
        }
        return;
    }

    if (g_save_slot_scan_active == 0) {
        menu_input_clear_state();
        /* While a slot is recorded as last used, the stored slot metadata is current: if the card
         * reports no change, rebuild the descriptions from it and skip the directory scan. */
        if (g_save_last_used_slot < 0x10) {
            if (memcard_wait_operation_result(5) == MEMCARD_RESULT_COMPLETE) {
                g_save_slot_scan_index = 0;
                while (g_save_slot_scan_index < CARD_SAVE_SLOT_COUNT) {
                    if (g_save_slot_file_states[g_save_slot_scan_index] >= 0) {
                        bcopy(&g_save_slot_metadata[g_save_slot_scan_index], &g_save_work_buffer->summary,
                            sizeof(save_summary_t));
                        save_build_slot_description(g_save_slot_file_states[g_save_slot_scan_index],
                            g_save_slot_descriptions[g_save_slot_scan_index]);
                        save_draw_slot_text(g_save_slot_scan_index);
                    }
                    g_save_slot_scan_index++;
                }
                g_save_menu_step = SAVE_STEP_SELECT_MODE;
                return;
            } else {
                g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
            }
        }

        /* Setup spans three calls: clear the VRAM column at x 0x180 (it holds slots 11 to 14's text),
         * start the checking message, then one idle call. */
        if (g_save_scan_start_frame < 3) {
            if (g_save_scan_start_frame == 0) {
                rect.x = 0x180;
                rect.y = 0;
                rect.w = 0x40;
                rect.h = 0xf0;
                ClearImage(&rect, 0, 0, 0);
            }
            if (g_save_scan_start_frame == 1) {
                menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, &g_save_checking_message);
            }
            g_save_scan_start_frame++;
            return;
        }

        /* Unlike CARD's twin, a new-card result is probed once more before use. */
        g_save_probe_result = memcard_open_or_create_selected_card();
        if (g_save_probe_result == MEMCARD_RESULT_NEW_CARD) {
            g_save_probe_result = memcard_open_or_create_selected_card();
        }
        if (g_save_probe_result == MEMCARD_RESULT_COMPLETE) {
            g_save_file_count = memcard_file_enumerate_saves(g_save_directory_buffer, 1);
            result = 0;
            /* Re-read the directory until two reads agree, at most 15 times, two vsyncs apart. */
            while (result < 15) {
                status = memcard_file_enumerate_saves(g_save_directory_buffer, 1);
                if (status == g_save_file_count) {
                    break;
                } else {
                    g_save_file_count = status;
                }
                VSync(2);
                result++;
            }
            g_save_free_block_count = memcard_file_count_free_blocks(g_save_directory_buffer, g_save_file_count);
        } else {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_sound_queued_effect_id = SOUND_SFX_ERROR_MESSAGE;
        }

        /* Every slot starts unscanned; the scan below classifies one slot per call. */
        for (result = 0; result < 15; result++) {
            g_save_slot_file_states[result] = SAVE_SLOT_UNSCANNED;
        }
        g_screen_selection_results[5] = -1;
        g_save_format_prompt[1].selected_index = 1;
        g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
        g_save_scan_prompt_active = 1;
        g_save_slot_scan_index = 0;
        g_save_slot_scan_active = 1;
        g_save_scan_start_frame = 0;
        return;
    }

    if (g_save_probe_result == MEMCARD_RESULT_NEW_CARD) {
        if (memcard_poll_operation_result() > MEMCARD_RESULT_COMPLETE) {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU_CHILD);
            g_save_slot_scan_active = 0;
        } else if (g_save_scan_prompt_active == 0) {
            if (g_screen_selection_results[5] == 0) {
                g_save_format_delay_counter = 1;
            } else {
                g_save_menu_step = SAVE_STEP_SELECT_CARD;
            }
            g_save_slot_scan_active = 0;
        } else {
            g_save_scan_prompt_active = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_save_format_prompt);
            if (g_save_scan_prompt_active == 0 && g_screen_selection_results[5] == 0) {
                menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, &g_save_formatting_message);
            }
        }
        return;
    }

    if (g_save_probe_result == MEMCARD_RESULT_ERROR || g_save_probe_result == MEMCARD_RESULT_TIMEOUT) {
        result = memcard_poll_operation_result();
        menu_start_or_poll_thread(THREAD_SCREEN_SLOT_MENU, &g_save_access_error_message);
        if ((g_menu_input_primary_repeat & PSX_PAD_CROSS) != 0 || (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) != 0) {
            menu_stop_managed_thread(THREAD_SCREEN_SLOT_MENU);
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
            g_save_error_message_closing = 1;
        } else if (result == MEMCARD_RESULT_NEW_CARD || result == MEMCARD_RESULT_COMPLETE) {
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_save_slot_scan_active = 0;
        }
        return;
    }

    if (g_save_probe_result == MEMCARD_RESULT_COMPLETE) {
        menu_input_clear_state();
        if (g_save_slot_scan_index < CARD_SAVE_SLOT_COUNT) {
            status = 1;
            entry = 0;
            while (entry < g_save_file_count) {
                status = strcmp(g_save_file_names[g_save_slot_scan_index], g_save_directory_buffer[entry].name);
                if (status == 0) {
                    break;
                }
                entry++;
            }
            /* -1: the card holds no file for this slot; 0: found. */
            status = status ? -1 : 0;

            if (status != -1) {
                memset(g_save_work_buffer, 0xff, 0x1e80);
                descriptor = memcard_file_open_selected_with_retries(g_save_file_names[g_save_slot_scan_index], 1);
                if (descriptor == -2) {
                    g_save_slot_scan_active = 0;
                    menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
                    return;
                }
                if (descriptor != -1) {
                    status = memcard_file_seek_with_retries(descriptor, 0x100, 0);
                    if (status != -1) {
                        status = memcard_file_read_with_retries(descriptor, &g_save_work_buffer->summary, 0x80);
                    }
                    /* The slot number is written last (save_write_step), so a summary still holding
                     * 0xff marks an unfinished save: delete it and count its block free again. */
                    if (status != -1 && g_save_work_buffer->summary.slot >= 0x10
                        && memcard_file_delete_selected_with_retries(g_save_file_names[g_save_slot_scan_index]) != 0) {
                        g_save_free_block_count++;
                        status = 0;
                    }
                    if (memcard_file_close_with_retries(descriptor) == 0) {
                        status = -1;
                    }
                    if (status == -1) {
                        g_save_slot_scan_active = 0;
                        menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
                        return;
                    }
                } else if (descriptor == -1) {
                    status = 0;
                }
            }

            /* A full 0x80-byte summary read means the slot holds a save. */
            if (status == 0x80) {
                g_save_slot_file_states[g_save_slot_scan_index] = SAVE_SLOT_OCCUPIED;
            }
            if (status <= 0) {
                if (g_save_free_block_count > 0) {
                    g_save_slot_file_states[g_save_slot_scan_index] = SAVE_SLOT_AVAILABLE;
                    g_save_free_block_count--;
                } else {
                    g_save_slot_file_states[g_save_slot_scan_index] = SAVE_SLOT_UNAVAILABLE;
                }
            }
            if (g_save_slot_file_states[g_save_slot_scan_index] >= 0) {
                save_build_slot_description(
                    g_save_slot_file_states[g_save_slot_scan_index], g_save_slot_descriptions[g_save_slot_scan_index]);
                bcopy(&g_save_work_buffer->summary, &g_save_slot_metadata[g_save_slot_scan_index],
                    sizeof(save_summary_t));
                save_update_slot_playtime(g_save_slot_scan_index);
                save_draw_slot_text(g_save_slot_scan_index);
            }
            g_save_slot_scan_index++;
        } else {
            /* A last-used slot below 0x10 lets the next scan reuse these results. */
            g_save_last_used_slot = 0;
            menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
            g_save_menu_step = SAVE_STEP_SELECT_MODE;
            g_save_slot_scan_active = 0;
        }
    }
}

/* Build the encoded save-slot description text for WORLD's save screen.
 *
 * mode 1 copies the empty-slot caption; mode 0 formats leader name, job and
 * level on the first line, and date and location on the second, out of the
 * loaded save summary, placing columns with set-pen-x codes. Other nonnegative
 * modes write only the terminator; negative modes leave the buffer untouched.
 *
 * Only the negative-mode exit explicitly returns a value (0). Other paths fall
 * through after writing the terminator, and no caller reads the result.
 * An explicit return at the tail introduces an extra constant load.
 *
 * `screen_text_bind_section` has no return statement; the job-name table
 * passed on below is the address its store left in $v0. */
static s32 save_build_slot_description(s32 mode, u8* destination) {
    s32 index;
    u8* source;

    if (mode == 1) {
        source = menu_text_find_entry_by_index(g_save_slot_text_table, 14, 1);
        while ((*destination++ = *source++) != 0xFE) { }
    } else if (mode < 0) {
        return 0;
    } else if (mode == 0) {
        index = 0;
        while (g_save_work_buffer->summary.name[index] != 0xFE) {
            *destination++ = g_save_work_buffer->summary.name[index];
            index++;
        }
        *destination++ = TEXT_SET_X_POSITION;
        *destination++ = 0x58;
        source = menu_text_find_entry_by_index(
            screen_text_bind_section(SCREEN_TEXT_DATA_JOB_NAMES), g_save_work_buffer->summary.job_id, 1);
        while (*source != 0xFE) {
            *destination++ = *source++;
        }
        *destination++ = TEXT_SET_X_POSITION;
        *destination++ = 0xAC;
        source = menu_text_find_entry_by_index(g_save_slot_text_table, 13, 1);
        while (*source != 0xFE) {
            *destination++ = *source++;
        }
        /* Digits 0-9 are glyph codes 0-9. */
        *destination++ = g_save_work_buffer->summary.level / 10;
        *destination++ = g_save_work_buffer->summary.level % 10;
        *destination++ = 0xFE;
        source = menu_text_find_entry_by_index(g_save_slot_text_table, g_save_work_buffer->summary.date.month, 1);
        while (*source != 0xFE) {
            *destination++ = *source++;
        }
        *destination++ = TEXT_SPACE;
        *destination++ = g_save_work_buffer->summary.date.day / 10;
        *destination++ = g_save_work_buffer->summary.date.day % 10;
        *destination++ = TEXT_SET_X_POSITION;
        *destination++ = 0x58;
        source = menu_text_find_entry_by_index(
            g_text_section_pointers[TEXT_SECTION_LOCATION_NAMES], g_save_work_buffer->summary.location, 1);
        while ((*destination++ = *source++) != 0xFE) { }
    }
    *destination = 0xFE;
}

/* Render a save slot's description into VRAM. The 15 slot images sit in three
 * VRAM columns of six, five and four slots (x 0x240, 0x1c0 and 0x180). Each
 * column sets the whole rectangle; the compiler merges the three tails. */
static void save_draw_slot_text(s32 save_slot_index) {
    RECT destination_rect;

    if (save_slot_index < 6) {
        destination_rect.x = 0x240;
        destination_rect.y = (save_slot_index << 5) + 0x130;
        destination_rect.w = 0x32;
        destination_rect.h = 0x20;
    } else if (save_slot_index < 11) {
        destination_rect.x = 0x1C0;
        destination_rect.y = (save_slot_index << 5) - 0x80;
        destination_rect.w = 0x32;
        destination_rect.h = 0x20;
    } else {
        destination_rect.x = 0x180;
        destination_rect.y = (save_slot_index << 5) - 0x100;
        destination_rect.w = 0x32;
        destination_rect.h = 0x20;
    }
    screen_text_render_rows(
        g_save_slot_descriptions[save_slot_index], g_save_slot_description_row_ids, &destination_rect, 0);
}

/* Count the memory-card save slots that qualify for the current menu mode
 * (occupied slots when g_save_mode is nonzero, every available slot otherwise),
 * select the last written slot, then run the slot list update until it settles for ten
 * consecutive passes. Returns the number of qualifying slots. */
static s32 save_count_selectable_slots(void) {
    s32 count;
    s32 i;

    g_screen_script_callbacks[0].value = 0;
    for (i = 0, count = 0; i < 15; i++) {
        if (i == g_save_last_used_slot) {
            g_save_active_slot = i;
            g_save_slot_cursor_row = count;
        }
        if (g_save_mode != 0) {
            if (g_save_slot_file_states[i] == SAVE_SLOT_OCCUPIED) {
                count++;
            }
        } else if (g_save_slot_file_states[i] != SAVE_SLOT_UNAVAILABLE) {
            count++;
        }
    }
    i = 0;
    while (i < 10) {
        save_scroll_slot_list(g_save_slot_cursor_row);
        if (g_save_list_scroll_velocity != 0) {
            i = 0;
        } else {
            i++;
        }
    }
    return count;
}

/*
 * Memory-card slot list: in menu step 3 handle slot selection input, then
 * draw the visible slot rows, the row cursor and the scroll arrows.
 *
 * WORLD twin of CARD's save_update_slot_list. The row sprites
 * and number commands are separate globals: the target keeps one address per
 * field and derives each call argument from it (reg - 2, reg - 8), which an
 * array of records would fold into a single base register.
 */
static void save_slot_list_step(s32 input) {
    s32 i;
    s32 row;
    s32 remaining;
    s32 busy;
    s16 source_v;
    u16 texture_page;
    s32 value;
    RECT rect;
    s16 label_y;

    if (g_save_menu_step == SAVE_STEP_SELECT_SAVE) {
        /* Input is ignored until every button has been released once, so a press carried over
         * from the previous step does not act here. */
        if (g_save_slot_list_input_ready == 0) {
            g_save_overwrite_prompt_running = 0;
            g_save_load_prompt_running = 0;
            if ((input & 0xffff) == 0) {
                g_save_slot_list_input_ready = 1;
            } else {
                input = 0;
            }
        }

        /* After an error or a new card, rescan unless the card still answers. */
        if (memcard_poll_operation_result() > MEMCARD_RESULT_COMPLETE) {
            if (memcard_wait_operation_result(3) != MEMCARD_RESULT_COMPLETE) {
                menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
                menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU_CHILD);
                g_save_menu_step = SAVE_STEP_SCANNING;
                g_save_last_used_slot = SAVE_LAST_USED_SLOT_NONE;
            }
        } else if (g_save_overwrite_prompt_running != 0) {
            g_save_overwrite_prompt_running
                = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_save_overwrite_confirm_prompt);
            if (g_save_overwrite_prompt_running == 0 && g_screen_selection_results[5] == 0) {
                g_save_menu_step = SAVE_STEP_SAVING;
            }
        } else if (g_save_load_prompt_running != 0) {
            g_save_load_prompt_running = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_save_load_confirm_prompt);
            if (g_save_load_prompt_running == 0 && g_screen_selection_results[5] == 0) {
                g_save_menu_step = SAVE_STEP_LOADING;
            }
        } else if ((input & PSX_PAD_CIRCLE) && g_save_list_scroll_velocity == 0) {
            sound_play_sfx(SOUND_SFX_CONFIRM);
            /* The cursor counts listed slots only; find the slot index it points at. */
            remaining = g_save_slot_cursor_row;
            if (g_save_mode == 0) {
                for (i = 0; i < 15; i++) {
                    if (g_save_slot_file_states[i] != SAVE_SLOT_UNAVAILABLE) {
                        remaining--;
                    }
                    if (remaining < 0) {
                        break;
                    }
                }
            } else {
                for (i = 0; i < 15; i++) {
                    if (g_save_slot_file_states[i] == SAVE_SLOT_OCCUPIED) {
                        remaining--;
                    }
                    if (remaining < 0) {
                        break;
                    }
                }
            }
            g_save_active_slot = i;

            /* The confirm prompt's window y depends on the cursor's visible row. */
            i = g_save_slot_cursor_row - g_save_list_first_visible_row;
            switch (i) {
            case 0:
                i = 0x5a;
                break;
            case 1:
                i = 0x88;
                break;
            case 2:
                i = 0x1e;
                break;
            case 3:
                i = 0x3a;
                break;
            }

            /* Saving asks only before overwriting; loading always asks. */
            if (g_save_mode == 0) {
                g_save_confirm_prompt = g_save_overwrite_confirm_prompt;
                g_save_overwrite_confirm_prompt[1].selected_index = 1;
            } else {
                g_save_confirm_prompt = g_save_load_confirm_prompt;
                g_save_load_confirm_prompt[1].selected_index = 0;
            }
            g_save_confirm_prompt->window.y = i;
            if (g_save_mode == 0) {
                if (g_save_slot_file_states[g_save_active_slot] == SAVE_SLOT_OCCUPIED) {
                    g_save_overwrite_prompt_running = 1;
                } else {
                    g_save_menu_step = SAVE_STEP_SAVING;
                }
            } else {
                g_save_load_prompt_running = 1;
            }
        } else if (input & PSX_PAD_CROSS) {
            if (g_save_list_scroll_velocity == 0) {
                g_sound_queued_effect_id = SOUND_SFX_CANCEL;
                g_save_menu_step = SAVE_STEP_SELECT_CARD;
            }
        } else if (g_save_list_scroll_velocity == 0) {
            i = g_save_slot_cursor_row;
            if (input & PSX_PAD_DOWN) {
                g_save_slot_cursor_row = i + 1;
                if (g_save_slot_cursor_row >= g_save_selectable_slot_count) {
                    g_save_slot_cursor_row = g_save_selectable_slot_count - 1;
                }
            } else if ((input & PSX_PAD_UP) && i != 0) {
                g_save_slot_cursor_row = i - 1;
            }
            if (i != g_save_slot_cursor_row) {
                g_sound_queued_effect_id = SOUND_SFX_CURSOR_MOVE;
            }
        }

        /* Leaving the selection step re-arms the release check. */
        if (g_save_menu_step != SAVE_STEP_SELECT_SAVE) {
            input = 0;
            g_save_slot_list_input_ready = 0;
        }
    }

    save_scroll_slot_list(g_save_slot_cursor_row);
    g_menu_window_rect_source = &g_save_slot_window;
    busy = g_save_menu_step != SAVE_STEP_SELECT_SAVE;
    for (i = 0, row = 0; i < CARD_SAVE_SLOT_COUNT; i++, row++) {
        /* Loading lists occupied slots only; saving skips the unavailable ones. */
        if (g_save_mode != 0) {
            while (g_save_slot_file_states[i] != SAVE_SLOT_OCCUPIED) {
                i++;
                if (i >= CARD_SAVE_SLOT_COUNT) {
                    break;
                }
            }
        } else {
            while (g_save_slot_file_states[i] == SAVE_SLOT_UNAVAILABLE) {
                i++;
                if (i >= CARD_SAVE_SLOT_COUNT) {
                    break;
                }
            }
        }
        if (i >= CARD_SAVE_SLOT_COUNT) {
            break;
        }

        g_save_slot_window.rect.y = g_save_list_scroll_y + g_save_list_scroll_velocity + row * 48 + 12;
        if (g_save_slot_window.rect.y < -48 || g_save_slot_window.rect.y >= 241) {
            continue;
        }

        gfx_set_otag_index(10);
        value = row != g_save_slot_cursor_row || busy != 0;
        screen_set_palette_mode(value);
        menu_opcode_00_draw_window_frame((menu_instruction_t*)&g_save_slot_window_command);

        label_y = g_save_slot_window.rect.y - 14;
        g_save_slot_quad_1.position.y = label_y;
        g_save_slot_quad_0.position.y = label_y;
        g_save_slot_quad_4.position.y = label_y;
        g_save_slot_quad_3.position.y = label_y;
        g_save_slot_quad_3.position.y = g_save_slot_quad_3.position.y + 1;
        g_save_slot_quad_4.position.y = g_save_slot_quad_4.position.y + 1;
        g_save_slot_quad_1.position.y++;
        if (g_save_slot_quad_0.position.y >= 0 && g_save_slot_quad_0.position.y < 241) {
            menu_gfx_enqueue_oriented_textured_quad(
                &g_save_slot_quad_0, screen_get_draw_color(), TEXTURE_UV_FLIP_NONE, 0, 11);
            menu_gfx_enqueue_oriented_textured_quad(
                &g_save_slot_quad_1, screen_get_draw_color(), TEXTURE_UV_FLIP_NONE, 0, 11);
            menu_gfx_enqueue_oriented_textured_quad(
                &g_save_slot_quad_3, screen_get_draw_color(), TEXTURE_UV_FLIP_NONE, 0, 11);
            menu_gfx_enqueue_oriented_textured_quad(
                &g_save_slot_quad_4, screen_get_draw_color(), TEXTURE_UV_FLIP_NONE, 0, 11);
            gfx_set_otag_index(11);
            g_save_slot_number_command.y = g_save_slot_quad_0.position.y;
            g_save_slot_number_command.value = i + 1;
            menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_save_slot_number_command);
        }

        g_save_slot_quad_2.position.y = g_save_slot_quad_0.position.y;
        if (g_save_slot_quad_2.position.y >= 0 && g_save_slot_quad_2.position.y < 241) {
            menu_gfx_enqueue_oriented_textured_quad(
                &g_save_slot_quad_2, screen_get_draw_color(), TEXTURE_UV_FLIP_NONE, 0, 11);
            g_save_playtime_seconds_command.y = g_save_playtime_minutes_command.y = g_save_playtime_hours_command.y
                = g_save_slot_quad_0.position.y;
            g_save_playtime_seconds_command.value = g_save_slot_playtimes[i].seconds;
            g_save_playtime_minutes_command.value = g_save_slot_playtimes[i].minutes;
            g_save_playtime_hours_command.value = g_save_slot_playtimes[i].hours;
            if (g_save_playtime_hours_command.value >= 100) {
                g_save_playtime_hours_command.value = 99;
            }
            menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_save_playtime_hours_command);
            menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_save_playtime_minutes_command);
            menu_opcode_0d_draw_formatted_number((menu_instruction_t*)&g_save_playtime_seconds_command);
        }

        if (g_save_slot_file_states[i] == SAVE_SLOT_AVAILABLE) {
            rect.x = 0x74;
            rect.y = g_save_slot_window.rect.y + 5;
            rect.w = 0x28;
            rect.h = 0x10;
        } else {
            rect.x = 0x1c;
            rect.y = g_save_slot_window.rect.y - 3;
            rect.w = 0xc8;
            rect.h = 0x1f;
        }

        /* Read the slot's text strip from where save_draw_slot_text put it. */
        if (i < 6) {
            texture_page = g_screen_window_tpage;
            source_v = i * 0x20 + 0x30;
        } else if (i < 11) {
            texture_page = GetTPage(0, 0, 0x1c0, 0);
            source_v = (i - 6) * 0x20 + 0x40;
        } else {
            texture_page = GetTPage(0, 0, 0x180, 0);
            source_v = (i - 11) * 0x20 + 0x60;
        }
        menu_gfx_enqueue_textured_quad(&rect, 0, source_v, 0, 0, texture_page, g_screen_window_clut, 11);
    }

    gfx_set_otag_index(15);
    /* The cursor stays on one of the four visible rows. */
    value = g_save_slot_cursor_row - g_save_list_first_visible_row;
    if (value < 0) {
        value = 0;
    }
    if (value >= 4) {
        value = 3;
    }
    g_save_slot_cursor_point.y = value * 48 + 0x28;
    screen_draw_sliding_cursor(&g_save_slot_cursor_point, &g_save_slot_cursor_anim, busy);

    /* Scroll arrows: up once the list has scrolled, down while rows remain below the visible four. */
    if (g_save_list_first_visible_row != 0) {
        rect.x = 0xe3;
        rect.y = 0x20;
        rect.w = 8;
        rect.h = 0x10;
        menu_gfx_enqueue_textured_quad(
            &rect, 0xd8, 0, 0, 0, g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, 15);
    }
    if (g_save_selectable_slot_count - g_save_list_first_visible_row >= 5) {
        rect.x = 0xe3;
        rect.y = 0xba;
        rect.w = 8;
        rect.h = 0x10;
        menu_gfx_enqueue_textured_quad(
            &rect, 0xe0, 0, 0, 0, g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, 15);
    }
}

/*
 * Keeps `row` inside the four visible rows of the save-slot list and steps
 * the list's scroll.
 *
 * At rest, a row above or below the window moves the first visible row by
 * one and starts a scroll. The signed 16-bit velocity then holds the pixels
 * covered so far, growing by 4 a call; after one row height (0x30) it folds
 * into g_save_list_scroll_y and returns to 0.
 */
static void save_scroll_slot_list(s32 row) {
    s16 velocity = g_save_list_scroll_velocity;
    s16 first;

    if (velocity == 0) {
        first = g_save_list_first_visible_row;
        if (row < first) {
            first -= 1;
            g_save_list_scroll_velocity = 4;
            g_save_list_first_visible_row = first;
        }
        first = g_save_list_first_visible_row;
        if (row >= first + 4) {
            first += 1;
            g_save_list_scroll_velocity = -4;
            g_save_list_first_visible_row = first;
        }
    } else if (velocity < 0) {
        g_save_list_scroll_velocity = velocity - 4;
        if (g_save_list_scroll_velocity < -0x2F) {
            g_save_list_scroll_velocity = 0;
            g_save_list_scroll_y -= 0x30;
        }
    } else if (velocity > 0) {
        g_save_list_scroll_velocity = velocity + 4;
        if (g_save_list_scroll_velocity >= 0x30) {
            g_save_list_scroll_velocity = 0;
            g_save_list_scroll_y += 0x30;
        }
    }
}

/* WORLD twin of save_build_file_header (built at -O2 here): fills the
 * memory-card header with the title template, the one-based save-file number
 * and the play time as full-width Shift-JIS digits, then the slot's icon.
 *
 * Callers pass the save slot (save_write_step; -1 for the tutorial
 * image), which also selects one of the 15 icon records; level is unused. */
static void save_build_file_header(s32 slot, s32 level, save_file_header_t* header) {
    s32 value;
    s32 digit;
    s32 divisor;
    s32 pos;
    save_icon_t* records;
    save_icon_t* record;

    header->magic0 = SAVE_HEADER_MAGIC_S;
    header->magic1 = SAVE_HEADER_MAGIC_C;
    header->icon_flags = SAVE_HEADER_ICON_ONE_FRAME;
    header->block_count = SAVE_HEADER_BLOCK_COUNT_ONE;
    memset(header->reserved, 0, 0x1C);
    slot++;
    bcopy(g_save_title_template, header->title, 0x16);

    /* Full-width digit d is Shift-JIS 0x82, 0x4F + d; 0x81 0x46 is a full-width colon. */
    value = slot / 10;
    header->title[0x10] = 0x82;
    header->title[0x11] = value + 0x4F;
    value = slot % 10;
    header->title[0x12] = 0x82;
    header->title[0x13] = value + 0x4F;

    pos = 0x16;

    value = g_save_play_time_hours;
    if (value >= 100) {
        value = 99;
    }
    divisor = 10;
    while (divisor > 0) {
        digit = value / divisor;
        value = value % divisor;
        header->title[pos++] = 0x82;
        header->title[pos++] = digit + 0x4F;
        divisor = divisor / 10;
    }

    header->title[pos++] = 0x81;
    header->title[pos++] = 0x46;

    value = g_save_play_time_minutes;
    if (value >= 60) {
        value = 59;
    }
    divisor = 10;
    while (divisor > 0) {
        digit = value / divisor;
        value = value % divisor;
        header->title[pos++] = 0x82;
        header->title[pos++] = digit + 0x4F;
        divisor = divisor / 10;
    }

    header->title[pos++] = 0x81;
    header->title[pos++] = 0x46;

    value = g_save_play_time_seconds;
    if (value >= 60) {
        value = 59;
    }
    divisor = 10;
    while (divisor > 0) {
        digit = value / divisor;
        value = value % divisor;
        header->title[pos++] = 0x82;
        header->title[pos++] = digit + 0x4F;
        divisor = divisor / 10;
    }

    header->title[pos++] = 0;
    header->title[pos++] = 0;

    records = g_save_icon_records;
    value = (slot - 1) % 15;
    record = records + value;
    bcopy(record->clut, header->icon_clut, 0x20);
    bcopy(record->image, header->icon_image, 0x80);
}
