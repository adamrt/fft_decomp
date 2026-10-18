/* Tutorial buttons, tutorial start and the tutorial command handlers with highlight rings and quads. */

#include "fft/tutorial.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/message.h"
#include "fft/option.h"
#include "fft/save.h"
#include "fft/screen.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* tutorial playback state */
extern tutorial_state_t g_tutorial_state;
extern s16 g_tutorial_wait_time_remaining;
extern s16 g_tutorial_highlight_brightness;
extern s16 g_tutorial_buttons;

/* The tutorial command stream. Commands below 0x10 are one-byte shifts; the rest are the id (1 byte) followed by
 * their parameters: 0x10 message id (2) and y (1), 0x11 message id (2), 0x13 x, y, size, width mod and height mod
 * (1 each), 0x15 time (1). */
enum {
    TUTORIAL_CMD_PRESS_BUTTON_LIMIT = 0x10,
    TUTORIAL_CMD_DISPLAY_MESSAGE = 0x10,
    TUTORIAL_CMD_CHANGE_DIALOG = 0x11,
    TUTORIAL_CMD_WAIT_FOR_MESSAGE = 0x12,
    TUTORIAL_CMD_HIGHLIGHT_RING = 0x13,
    TUTORIAL_CMD_FADE_OUT_HIGHLIGHT = 0x14,
    TUTORIAL_CMD_WAIT_TIME = 0x15,
    TUTORIAL_CMD_WAIT_FOR_BUTTON = 0x16,
    TUTORIAL_CMD_END = 0x17
};

/* Serialized tutorial commands; opcode selects the operand record. */
typedef struct tutorial_message_instruction {
    u8 opcode;
    u8 text_id_le[2];
    u8 y_offset;
} tutorial_message_instruction_t;
typedef char tutorial_message_instruction_size_must_be_4[sizeof(tutorial_message_instruction_t) == 4 ? 1 : -1];

typedef struct tutorial_dialog_instruction {
    u8 opcode;
    u8 text_id_le[2];
} tutorial_dialog_instruction_t;
typedef char tutorial_dialog_instruction_size_must_be_3[sizeof(tutorial_dialog_instruction_t) == 3 ? 1 : -1];

typedef struct tutorial_circle_instruction {
    u8 opcode;
    u8 x;
    u8 y;
    u8 radius;
    u8 scale_x;
    u8 scale_y;
} tutorial_circle_instruction_t;
typedef char tutorial_circle_instruction_size_must_be_6[sizeof(tutorial_circle_instruction_t) == 6 ? 1 : -1];

typedef struct tutorial_wait_instruction {
    u8 opcode;
    u8 frames;
} tutorial_wait_instruction_t;
typedef char tutorial_wait_instruction_size_must_be_2[sizeof(tutorial_wait_instruction_t) == 2 ? 1 : -1];

static u8* g_tutorial_command_ptr = 0;
static s32 g_tutorial_command_active = 0;
static u32 g_tutorial_button_input = 0;
/* GPU coordinate halfwords; signed circle arithmetic is truncated when stored. */
static upoint16_t g_tutorial_highlight_corners_a[31][2] = { 0 }; /* outer/middle ring samples */
static upoint16_t g_tutorial_highlight_corners_b[31][2] = { 0 }; /* middle/inner ring samples */
static rgb24_t g_tutorial_highlight_colors_a[4] = { 0 };         /* first quad corner colors */
static rgb24_t g_tutorial_highlight_colors_b[4] = { 0 };         /* second quad corner colors */
static u8 g_tutorial_highlight_quad_count = 0;

static void tutorial_advance_highlight(void);
static void tutorial_draw_highlight(s32 brightness);
static void tutorial_opcode_00_press_button(void);
static void tutorial_opcode_10_display_message(void);
static void tutorial_opcode_11_change_dialog(void);
static void tutorial_opcode_12_wait_for_message(void);
static void tutorial_opcode_13_highlight_ring(void);
static void tutorial_opcode_14_fade_out_highlight(void);
static void tutorial_opcode_15_wait_time(void);
static void tutorial_opcode_16_wait_for_button(void);
static s32 tutorial_opcode_17_end(void);

/* The pad state tutorial_step read this frame; the tutorial text
 * thread takes its button input from it. */
