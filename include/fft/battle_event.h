#ifndef BATTLE_EVENT_H
#define BATTLE_EVENT_H

/* Battle scenario orchestration and deferred commands produced by scripts. */

#include "fft/map.h"
#include "fft/script.h"

/* scenario state */
extern s32 g_battle_event_frame_counter;
extern u8
    g_battle_event_thread_status_snapshot[]; /* per-thread running flags sampled before the yield, indexed 1..15 */

/* input */

/* character graphics */
extern s32 g_battle_event_pending_evtchr_save_slot;           /* 0x80173f4c */
extern s32 g_battle_event_pending_loaded_evtchr_clear_slot;   /* 0x80173ca0 */
extern s32 g_battle_event_pending_reserved_vram_release_slot; /* 0x80173f48 */
extern s32 g_battle_event_pending_saved_evtchr_clear_slot;    /* 0x80173f50 */
/* 0x80173cac: packed unit ID << 8 | reserved VRAM slot; -1 absent. */
extern s32 g_battle_event_pending_unit_vram_copy;

/* effect requests */
extern u16 g_battle_event_effect_target_runtime_id; /* 0x80165ff8 */
/* 0x80166000: consumer passes mode 2 when equal to 2, otherwise mode 0.
 * The last Effect operand is not a second Y coordinate. */
extern s16 g_battle_event_effect_target_mode;
extern s16 g_battle_event_effect_target_x;
extern s16 g_battle_event_effect_target_y;
extern s16 g_battle_event_pending_effect_id; /* 0x80173cb4 */

/* map requests */
extern u16 g_battle_event_map_command_83_arg1; /* 0x80174058 */
extern u16 g_battle_event_map_command_83_arg2; /* 0x8017405a */
extern s16 g_battle_event_map_jump_in_request;
extern s16 g_battle_event_pending_map_jump_out_2_id; /* 0x8017405c */
/* Deferred BATTLE commands: producer 0x80143bd8, consumer
 * 0x80143418..0x801439c0. The outer update at 0x80142d58 clears one-shot
 * requests, schedules script threads, then applies commands on its normal
 * path. These are separate globals, not a contiguous struct. Signed pending
 * slots use -1; request bits and polled states do not. */
extern s16 g_battle_event_pending_map_jump_out_id; /* 0x80174054 */
extern s16 g_battle_event_pending_map_state;       /* 0x80173f64; script variable 0x24 */

/* object requests */
/* Separate triggers, command arguments, and completion status. Argument
 * domains remain provisional: map commands 0x80/0x83, polls 0x81/0x84.
 * The u16 requests and arguments are read with lhu by the consumer
 * script_process_pending_requests; every other access is a store. */
extern u16 g_battle_3d_object_use_request;    /* 0x80165fe2 */
extern u16 g_battle_3d_object_wait_status;    /* 0x8016606e */
extern u16 g_battle_event_3d_object_id;       /* 0x80173c94 */
extern u16 g_battle_event_3d_object_state;    /* 0x80173c96 */
extern u16 g_battle_field_object_use_request; /* 0x80165fe4 */
extern u16 g_battle_field_object_wait_status; /* 0x80166070 */

/* weather and lighting */
typedef struct script_weather_environment {
    u8 environment_id;
    u8 _unused_01;
} script_weather_environment_t;
typedef char script_weather_environment_size_must_be_2[(sizeof(script_weather_environment_t) == 2) ? 1 : -1];

extern map_background_gradient_colors_t g_battle_event_background_colors;
extern script_weather_environment_t g_battle_event_weather_commands[];
/* 0x80173f68: packed weather flags; consumer replaces it with a table index. */
extern s16 g_battle_event_weather_request;

/* music */
extern u16 g_battle_event_music_switch_fade;   /* 0x80173f56; consumer multiplies by 4 */
extern u16 g_battle_event_music_switch_volume; /* 0x80173f54; scaled 0..96 to 0..127 */
extern u16 g_battle_scenario_music_1_id;
extern u16 g_battle_scenario_music_2_id;

#endif
