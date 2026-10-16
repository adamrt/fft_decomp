/* Tutorial buttons, tutorial start and the tutorial command handlers with highlight rings and quads. */

#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/menu.h"
#include "fft/menu_text.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "fft/world_card.h"
#include "fft/world_formation.h"
#include "fft/world_gfx.h"
#include "fft/world_script.h"
#include "fft/world_text.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

static void world_script_advance_tutorial_highlight_brightness(void);
static void world_script_draw_tutorial_highlight_quads(s32 brightness);
static void world_script_handle_tutorial_command_change_dialog(void);
static void world_script_handle_tutorial_command_display_message(void);
static s32 world_script_handle_tutorial_command_end(void);
static void world_script_handle_tutorial_command_highlight_ring(void);
static void world_script_handle_tutorial_command_shift(void);
static void world_script_handle_tutorial_command_wait(void);
static void world_script_handle_tutorial_command_wait_for_button(void);
static void world_script_handle_tutorial_command_wait_for_message(void);
static void world_script_handle_tutorial_command_wait_time(void);

/* The pad state world_script_handle_tutorial_command read this frame; the tutorial text
 * thread takes its button input from it. */
s32 world_input_get_tutorial_buttons(void) {
    return g_world_script_tutorial_buttons;
}

/*
 * Start tutorial `index` (0 only clears the tutorial id).
 *
 * Loads the tutorial's files and binds its command stream and messages, then
 * swaps the game state for the tutorial's: the current game is serialized
 * into g_world_tutorial_state.saved_game, the 0x2000-byte tutorial save image
 * at sector 0x11A7F is loaded in its place (keeping the player's job-change
 * equip and sound options, with unequippable items shown) and unpacked into
 * the globals.
 */
void world_script_start_tutorial(s32 index) {
    g_world_script_tutorial_id = index;
    if (index != 0) {
        world_script_load_tutorial_files(index, &g_world_tutorial_state);
        g_world_script_tutorial_command_ptr = g_world_tutorial_state.script;
        g_text_section_pointers[WORLD_TEXT_SECTION_TUTORIAL_MESSAGES] = g_world_tutorial_state.messages;
        world_card_init_menu_state();
        world_card_build_save_image(-1);
        bcopy(g_world_load_work_buffer, &g_world_tutorial_state.saved_game, sizeof(g_world_tutorial_state.saved_game));
        main_file_load_checked_to_address(DISC_SECTOR_MENU_FFTSAVE_DAT, 0x2000, g_world_load_work_buffer);
        g_world_load_work_buffer->options.show_unequippable_items = GAME_OPTION_ON;
        g_world_load_work_buffer->options.max_equip_at_job_change = g_main_game_options.fields.max_equip_at_job_change;
        g_world_load_work_buffer->options.sound_mode = g_main_game_options.fields.sound_mode;
        world_card_load_globals_from_save_image(1);
        g_world_script_tutorial_command_active = 0;
        world_text_generate_formation_unit_name_string();
    }
}

/*
 * Per-frame step of the tutorial script. Returns 1 while no tutorial runs or
 * the current one continues, and the END command's result when it ends.
 *
 * Reads pad 0 as the tutorial's own button input (the soft-reset combination
 * resets the game), advances the highlight brightness while a wait is timed,
 * and dispatches the command byte at g_world_script_tutorial_command_ptr:
 * values below WORLD_TUTORIAL_CMD_SHIFT_LIMIT are pad-input commands, the
 * rest display a message, draw the highlight ring, wait or end.
 */
