#ifndef BATTLE_EFFECT_SCRIPT_H
#define BATTLE_EFFECT_SCRIPT_H

/* BATTLE effect files: loading and playback, timelines and the effect script. */

#include "fft/battle_effect.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "psx/types.h"

/* g_battle_effect_load_state: the steps of battle_effect_update_file_load, which loads the ability's effect file
 * while its charge secondary effect plays. */
typedef enum effect_load_state {
    EFFECT_LOAD_START = 0,            /* start reading the file */
    EFFECT_LOAD_READING = 1,          /* wait for the read */
    EFFECT_LOAD_COUNTDOWN = 2,        /* wait out the 60 frames given to a charge secondary effect */
    EFFECT_LOAD_ENDING_SECONDARY = 3, /* wait for the charge secondary effect to end */
} effect_load_state_e;

/* g_battle_effect_playback_phase: the stages of battle_effect_update_stage. */
typedef enum effect_playback_phase {
    EFFECT_PLAYBACK_PHASE_IDLE = 0,
    EFFECT_PLAYBACK_PHASE_REQUESTED = 1, /* waiting for battle_effect_play */
    EFFECT_PLAYBACK_PHASE_INIT_PALETTE = 2,
    EFFECT_PLAYBACK_PHASE_START_SCRIPT = 3, /* bind the effect file's sections and start its script */
    EFFECT_PLAYBACK_PHASE_RUNNING = 4,
    EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT = 6, /* start the script again without rebinding the file */
} effect_playback_phase_e;

/* The timeline section of an effect file (effect_file_header_t.timeline).
 *
 * A script that spawns children (opcode 29) runs the parent timeline in three
 * stages: phase 1 for phase1_duration frames; then one child record per
 * effect target, spawn_delay frames apart, each running the child timeline
 * for child.duration frames; then phase 2, which starts when the last child's
 * timeline ends, for phase2_duration frames. A script without children
 * (opcode 28 on the root record) runs only the child timeline, once.
 *
 * The child timeline's channels are the effect_child_timeline_t that
 * g_battle_effect_child_timeline addresses at 0x008; its camera table is the
 * 17-keyframe one at 0x6b2, laid out like effect_camera_keyframes_21_t. Each
 * phase has five particle channels, three sound tracks, four color tracks
 * and a 21-keyframe camera table. */
typedef struct effect_timeline {
    u16 _unused_0000;
    u16 phase2_duration;           /* 0x002 */
    u16 phase1_duration;           /* 0x004 */
    u16 spawn_delay;               /* 0x006; frames between child spawns (see effect_flags_section_t) */
    effect_child_timeline_t child; /* 0x008; child duration, particle, sound and color tracks */
    s16 end_frame[17];             /* 0x6b2; child camera table: frame each keyframe's moves arrive */
    vec3s16_t rotation[17];        /* 0x6d4 */
    vec3s16_t position[17];        /* 0x73a */
    vec3s16_t zoom[17];            /* 0x7a0 */
    s16 command[17];               /* 0x806; as effect_camera_keyframes_21_t.command */
    s16 last_keyframe_index;       /* 0x828; highest valid keyframe index */
    effect_keyframe_table_t particle_channels[2][EFFECT_PARTICLE_CHANNEL_COUNT]; /* 0x82a; [phase][channel] */
    effect_sound_track_t sound_tracks[2][EFFECT_SOUND_TRACK_COUNT];              /* 0xd2a; [phase][track] */
    effect_timeline_color_tracks_t color_tracks[2];                              /* 0xdde */
    effect_camera_keyframes_21_t phase1_camera;                                  /* 0x14e6 */
    effect_camera_keyframes_21_t phase2_camera;                                  /* 0x16b6 */
} effect_timeline_t;

typedef char effect_timeline_size_must_be_6278[(sizeof(effect_timeline_t) == 6278) ? 1 : -1];

/* An effect script instruction starts with a 16-bit header, its opcode in the low nine bits
 * (g_battle_effect_instructions holds 120 handlers; retail scripts stay below
 * 2e) and small operands in the bits above, followed by up to three operand
 * 16-bit values. Script offsets are bytes from the start of the script. */
enum { EFFECT_SCRIPT_OPCODE_MASK = 0x1ff };

/* What an opcode handler returns to battle_effect_run_block. */
typedef enum effect_script_result {
    EFFECT_SCRIPT_RESULT_YIELD = 0,    /* stop for this frame */
    EFFECT_SCRIPT_RESULT_CONTINUE = 1, /* run the next instruction */
    EFFECT_SCRIPT_RESULT_ABORT = 2,    /* the script has ended: free the record */
} effect_script_result_e;

/* Opcodes in the low seven bits of a sprite-sequence control byte. */
typedef enum effect_sequence_opcode {
    EFFECT_SEQUENCE_OP_LOOP = 1,       /* back to the first byte */
    EFFECT_SEQUENCE_OP_SET_OFFSET = 2, /* two s16 sprite offsets follow */
    EFFECT_SEQUENCE_OP_ADD_OFFSET = 3, /* two s8 sprite offset deltas follow */
} effect_sequence_opcode_e;

/* The high bit distinguishes control bytes from ordinary frame indices. */
enum {
    EFFECT_SEQUENCE_OP_MASK = 0x7f,
    EFFECT_SEQUENCE_OP_FLAG = 0x80,
};

/* loading and playback */
/* Action/reaction ability IDs index directly; event IDs map after those rows. */
extern s16 g_battle_effect_ability_files[];
extern effect_file_header_t* g_battle_effect_data_ptrs[EFFECT_FILE_ID_COUNT];
extern file_extent_t g_battle_effect_disc_entries[EFFECT_FILE_ID_COUNT];
extern const effect_flags_section_t* g_battle_effect_flags_section;
extern s16 g_battle_effect_load_countdown;
extern u8 g_battle_effect_load_state;      /* effect_load_state_e */
extern s32 g_battle_effect_playback_phase; /* effect_playback_phase_e */
void battle_effect_call_play(void);
void battle_effect_cancel_playback_request(void);
s32 battle_effect_get_playback_phase(void);
s32 battle_effect_update_file_load(void);
void battle_effect_init_system(void);
s32 battle_effect_is_item_ability(s32 ability_id);
u32 battle_effect_ability_has_no_file(s32 ability_id);
void battle_effect_play(void);
void battle_effect_replace_flags_0006(struct effect_flag_prefix* object, s32 value);
void battle_effect_reset_subsystems(void);
void battle_effect_set_ability_animation(u32 packed, s32 ability, effect_secondary_init_t* source);
void battle_effect_start_new(s32 ability_id, s32 target_mode, effect_start_target_data_t target);
s32 battle_effect_try_init_data(void);
s32 battle_effect_update_cycle(void);

/* timelines */
extern effect_child_timeline_t* g_battle_effect_child_timeline;
extern effect_timeline_t* g_battle_effect_timeline;
extern const effect_timing_curves_t* g_battle_effect_timing_curves;

/* scripts */
/* Written by effect-script opcode 05; no game source reads it. */
extern u16 g_battle_effect_unknown_801bf000;
extern effect_callback_t g_battle_effect_callback_slots[];
extern effect_script_result_e (*g_battle_effect_instructions[120])(effect_record_t*);
extern effect_callback_t g_battle_effect_native_callbacks[EFFECT_NATIVE_CALLBACK_COUNT];
extern const u8* g_battle_effect_script;
effect_script_result_e battle_effect_code_script_04_end(effect_record_t* unused_record);
effect_script_result_e battle_effect_code_script_2e_invalid(effect_record_t* unused_record);

#endif
