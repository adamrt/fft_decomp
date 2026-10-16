#ifndef BATTLE_EVENT_H
#define BATTLE_EVENT_H

/* Battle scenario orchestration and deferred commands produced by scripts. */

#include "psx/types.h"

/* scenario state */
extern s32 g_battle_event_frame_counter;

/* effect requests */
extern u16 g_battle_event_effect_target_runtime_id; /* 0x80165ff8 */

/* map requests */
extern u16 g_battle_event_map_command_83_arg1; /* 0x80174058 */
extern u16 g_battle_event_map_command_83_arg2; /* 0x8017405a */

/* object requests */
/* Separate triggers, command arguments, and completion status. Argument
 * domains remain provisional: map commands 0x80/0x83, polls 0x81/0x84.
 * The u16 requests and arguments are read with lhu by the consumer
 * script_process_pending_requests; every other access is a store. */
extern u16 g_battle_3d_object_use_request;    /* 0x80165fe2 */
extern u16 g_battle_event_3d_object_id;       /* 0x80173c94 */
extern u16 g_battle_event_3d_object_state;    /* 0x80173c96 */
extern u16 g_battle_field_object_use_request; /* 0x80165fe4 */

/* weather and lighting */
typedef struct script_weather_environment {
    u8 environment_id;
    u8 _unused_01;
} script_weather_environment_t;
typedef char script_weather_environment_size_must_be_2[(sizeof(script_weather_environment_t) == 2) ? 1 : -1];

extern script_weather_environment_t g_battle_event_weather_commands[];

/* music */
extern u16 g_battle_event_music_switch_fade;   /* 0x80173f56; consumer multiplies by 4 */
extern u16 g_battle_event_music_switch_volume; /* 0x80173f54; scaled 0..96 to 0..127 */
extern u16 g_battle_scenario_music_1_id;
extern u16 g_battle_scenario_music_2_id;

#endif