s32 world_script_handle_tutorial_command(void) {
    s32 pad;
    u8 cmd;

    if (g_world_script_tutorial_id == 0) {
        return 1;
    }
    g_world_script_tutorial_button_input = PadRead(0);
    if (g_world_script_tutorial_button_input == MAIN_INPUT_RESET_BUTTONS) {
        world_game_reset();
    }
    g_world_script_tutorial_buttons = g_world_script_tutorial_button_input;
    if (g_world_script_tutorial_wait_timer != 0) {
        world_script_advance_tutorial_highlight_brightness();
    }
    cmd = *g_world_script_tutorial_command_ptr;
    if (cmd < WORLD_TUTORIAL_CMD_SHIFT_LIMIT) {
        world_script_handle_tutorial_command_shift();
    } else if (cmd == WORLD_TUTORIAL_CMD_DISPLAY_MESSAGE) {
        world_script_handle_tutorial_command_display_message();
    } else if (cmd == WORLD_TUTORIAL_CMD_DRAW_CIRCLE) {
        world_script_handle_tutorial_command_highlight_ring();
    } else if (cmd == WORLD_TUTORIAL_CMD_WAIT) {
        world_script_handle_tutorial_command_wait();
    } else if (cmd == WORLD_TUTORIAL_CMD_WAIT_TIME) {
        world_script_handle_tutorial_command_wait_time();
    } else if (cmd == WORLD_TUTORIAL_CMD_WAIT_FOR_BUTTON) {
        world_script_handle_tutorial_command_wait_for_button();
    } else if (cmd == WORLD_TUTORIAL_CMD_END) {
        return world_script_handle_tutorial_command_end();
    } else if (cmd == WORLD_TUTORIAL_CMD_WAIT_FOR_MESSAGE) {
        world_script_handle_tutorial_command_wait_for_message();
    } else if (cmd == WORLD_TUTORIAL_CMD_CHANGE_DIALOG) {
        world_script_handle_tutorial_command_change_dialog();
    }
    return 1;
}

/* Tutorial "end" command: fade the tutorial overlay out, then release the
 * saved screen state and restore the pre-tutorial world buffers. */
