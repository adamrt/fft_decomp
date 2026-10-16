#ifndef WORLD_SCRIPT_H
#define WORLD_SCRIPT_H

/* WORLD's event scripts, tutorials, controller input and sound requests. */

#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/world.h"
#include "psx/types.h"

/* Operand pairs that one WORLD script_execute_event_variant opcode stores together;
 * each second 16-bit value sits off a 32-bit alignment boundary in WORLD bss, so each pair is
 * one object. */
typedef struct world_event_command_args {
    s16 arg1;
    s16 arg2;
} world_event_command_args_t;

typedef struct world_event_music_switch {
    s16 volume;
    s16 fade;
} world_event_music_switch_t;

typedef char
    world_tutorial_message_instruction_size_must_be_4[sizeof(world_tutorial_message_instruction_t) == 4 ? 1 : -1];

typedef char
    world_tutorial_dialog_instruction_size_must_be_3[sizeof(world_tutorial_dialog_instruction_t) == 3 ? 1 : -1];

typedef char
    world_tutorial_circle_instruction_size_must_be_6[sizeof(world_tutorial_circle_instruction_t) == 6 ? 1 : -1];

typedef struct world_tutorial_wait_instruction {
    u8 opcode;
    u8 frames;
} world_tutorial_wait_instruction_t;
typedef char world_tutorial_wait_instruction_size_must_be_2[sizeof(world_tutorial_wait_instruction_t) == 2 ? 1 : -1];

/* The current game is serialized here while a tutorial runs; its message
 * text and command stream follow the saved bytes at +0x1e00/+0x1e04. */
typedef struct world_tutorial_state {
    card_save_buffer_t saved_game;
    u8* messages;
    u8* script;
} world_tutorial_state_t;

typedef char world_tutorial_state_size_must_be_7688[sizeof(world_tutorial_state_t) == 7688 ? 1 : -1];

/*
 * WORLD.BIN tutorial command stream. Commands below 0x10 are one-byte shifts;
 * the rest are ID (1) followed by the listed parameters: 0x10 message id (2) +
 * y (1), 0x11 message id (2), 0x13 x, y, size, width mod, height mod (1 each),
 * 0x15 time (1).
 */
enum {
    WORLD_TUTORIAL_CMD_SHIFT_LIMIT = 0x10,
    WORLD_TUTORIAL_CMD_DISPLAY_MESSAGE = 0x10,
    WORLD_TUTORIAL_CMD_CHANGE_DIALOG = 0x11,
    WORLD_TUTORIAL_CMD_WAIT_FOR_MESSAGE = 0x12,
    WORLD_TUTORIAL_CMD_DRAW_CIRCLE = 0x13,
    WORLD_TUTORIAL_CMD_WAIT = 0x14,
    WORLD_TUTORIAL_CMD_WAIT_TIME = 0x15,
    WORLD_TUTORIAL_CMD_WAIT_FOR_BUTTON = 0x16,
    WORLD_TUTORIAL_CMD_END = 0x17,
};

/* script */
extern u32 g_world_event_current_input;
extern u32 g_world_event_initial_repeat_counter;
extern u32 g_world_event_previous_input;
extern u32 g_world_event_repeat_counter;
extern u32 g_world_event_secondary_repeat_counter;
extern u32 g_world_script_tutorial_button_input;
extern s32 g_world_script_tutorial_command_active;
extern u8 g_world_script_tutorial_id;
extern u16 g_world_script_tutorial_shift_command_bit;
extern s16 g_world_script_tutorial_wait_time_remaining;
extern s16 g_world_script_tutorial_wait_timer;
extern u16 g_world_script_tutorial_latched_button;
extern u8 g_world_script_tutorial_activation_timer;
extern s16 g_world_script_tutorial_buttons;
extern s32 g_world_script_saved_event_input;
extern u32 g_world_script_tutorial_repeat_counters[16];
extern u16 g_world_script_tutorial_previous_input;
extern u8* g_world_script_tutorial_command_ptr;
extern s16 g_world_event_effect_target_runtime_id;
extern world_event_command_args_t g_world_event_map_command_80_args;
extern world_event_command_args_t g_world_event_map_command_83_args;
extern world_event_music_switch_t g_world_event_music_switch;
s32 world_script_handle_tutorial_command(void);
void world_script_start_tutorial(s32 index);
extern s16 g_world_field_object_use_request;

/* sound */
extern s16 g_world_sound_current_music_track_index;
extern s16 g_world_sound_music_switch_request;
extern s16 g_world_sound_music_unload_slot_request;
extern s32 g_world_sound_music_volume_transition_request;
extern u8 g_world_sound_release_held_loop;
void world_sound_dispatch_effect(void);

/* gfx */
/* Scenario interpreter interfaces shared by the BATTLE and WORLD
 * script_execute_event_variant bodies. Signatures are as the interpreters bind them;
 * unverified names stay provisional. */
extern s16 g_world_3d_object_use_request;

/* tutorial */
extern s16 g_world_tutorial_categories_request_state; /* world_screen_request_state_e */
extern world_tutorial_state_t g_world_tutorial_state;
/* Sector and size pairs; flat 32-bit value access preserves the original addressing. */
extern s32 g_world_script_tutorial_message_files[7 * 2];
extern s32 g_world_script_tutorial_script_files[7 * 2];
extern point16_t g_world_tutorial_highlight_corners_a[31][2]; /* outer/middle ring samples */
extern point16_t g_world_tutorial_highlight_corners_b[31][2]; /* middle/inner ring samples */
extern rgb24_t g_world_tutorial_highlight_colors_a[4];        /* first quad corner colors */
extern rgb24_t g_world_tutorial_highlight_colors_b[4];        /* second quad corner colors */
extern u8 g_world_tutorial_highlight_quad_count;
void world_script_load_tutorial_files(s32 index, world_tutorial_state_t* record);

/* input */
extern menu_input_state_t g_world_input_frame_state;
extern u32 g_world_input_frame_current;
extern u32 g_world_input_frame_initial_repeat_counter;
extern u32 g_world_input_frame_new;
extern u32 g_world_input_frame_previous;
extern u32 g_world_input_frame_repeat_counter;
extern u32 g_world_input_frame_secondary_repeat_counter;
/* Countdown maintained by world_input_update_lockout_timer. Volatile:
 * that routine reloads it after the zero test and again before the running
 * check instead of reusing the first load. */
extern volatile u8 g_world_input_lockout_timer;
extern u16 g_world_input_previous, g_world_input_latched_button;
extern u32 g_world_input_primary_repeat;
extern menu_input_timing_profile_t g_world_input_timing_profiles[];
extern u8 g_world_input_triangle_latched;
extern u8 g_world_input_triangle_latch;
extern u8 g_world_input_blocked_state;
extern s32 g_world_input_primary_repeat_latched;
extern s32 g_world_input_secondary_repeat_latched;
s32 world_input_filter_menu(void);
s32 world_input_get_lockout_timer(void);
s32 world_input_get_tutorial_buttons(void);

#endif
