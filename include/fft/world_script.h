#ifndef WORLD_SCRIPT_H
#define WORLD_SCRIPT_H

/* WORLD's scripts and input: WORLD's state for the event interpreter, the tutorials, music requests and controller
 * input. */

#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_card.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "psx/types.h"

/* event script */
/* Operand pairs that one WORLD script_execute_event opcode stores together;
 * each second 16-bit value sits off a 32-bit alignment boundary in WORLD bss, so each pair is
 * one object. */
typedef struct script_command_args {
    s16 arg1;
    s16 arg2;
} script_command_args_t;

typedef struct script_music_switch {
    s16 volume;
    s16 fade;
} script_music_switch_t;

extern s16 g_script_effect_target_runtime_id;
extern script_command_args_t g_script_map_command_80_args;
extern script_command_args_t g_script_map_command_83_args;
extern script_music_switch_t g_script_music_switch;
extern s16 g_script_field_object_use_request;
extern s16 g_script_3d_object_use_request;
extern s16 g_script_opcode_dc_value; /* EVENT_OPCODE_UNKNOWN_DC stores 13; nothing reads it */

/* tutorials */
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
    TUTORIAL_CMD_END = 0x17,
};

/* Serialized tutorial commands; opcode selects the operand record. */
typedef struct tutorial_message_instruction {
    u8 opcode;
    u8 text_id_le[2];
    u8 y_offset;
} tutorial_message_instruction_t;
typedef char world_tutorial_message_instruction_size_must_be_4[sizeof(tutorial_message_instruction_t) == 4 ? 1 : -1];

typedef struct tutorial_dialog_instruction {
    u8 opcode;
    u8 text_id_le[2];
} tutorial_dialog_instruction_t;
typedef char world_tutorial_dialog_instruction_size_must_be_3[sizeof(tutorial_dialog_instruction_t) == 3 ? 1 : -1];

typedef struct tutorial_circle_instruction {
    u8 opcode;
    u8 x;
    u8 y;
    u8 radius;
    u8 scale_x;
    u8 scale_y;
} tutorial_circle_instruction_t;
typedef char world_tutorial_circle_instruction_size_must_be_6[sizeof(tutorial_circle_instruction_t) == 6 ? 1 : -1];

typedef struct tutorial_wait_instruction {
    u8 opcode;
    u8 frames;
} tutorial_wait_instruction_t;
typedef char world_tutorial_wait_instruction_size_must_be_2[sizeof(tutorial_wait_instruction_t) == 2 ? 1 : -1];

/* The current game is serialized here while a tutorial runs; its message
 * text and command stream follow the saved bytes at +0x1e00/+0x1e04. */
typedef struct tutorial_state {
    save_image_t saved_game;
    u8* messages;
    u8* script;
} tutorial_state_t;
typedef char world_tutorial_state_size_must_be_7688[sizeof(tutorial_state_t) == 7688 ? 1 : -1];

extern s16 g_world_map_tutorial_request_state; /* world_map_request_state_e */
extern tutorial_state_t g_tutorial_state;
extern u8 g_tutorial_id;
extern u16 g_tutorial_pressed_button;
extern s16 g_tutorial_wait_time_remaining;
extern s16 g_tutorial_highlight_brightness;
extern s16 g_tutorial_buttons;
extern u16 g_tutorial_latched_button;
extern u8 g_tutorial_activation_timer;
void tutorial_start(s32 index);
void tutorial_load_files(s32 index, tutorial_state_t* record);
s32 tutorial_step(void);

/* music and sound effects */
extern s16 g_screen_sound_current_music_track_index;
extern s16 g_screen_sound_music_switch_request;
extern s16 g_screen_sound_music_unload_slot_request;
extern s32 g_screen_sound_music_volume_transition_request;
extern u8 g_screen_sound_release_held_loop;
void screen_sound_dispatch_effect(void);

/* controller input */
extern menu_input_state_t g_screen_input_frame_state;
extern u32 g_screen_input_frame_current;
extern u32 g_screen_input_frame_new;
extern u32 g_screen_input_frame_previous;
extern u32 g_screen_input_frame_initial_repeat_counter;
extern u32 g_screen_input_frame_repeat_counter;
extern u32 g_screen_input_frame_secondary_repeat_counter;
extern u32 g_script_current_input;
extern u32 g_script_previous_input;
extern u32 g_script_initial_repeat_counter;
extern u32 g_script_repeat_counter;
extern u32 g_script_secondary_repeat_counter;
extern s32 g_script_saved_event_input;
extern menu_input_timing_profile_t g_screen_input_timing_profiles[];
extern u16 g_screen_input_previous, g_screen_input_latched_button;
extern u32 g_screen_input_primary_repeat;
extern s32 g_screen_input_primary_repeat_latched;
extern s32 g_screen_input_secondary_repeat_latched;
extern u8 g_screen_input_triangle_latch;
extern u8 g_screen_input_triangle_latched;
extern u8 g_screen_input_blocked_state;
/* Countdown maintained by screen_input_update_lockout. Volatile:
 * that routine reloads it after the zero test and again before the running
 * check instead of reusing the first load. */
extern volatile u8 g_screen_input_lockout_timer;
s32 screen_input_filter(void);
s32 tutorial_get_buttons(void);
s32 screen_input_get_lockout(void);
void screen_input_update_lockout(void);

#endif