static s32 world_script_handle_tutorial_command_end(void) {
    if (g_world_script_tutorial_command_active != 0) {
        if (world_gfx_update_fade_out_tile() == 0) {
            g_world_script_tutorial_id = 0;
            main_heap_call_free(g_world_tutorial_state.messages);
            main_heap_call_free(g_world_tutorial_state.script);
            bcopy(&g_world_tutorial_state.saved_game, g_world_load_work_buffer,
                sizeof(g_world_tutorial_state.saved_game));
            world_card_load_globals_from_save_image(1);
            world_text_generate_formation_unit_name_string();
            main_heap_call_free(g_world_load_work_buffer);
            world_formation_build_record_list(
                0, (world_unit_t**)&g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
            return 0;
        }
    } else {
        world_gfx_start_increasing_fade();
        g_world_script_tutorial_command_active = 1;
    }
}

/* Tutorial command below WORLD_TUTORIAL_CMD_SHIFT_LIMIT: press pad bit
 * number <command> for one frame, then wait 30 frames (counted by the VSync
 * mode) before moving to the next command. */
static void world_script_handle_tutorial_command_shift(void) {
    if (g_world_script_tutorial_command_active != 0) {
        g_world_script_tutorial_command_active -= menu_get_event_speed();
        g_world_script_tutorial_shift_command_bit = 0;
        if (g_world_script_tutorial_command_active <= 0) {
            g_world_script_tutorial_command_active = 0;
            ++g_world_script_tutorial_command_ptr;
        }
    } else {
        g_world_script_tutorial_command_active = 0x1E;
        g_world_script_tutorial_shift_command_bit = 1 << *g_world_script_tutorial_command_ptr;
    }
}

/* Tutorial command: start the text thread on a new message. Operands: two
 * message-id bytes (little-endian, biased by 0xB800) and a third byte stored
 * as the text thread's y offset.
 *
 * -O0, like the neighboring tutorial handlers. The message id is summed as
 * pointer arithmetic on purpose: written as plain integer addition, GCC
 * 2.6.3's fold re-associates `hi + (lo + 0xB800)` into `(hi + 0xB800) + lo`,
 * and the target adds the bias to the low byte first. */
/* Address-of operand fields preserves the -O0 byte-address steps. */
static void world_script_handle_tutorial_command_display_message(void) {
    thread_start(WORLD_THREAD_SLOT_TUTORIAL_TEXT, text_run_dialog_thread_variant);
    thread_set_parameters(WORLD_THREAD_SLOT_TUTORIAL_TEXT, 0x99,
        (s32)((u8*)(*(&((world_tutorial_message_instruction_t*)g_world_script_tutorial_command_ptr)->text_id_le[1])
                  * 0x100)
            + (*(&((world_tutorial_message_instruction_t*)g_world_script_tutorial_command_ptr)->text_id_le[0])
                + 0xB800)),
        0);
    /* Direct slot/member indexing splits this address at -O0. */
    *(s32*)((u8*)g_threads + (u32) & ((thread_t*)0)[WORLD_THREAD_SLOT_TUTORIAL_TEXT].task_words[3])
        = *(&((world_tutorial_message_instruction_t*)g_world_script_tutorial_command_ptr)->y_offset);
    g_world_script_tutorial_command_ptr += 4;
}

/* Tutorial command: let the tutorial text continue and move on once its
 * thread has ended. */
static void world_script_handle_tutorial_command_wait_for_message(void) {
    world_text_resume_printing(WORLD_THREAD_SLOT_TUTORIAL_TEXT);
    if (thread_is_running(WORLD_THREAD_SLOT_TUTORIAL_TEXT) == 0) {
        g_world_script_tutorial_command_ptr++;
    }
}

/* Tutorial command: replace the dialog text once the current text has
 * finished printing. Operands: two message-id bytes (little-endian, biased
 * by 0xB800) that are handed to the text thread as its second parameter.
 *
 * -O0, like the neighboring tutorial handlers. The message id is summed as
 * pointer arithmetic on purpose: written as plain integer addition, GCC
 * 2.6.3's fold re-associates `hi + (lo + 0xB800)` into `(hi + 0xB800) + lo`,
 * and the target adds the bias to the low byte first. */
/* Address-of operand fields preserves the -O0 byte-address steps. */
static void world_script_handle_tutorial_command_change_dialog(void) {
    if (world_text_is_printing(WORLD_THREAD_SLOT_TUTORIAL_TEXT) != 1) {
        thread_set_parameters(WORLD_THREAD_SLOT_TUTORIAL_TEXT, 0x99,
            (s32)((u8*)(*(&((world_tutorial_dialog_instruction_t*)g_world_script_tutorial_command_ptr)->text_id_le[1])
                      * 0x100)
                + (*(&((world_tutorial_dialog_instruction_t*)g_world_script_tutorial_command_ptr)->text_id_le[0])
                    + 0xB800)),
            0);
        g_world_script_tutorial_command_ptr += 3;
        /* Direct slot/member indexing splits this address at -O0. */
        *(s32*)((u8*)g_threads + (u32) & ((thread_t*)0)[WORLD_THREAD_SLOT_TUTORIAL_TEXT].task_id) = THREAD_TASK_RESUME;
    }
}

/* Tutorial command: lay out the highlight ring. The two corner arrays the
 * draw routine consumes are filled with 30 samples of a circle centered on
 * the command's (x, y) operand bytes, at three radii -- outer (r + 8),
 * middle (r + 4) and inner (r) -- each scaled independently in x and y by
 * the command's two scale bytes. Quad i of the first array spans outer to
 * middle, quad i of the second spans middle to inner, so the pair paints an
 * annulus. Entry 30 repeats entry 0 to close the ring. Operands are five
 * bytes after the opcode, so the command pointer advances by six.
 *
 * The step is 0x88 and the phase bias is 1024 (ONE / 4), i.e. a quarter
 * turn on the GTE's ONE-unit circle, so sample 0 starts at the top.
 */
/* Address-of operand fields preserves the -O0 byte-address steps. */
static void world_script_handle_tutorial_command_highlight_ring(void) {
    s32 radius;
    s32 outer_x;
    s32 outer_y;
    s32 middle_x;
    s32 middle_y;
    s32 inner_x;
    s32 inner_y;
    s32 scale_x;
    s32 scale_y;
    s32 unused;
    s32 i;
    s32 step = 0x88;

    radius = *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->radius);
    scale_x = *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->scale_x);
    scale_y = *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->scale_y);
    for (i = 0; i < 30; i++) {
        outer_x = (radius + 8) * scale_x;
        outer_y = (radius + 8) * scale_y;
        middle_x = (radius + 4) * scale_x;
        middle_y = (radius + 4) * scale_y;
        inner_x = radius * scale_x;
        inner_y = radius * scale_y;
        g_world_tutorial_highlight_corners_a[i][0].x = ((rcos(step * i + 1024) * outer_x) >> 19)
            + *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->x);
        g_world_tutorial_highlight_corners_a[i][0].y = ((rsin(step * i + 1024) * outer_y) >> 19)
            + *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->y);
        g_world_tutorial_highlight_corners_a[i][1].x = ((rcos(step * i + 1024) * middle_x) >> 19)
            + *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->x);
        g_world_tutorial_highlight_corners_a[i][1].y = ((rsin(step * i + 1024) * middle_y) >> 19)
            + *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->y);
        g_world_tutorial_highlight_corners_b[i][0].x = g_world_tutorial_highlight_corners_a[i][1].x;
        g_world_tutorial_highlight_corners_b[i][0].y = g_world_tutorial_highlight_corners_a[i][1].y;
        g_world_tutorial_highlight_corners_b[i][1].x = ((rcos(step * i + 1024) * inner_x) >> 19)
            + *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->x);
        g_world_tutorial_highlight_corners_b[i][1].y = ((rsin(step * i + 1024) * inner_y) >> 19)
            + *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->y);
    }

    /* Point copies emit 32-bit transfers; retail copies the closing sample as four 16-bit values. */
    for (i = 0; i < 4; i++) {
        (&g_world_tutorial_highlight_corners_a[30][0].x)[i] = (&g_world_tutorial_highlight_corners_a[0][0].x)[i];
        (&g_world_tutorial_highlight_corners_b[30][0].x)[i] = (&g_world_tutorial_highlight_corners_b[0][0].x)[i];
    }

    g_world_script_tutorial_wait_timer = 2;
    g_world_script_tutorial_command_ptr += 6;
    g_world_tutorial_highlight_quad_count = 0;
}