s32 tutorial_get_buttons(void) {
    return g_tutorial_buttons;
}

/*
 * Start tutorial `index` (0 only clears the tutorial id).
 *
 * Loads the tutorial's files and binds its command stream and messages, then
 * swaps the game state for the tutorial's: the current game is serialized
 * into g_tutorial_state.saved_game, the 0x2000-byte tutorial save image
 * at sector 0x11A7F is loaded in its place (keeping the player's job-change
 * equip and sound options, with unequippable items shown) and unpacked into
 * the globals.
 */
void tutorial_start(s32 index) {
    g_tutorial_id = index;
    if (index != 0) {
        tutorial_load_files(index, &g_tutorial_state);
        g_tutorial_command_ptr = g_tutorial_state.script;
        g_text_section_pointers[SCREEN_TEXT_SECTION_TUTORIAL_MESSAGES] = g_tutorial_state.messages;
        save_init_screen();
        save_build_image(-1);
        bcopy(g_save_work_buffer, &g_tutorial_state.saved_game, sizeof(g_tutorial_state.saved_game));
        file_load_checked_to_address(DISC_SECTOR_MENU_FFTSAVE_DAT, 0x2000, g_save_work_buffer);
        g_save_work_buffer->options.show_unequippable_items = GAME_OPTION_ON;
        g_save_work_buffer->options.max_equip_at_job_change = g_game_options.fields.max_equip_at_job_change;
        g_save_work_buffer->options.sound_mode = g_game_options.fields.sound_mode;
        save_restore_image(1);
        g_tutorial_command_active = 0;
        formation_build_roster_names();
    }
}

/*
 * Per-frame step of the tutorial script. Returns 1 while no tutorial runs or
 * the current one continues, and the END command's result when it ends.
 *
 * Reads pad 0 as the tutorial's own button input (the soft-reset combination
 * resets the game), advances the highlight brightness while a wait is timed,
 * and dispatches the command byte at g_tutorial_command_ptr:
 * values below TUTORIAL_CMD_PRESS_BUTTON_LIMIT are pad-input commands, the
 * rest display a message, draw the highlight ring, wait or end.
 */
s32 tutorial_step(void) {
    s32 pad;
    u8 cmd;

    if (g_tutorial_id == 0) {
        return 1;
    }
    g_tutorial_button_input = PadRead(0);
    if (g_tutorial_button_input == INPUT_RESET_BUTTONS) {
        screen_reset_game();
    }
    g_tutorial_buttons = g_tutorial_button_input;
    if (g_tutorial_highlight_brightness != 0) {
        tutorial_advance_highlight();
    }
    cmd = *g_tutorial_command_ptr;
    if (cmd < TUTORIAL_CMD_PRESS_BUTTON_LIMIT) {
        tutorial_opcode_00_press_button();
    } else if (cmd == TUTORIAL_CMD_DISPLAY_MESSAGE) {
        tutorial_opcode_10_display_message();
    } else if (cmd == TUTORIAL_CMD_HIGHLIGHT_RING) {
        tutorial_opcode_13_highlight_ring();
    } else if (cmd == TUTORIAL_CMD_FADE_OUT_HIGHLIGHT) {
        tutorial_opcode_14_fade_out_highlight();
    } else if (cmd == TUTORIAL_CMD_WAIT_TIME) {
        tutorial_opcode_15_wait_time();
    } else if (cmd == TUTORIAL_CMD_WAIT_FOR_BUTTON) {
        tutorial_opcode_16_wait_for_button();
    } else if (cmd == TUTORIAL_CMD_END) {
        return tutorial_opcode_17_end();
    } else if (cmd == TUTORIAL_CMD_WAIT_FOR_MESSAGE) {
        tutorial_opcode_12_wait_for_message();
    } else if (cmd == TUTORIAL_CMD_CHANGE_DIALOG) {
        tutorial_opcode_11_change_dialog();
    }
    return 1;
}

/* Tutorial "end" command: fade the screen out, then free the tutorial's files,
 * restore the game saved by tutorial_start and rebuild the
 * formation records; returns 0 then.
 *
 * The paths that keep running return nothing (a retail quirk); at -O0 $v0
 * still holds 1 there, from the fade's running result or the stored flag, so
 * tutorial_step passes 1 on. */
