#ifndef BATTLE_EVENT_H
#define BATTLE_EVENT_H

/* BATTLE's side of its event scripts: the requests script instructions leave for the battle engine,
 * the scenario state the scripts keep, and the instructions BATTLE implements itself. */

#include "psx/types.h"

/* scenario state */
extern s32 g_battle_event_frame_counter; /* counts the frames the event interpreter runs */
extern u16 g_battle_scenario_music_1_id;
extern u16 g_battle_scenario_music_2_id;

/* units */
/* AddGhostUnit: when runtime_id is free, queue the graphics of a sprite with no unit stats at the
 * given tile and facing and return 1; return 0 when the id is taken. */
s32 battle_script_opcode_47_add_ghost_unit(
    s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id, s32 runtime_id, s32 flags);

/* effect requests */
extern u16 g_battle_event_effect_target_runtime_id;

/* map object requests */
/* EVENT_OPCODE_USE_3D_OBJECT and EVENT_OPCODE_USE_FIELD_OBJECT set a request with their two
 * operands; script_process_pending_requests passes them to battle_map_command_set_3d_object_state
 * and battle_map_command_start_texture_animation, and the matching wait instructions poll until
 * the object is done. Provisional: the operands' value domains. The consumer reads the u16
 * requests and operands with lhu; every other access is a store. */
extern u16 g_battle_3d_object_use_request;
extern u16 g_battle_event_3d_object_id;
extern u16 g_battle_event_3d_object_state;
extern u16 g_battle_field_object_use_request;
extern u16 g_battle_event_map_command_83_arg1; /* the field object's texture animation id */
extern u16 g_battle_event_map_command_83_arg2;

/* weather and lighting */
typedef struct script_weather_environment {
    u8 environment_id;
    u8 _unused_01;
} script_weather_environment_t;
typedef char script_weather_environment_size_must_be_2[(sizeof(script_weather_environment_t) == 2) ? 1 : -1];

extern script_weather_environment_t g_battle_event_weather_commands[];

/* music */
/* The volume and fade of a music switch; the switch itself and the other music requests are in
 * battle_sound.h. */
extern u16 g_battle_event_music_switch_fade;   /* the consumer multiplies it by 4 */
extern u16 g_battle_event_music_switch_volume; /* scaled 0..96 to 0..127 */

/* debug */
extern s16 g_battle_script_opcode_dc_value; /* EVENT_OPCODE_UNKNOWN_DC stores 13; nothing reads it */

#endif