/* Extend the tutorial highlight while other commands run; WAIT counts its brightness down. */
static void world_script_advance_tutorial_highlight_brightness(void) {
    if (*g_world_script_tutorial_command_ptr != WORLD_TUTORIAL_CMD_WAIT) {
        g_world_script_tutorial_wait_timer = 0x12;
        g_world_tutorial_highlight_quad_count += menu_get_event_speed();
        if (g_world_tutorial_highlight_quad_count >= 0x1e) {
            g_world_tutorial_highlight_quad_count = 0x1e;
        }
    }
    world_script_draw_tutorial_highlight_quads(g_world_script_tutorial_wait_timer);
}

/* Tutorial command: count the wait timer down by the VSync mode and move to
 * the next command when it reaches zero. */
static void world_script_handle_tutorial_command_wait(void) {
    g_world_script_tutorial_wait_timer -= menu_get_event_speed();
    if (g_world_script_tutorial_wait_timer <= 0) {
        g_world_script_tutorial_wait_timer = 0;
        g_world_script_tutorial_command_ptr++;
    }
}

/*
 * Submit the tutorial highlight quads as gradient primitives: the first set
 * fades from a dim left edge to a bright right edge, the second the other
 * way round, with brightness scaled by ten. Built with -O0 like the other
 * tutorial handlers.
 */