static s32 tutorial_opcode_17_end(void) {
    if (g_tutorial_command_active != 0) {
        if (screen_fade_update_out() == 0) {
            g_tutorial_id = 0;
            heap_call_free(g_tutorial_state.messages);
            heap_call_free(g_tutorial_state.script);
            bcopy(&g_tutorial_state.saved_game, g_save_work_buffer, sizeof(g_tutorial_state.saved_game));
            save_restore_image(1);
            formation_build_roster_names();
            heap_call_free(g_save_work_buffer);
            formation_build_record_list(0, g_formation_unit_pointers, FORMATION_RECORD_LIST_ALL);
            return 0;
        }
    } else {
        screen_fade_start_increasing();
        g_tutorial_command_active = 1;
    }
}

/* Tutorial command below TUTORIAL_CMD_PRESS_BUTTON_LIMIT: press pad bit
 * number <command> for one frame, then wait 30 frames (counted by the VSync
 * mode) before moving to the next command. */
static void tutorial_opcode_00_press_button(void) {
    if (g_tutorial_command_active != 0) {
        g_tutorial_command_active -= menu_get_event_speed();
        g_tutorial_pressed_button = 0;
        if (g_tutorial_command_active <= 0) {
            g_tutorial_command_active = 0;
            ++g_tutorial_command_ptr;
        }
    } else {
        g_tutorial_command_active = 0x1E;
        g_tutorial_pressed_button = 1 << *g_tutorial_command_ptr;
    }
}

/* Tutorial command: start the text thread on a new message. Operands: two
 * message-id bytes (little-endian, biased by 0xB800) and a third byte stored
 * as the text thread's y offset.
 *
 * -O0, like the neighboring tutorial handlers. The message id is summed as
 * pointer arithmetic on purpose: written as plain integer addition, GCC
 * 2.6.3's fold re-associates `hi + (lo + 0xB800)` into `(hi + 0xB800) + lo`,
 * and the target adds the bias to the low byte first. Taking the address of
 * the operand fields preserves the -O0 byte-address steps. */
static void tutorial_opcode_10_display_message(void) {
    thread_start(SCREEN_THREAD_SLOT_TUTORIAL_TEXT, message_dialog_thread);
    thread_set_parameters(SCREEN_THREAD_SLOT_TUTORIAL_TEXT, 0x99,
        (s32)((u8*)(*(&((tutorial_message_instruction_t*)g_tutorial_command_ptr)->text_id_le[1]) * 0x100)
            + (*(&((tutorial_message_instruction_t*)g_tutorial_command_ptr)->text_id_le[0]) + 0xB800)),
        0);
    /* Direct slot/member indexing splits this address at -O0. */
    *(s32*)((u8*)g_threads
        + (u32)(&((thread_t*)0)[SCREEN_THREAD_SLOT_TUTORIAL_TEXT].task_words[DIALOG_THREAD_WORD_Y_OFFSET]))
        = *(&((tutorial_message_instruction_t*)g_tutorial_command_ptr)->y_offset);
    g_tutorial_command_ptr += 4;
}

/* Tutorial command: let the tutorial text continue and move on once its
 * thread has ended. */
static void tutorial_opcode_12_wait_for_message(void) {
    screen_text_resume_printing(SCREEN_THREAD_SLOT_TUTORIAL_TEXT);
    if (thread_is_running(SCREEN_THREAD_SLOT_TUTORIAL_TEXT) == 0) {
        g_tutorial_command_ptr++;
    }
}

/* Tutorial command: replace the dialog text once the current text has
 * finished printing. Operands: two message-id bytes (little-endian, biased
 * by 0xB800) that are handed to the text thread as its second parameter.
 *
 * -O0, like the neighboring tutorial handlers. The message id is summed as
 * pointer arithmetic on purpose: written as plain integer addition, GCC
 * 2.6.3's fold re-associates `hi + (lo + 0xB800)` into `(hi + 0xB800) + lo`,
 * and the target adds the bias to the low byte first. Taking the address of
 * the operand fields preserves the -O0 byte-address steps. */
static void tutorial_opcode_11_change_dialog(void) {
    if (screen_text_is_printing(SCREEN_THREAD_SLOT_TUTORIAL_TEXT) != 1) {
        thread_set_parameters(SCREEN_THREAD_SLOT_TUTORIAL_TEXT, 0x99,
            (s32)((u8*)(*(&((tutorial_dialog_instruction_t*)g_tutorial_command_ptr)->text_id_le[1]) * 0x100)
                + (*(&((tutorial_dialog_instruction_t*)g_tutorial_command_ptr)->text_id_le[0]) + 0xB800)),
            0);
        g_tutorial_command_ptr += 3;
        /* Direct slot/member indexing splits this address at -O0. */
        *(s32*)((u8*)g_threads + (u32) & ((thread_t*)0)[SCREEN_THREAD_SLOT_TUTORIAL_TEXT].task_id) = THREAD_TASK_RESUME;
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
 * turn on the GTE's ONE-unit circle, so sample 0 starts at the top. Taking
 * the address of the operand fields preserves the -O0 byte-address steps.
 */
static void tutorial_opcode_13_highlight_ring(void) {
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

    radius = *(&((tutorial_circle_instruction_t*)g_tutorial_command_ptr)->radius);
    scale_x = *(&((tutorial_circle_instruction_t*)g_tutorial_command_ptr)->scale_x);
    scale_y = *(&((tutorial_circle_instruction_t*)g_tutorial_command_ptr)->scale_y);
    for (i = 0; i < 30; i++) {
        outer_x = (radius + 8) * scale_x;
        outer_y = (radius + 8) * scale_y;
        middle_x = (radius + 4) * scale_x;
        middle_y = (radius + 4) * scale_y;
        inner_x = radius * scale_x;
        inner_y = radius * scale_y;
        g_tutorial_highlight_corners_a[i][0].x = ((rcos(step * i + 1024) * outer_x) >> 19)
            + *(&((tutorial_circle_instruction_t*)g_tutorial_command_ptr)->x);
        g_tutorial_highlight_corners_a[i][0].y = ((rsin(step * i + 1024) * outer_y) >> 19)
            + *(&((tutorial_circle_instruction_t*)g_tutorial_command_ptr)->y);
        g_tutorial_highlight_corners_a[i][1].x = ((rcos(step * i + 1024) * middle_x) >> 19)
            + *(&((tutorial_circle_instruction_t*)g_tutorial_command_ptr)->x);
        g_tutorial_highlight_corners_a[i][1].y = ((rsin(step * i + 1024) * middle_y) >> 19)
            + *(&((tutorial_circle_instruction_t*)g_tutorial_command_ptr)->y);
        g_tutorial_highlight_corners_b[i][0].x = g_tutorial_highlight_corners_a[i][1].x;
        g_tutorial_highlight_corners_b[i][0].y = g_tutorial_highlight_corners_a[i][1].y;
        g_tutorial_highlight_corners_b[i][1].x = ((rcos(step * i + 1024) * inner_x) >> 19)
            + *(&((tutorial_circle_instruction_t*)g_tutorial_command_ptr)->x);
        g_tutorial_highlight_corners_b[i][1].y = ((rsin(step * i + 1024) * inner_y) >> 19)
            + *(&((tutorial_circle_instruction_t*)g_tutorial_command_ptr)->y);
    }

    /* Point copies emit 32-bit transfers; retail copies the closing sample as four 16-bit values. */
    for (i = 0; i < 4; i++) {
        (&g_tutorial_highlight_corners_a[30][0].x)[i] = (&g_tutorial_highlight_corners_a[0][0].x)[i];
        (&g_tutorial_highlight_corners_b[30][0].x)[i] = (&g_tutorial_highlight_corners_b[0][0].x)[i];
    }

    g_tutorial_highlight_brightness = 2;
    g_tutorial_command_ptr += 6;
    g_tutorial_highlight_quad_count = 0;
}

/* Draw the tutorial highlight ring each frame while it is up. Until a WAIT
 * command runs, the ring grows by one quad per VSync up to all 30, at full
 * brightness (18); WAIT then counts the brightness down to fade it out. */
static void tutorial_advance_highlight(void) {
    if (*g_tutorial_command_ptr != TUTORIAL_CMD_FADE_OUT_HIGHLIGHT) {
        g_tutorial_highlight_brightness = 0x12;
        g_tutorial_highlight_quad_count += menu_get_event_speed();
        if (g_tutorial_highlight_quad_count >= 0x1e) {
            g_tutorial_highlight_quad_count = 0x1e;
        }
    }
    tutorial_draw_highlight(g_tutorial_highlight_brightness);
}

/* Tutorial command: fade the highlight out, counting its brightness down by the
 * VSync mode, and move to the next command when it reaches zero. */
static void tutorial_opcode_14_fade_out_highlight(void) {
    g_tutorial_highlight_brightness -= menu_get_event_speed();
    if (g_tutorial_highlight_brightness <= 0) {
        g_tutorial_highlight_brightness = 0;
        g_tutorial_command_ptr++;
    }
}

/*
 * Submit the tutorial highlight quads as gradient primitives: the first set
 * fades from a dim left edge to a bright right edge, the second the other
 * way round, with brightness scaled by ten. Built with -O0 like the other
 * tutorial handlers.
 */
static void tutorial_draw_highlight(s32 brightness) {
    s32 i;

    brightness = brightness * 10;
    menu_gfx_enqueue_draw_mode(1, 0, GetTPage(0, 1, 0, 0), 0, 0x3C);
    g_tutorial_highlight_colors_a[2].r = g_tutorial_highlight_colors_a[0].r = brightness >> 1;
    g_tutorial_highlight_colors_a[2].g = g_tutorial_highlight_colors_a[0].g = brightness >> 3;
    g_tutorial_highlight_colors_a[2].b = g_tutorial_highlight_colors_a[0].b = brightness >> 3;
    g_tutorial_highlight_colors_b[1].r = g_tutorial_highlight_colors_b[3].r = brightness >> 1;
    g_tutorial_highlight_colors_b[1].g = g_tutorial_highlight_colors_b[3].g = brightness >> 3;
    g_tutorial_highlight_colors_b[1].b = g_tutorial_highlight_colors_b[3].b = brightness >> 3;
    g_tutorial_highlight_colors_a[3].r = g_tutorial_highlight_colors_a[1].r = brightness;
    g_tutorial_highlight_colors_a[3].g = g_tutorial_highlight_colors_a[1].g = brightness >> 2;
    g_tutorial_highlight_colors_a[3].b = g_tutorial_highlight_colors_a[1].b = brightness >> 2;
    g_tutorial_highlight_colors_b[0].r = g_tutorial_highlight_colors_b[2].r = brightness;
    g_tutorial_highlight_colors_b[0].g = g_tutorial_highlight_colors_b[2].g = brightness >> 2;
    g_tutorial_highlight_colors_b[0].b = g_tutorial_highlight_colors_b[2].b = brightness >> 2;
    for (i = 0; i < g_tutorial_highlight_quad_count; i++) {
        gfx_enqueue_gouraud_quad(g_tutorial_highlight_corners_a[i], g_tutorial_highlight_colors_a, 1, 0x3D);
        gfx_enqueue_gouraud_quad(g_tutorial_highlight_corners_b[i], g_tutorial_highlight_colors_b, 1, 0x3D);
    }
}

/* Tutorial command: wait the number of frames in the command's operand byte
 * (counted by the VSync mode), then step past the two-byte command. Taking the
 * address of the operand field preserves the -O0 byte-address steps. */
static void tutorial_opcode_15_wait_time(void) {
    if (g_tutorial_command_active != 0) {
        g_tutorial_wait_time_remaining -= menu_get_event_speed();
        if (g_tutorial_wait_time_remaining < 0) {
            g_tutorial_command_active = 0;
            g_tutorial_command_ptr += 2;
        }
    } else {
        g_tutorial_command_active = 1;
        g_tutorial_wait_time_remaining = *(&((tutorial_wait_instruction_t*)g_tutorial_command_ptr)->frames);
    }
}

/* Tutorial command: advance on any button once the tutorial text thread has
 * ended or has stopped printing. */
static void tutorial_opcode_16_wait_for_button(void) {
    s32 running = thread_is_running(SCREEN_THREAD_SLOT_TUTORIAL_TEXT);

    if (running == 0) {
        if (g_tutorial_button_input != 0) {
            g_tutorial_command_ptr++;
        }
    } else if (screen_text_is_printing(SCREEN_THREAD_SLOT_TUTORIAL_TEXT) != 1) {
        if (g_tutorial_button_input != 0) {
            g_tutorial_command_ptr++;
        }
    }
}