static void world_script_draw_tutorial_highlight_quads(s32 brightness) {
    s32 i;

    brightness = brightness * 10;
    menu_gfx_enqueue_draw_mode_variant(1, 0, GetTPage(0, 1, 0, 0), 0, 0x3C);
    g_world_tutorial_highlight_colors_a[2].r = g_world_tutorial_highlight_colors_a[0].r = brightness >> 1;
    g_world_tutorial_highlight_colors_a[2].g = g_world_tutorial_highlight_colors_a[0].g = brightness >> 3;
    g_world_tutorial_highlight_colors_a[2].b = g_world_tutorial_highlight_colors_a[0].b = brightness >> 3;
    g_world_tutorial_highlight_colors_b[1].r = g_world_tutorial_highlight_colors_b[3].r = brightness >> 1;
    g_world_tutorial_highlight_colors_b[1].g = g_world_tutorial_highlight_colors_b[3].g = brightness >> 3;
    g_world_tutorial_highlight_colors_b[1].b = g_world_tutorial_highlight_colors_b[3].b = brightness >> 3;
    g_world_tutorial_highlight_colors_a[3].r = g_world_tutorial_highlight_colors_a[1].r = brightness;
    g_world_tutorial_highlight_colors_a[3].g = g_world_tutorial_highlight_colors_a[1].g = brightness >> 2;
    g_world_tutorial_highlight_colors_a[3].b = g_world_tutorial_highlight_colors_a[1].b = brightness >> 2;
    g_world_tutorial_highlight_colors_b[0].r = g_world_tutorial_highlight_colors_b[2].r = brightness;
    g_world_tutorial_highlight_colors_b[0].g = g_world_tutorial_highlight_colors_b[2].g = brightness >> 2;
    g_world_tutorial_highlight_colors_b[0].b = g_world_tutorial_highlight_colors_b[2].b = brightness >> 2;
    for (i = 0; i < g_world_tutorial_highlight_quad_count; i++) {
        gfx_enqueue_gouraud_quad(g_world_tutorial_highlight_corners_a[i], g_world_tutorial_highlight_colors_a, 1, 0x3D);
        gfx_enqueue_gouraud_quad(g_world_tutorial_highlight_corners_b[i], g_world_tutorial_highlight_colors_b, 1, 0x3D);
    }
}

/* Tutorial command: wait the number of frames in the command's operand byte
 * (counted by the VSync mode), then step past the two-byte command. */
/* Address-of operand fields preserves the -O0 byte-address steps. */
static void world_script_handle_tutorial_command_wait_time(void) {
    if (g_world_script_tutorial_command_active != 0) {
        g_world_script_tutorial_wait_time_remaining -= menu_get_event_speed();
        if (g_world_script_tutorial_wait_time_remaining < 0) {
            g_world_script_tutorial_command_active = 0;
            g_world_script_tutorial_command_ptr += 2;
        }
    } else {
        g_world_script_tutorial_command_active = 1;
        g_world_script_tutorial_wait_time_remaining
            = *(&((world_tutorial_wait_instruction_t*)g_world_script_tutorial_command_ptr)->frames);
    }
}

/* Tutorial command: advance on any button once the tutorial text thread has
 * ended or has stopped printing. */
static void world_script_handle_tutorial_command_wait_for_button(void) {
    s32 running = thread_is_running(WORLD_THREAD_SLOT_TUTORIAL_TEXT);

    if (running == 0) {
        if (g_world_script_tutorial_button_input != 0) {
            g_world_script_tutorial_command_ptr++;
        }
    } else if (world_text_is_printing(WORLD_THREAD_SLOT_TUTORIAL_TEXT) != 1) {
        if (g_world_script_tutorial_button_input != 0) {
            g_world_script_tutorial_command_ptr++;
        }
    }
}
