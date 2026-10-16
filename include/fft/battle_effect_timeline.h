#ifndef BATTLE_EFFECT_TIMELINE_H
#define BATTLE_EFFECT_TIMELINE_H

/* Effect timelines: channels, sound and color tracks, camera keyframes, record counters and the effect camera. */

#include "fft/gfx.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* timeline shape */
/* Particle channels and sound tracks of each timeline phase. */
enum {
    EFFECT_PARTICLE_CHANNEL_COUNT = 5,
    EFFECT_SOUND_TRACK_COUNT = 3,
};

/* Keyframe slots of a color track. */
enum {
    EFFECT_PALETTE_TRACK_KEYFRAMES = 33,
};

/* particle channels */
/* One 0x80-byte particle channel of an effect timeline. Keyframe k lasts
 * from frame_start[k] to frame_start[k + 1]. Entering it runs its 16-bit action value
 * (unit reactions, and binding a native handler to a callback slot); on each
 * of its frames it spawns its emitter, through the handler of the action's
 * callback slot when that is set. Keyframe 0 is passed over without spawning;
 * emitter[k - 1] belongs to keyframe k. Retail channels have up to 24 keyframes. */
typedef struct effect_keyframe_table {
    u16 frame_start[25]; /* 0x00: keyframe boundaries */
    u8 emitter[24];      /* 0x32: keyframe 1 starts at element 0; emitter index + 1, 0 for none */
    u16 action[25];      /* 0x4a: EFFECT_KEYFRAME_ACTION_*, including the final transition */
    u8 _unused_7c[2];    /* 0x7c */
    s16 last_index;      /* 0x7e; highest valid keyframe index */
} effect_keyframe_table_t;
typedef char effect_keyframe_table_size_must_be_128[(sizeof(effect_keyframe_table_t) == 128) ? 1 : -1];

/* A particle channel keyframe's 16-bit action value (effect_keyframe_table_t.action), run by
 * battle_effect_run_keyframe_actions when the keyframe is entered. Bit 7 is set in a few retail
 * files and read by nothing. */
enum {
    EFFECT_KEYFRAME_ACTION_CALLBACK_MASK = 0x0007,
    EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY = 0x0008,
    EFFECT_KEYFRAME_ACTION_FLAG_POST_ACTION_DISPLAY = 0x0010,
    EFFECT_KEYFRAME_ACTION_FLAG_UPDATE_DISPLAY = 0x0020,
    EFFECT_KEYFRAME_ACTION_FLAG_TARGET_ANIMATION = 0x0040,
    EFFECT_KEYFRAME_ACTION_ANIMATION_MASK = 0xff00,
    EFFECT_KEYFRAME_ACTION_ANIMATION_SHIFT = 8,
};

/* color tracks */
/* The preset byte of a color track keyframe. */
enum {
    EFFECT_COLOR_KEYFRAME_MODE_MASK = 0x7f, /* the map color mode (MAP_COLOR_MODE_*) the color is applied in */
    EFFECT_COLOR_KEYFRAME_APPLY = 0x80,     /* clear on the unit tracks: the keyframe only waits */
};

/* A color track of an effect timeline: timed palette changes of the
 * surroundings, the caster or the targets. A keyframe lasts duration * 8
 * frames (one frame for 0); a track of count keyframes plays count - 1. */
typedef struct effect_palette_track {
    s16 duration[EFFECT_PALETTE_TRACK_KEYFRAMES];
    rgb_s8_t color[EFFECT_PALETTE_TRACK_KEYFRAMES];
    u8 preset[EFFECT_PALETTE_TRACK_KEYFRAMES];
    s16 count;
} effect_palette_track_t;
typedef char effect_palette_track_size_must_be_200[sizeof(effect_palette_track_t) == 200 ? 1 : -1];

/* The screen color track: the palette-track layout with a second color per
 * keyframe. An APPLY keyframe shifts the map background gradient by twice
 * its color; any other sets the gradient's two stops to its two colors. */
typedef struct effect_background_track {
    s16 duration[EFFECT_PALETTE_TRACK_KEYFRAMES];
    rgb_s8_t color[EFFECT_PALETTE_TRACK_KEYFRAMES];
    rgb_s8_t second_color[EFFECT_PALETTE_TRACK_KEYFRAMES];
    u8 preset[EFFECT_PALETTE_TRACK_KEYFRAMES];
    s16 count;
} effect_background_track_t;
typedef char effect_background_track_size_must_be_300[(sizeof(effect_background_track_t) == 300) ? 1 : -1];

/* One phase's color tracks. The surroundings are the map's field color and
 * the spectators: the living units that are neither a target nor the caster. */
typedef struct effect_timeline_color_tracks {
    effect_palette_track_t surroundings; /* 0x000: the map and the units that only look on */
    effect_palette_track_t caster;       /* 0x0c8 */
    effect_palette_track_t target;       /* 0x190 */
    effect_background_track_t screen;    /* 0x258 */
} effect_timeline_color_tracks_t;

/* sound tracks */
/* A sound track of the child timeline: up to 17 entries, each naming an
 * effect sound channel to play when it is entered. The parent timeline's
 * tracks hold nine (effect_timeline_t.sound_tracks). */
typedef struct effect_child_sound_track {
    u16 duration[17]; /* 0x00; frames each entry lasts */
    u8 channel[17];   /* 0x22; 0 and 1 none, otherwise a sound channel of the flags section + 2 */
    u8 _padding_33;   /* aligns count */
    s16 count;        /* 0x34 */
} effect_child_sound_track_t;

/* A parent-timeline sound track: channel 0/1 is silent; other values select
 * the effect's sound channel plus two. */
typedef struct effect_sound_track {
    u16 durations[9]; /* 0x00 */
    u8 channels[9];   /* 0x12 */
    u8 _padding_1b;   /* aligns entry_count */
    s16 entry_count;  /* 0x1c */
} effect_sound_track_t;

/* camera tables */
/* Bits 0-2 of an effect timeline camera command: the tracks the keyframe moves. */
enum {
    EFFECT_CAMERA_TRACK_ROTATION = 0x1,
    EFFECT_CAMERA_TRACK_POSITION = 0x2,
    EFFECT_CAMERA_TRACK_ZOOM = 0x4,
};

/* Bits 0x1e0 of an effect timeline camera command: where the track's target value comes from.
 * The zoom command takes the command's vector for every source but CURRENT and SAVED. */
typedef enum effect_camera_source {
    EFFECT_CAMERA_SOURCE_TARGET = 0x000,           /* the effect target the command names */
    EFFECT_CAMERA_SOURCE_CURRENT_YAW_ROLL = 0x020, /* angle: given pitch; current yaw and roll plus the given */
    EFFECT_CAMERA_SOURCE_GIVEN = 0x040,            /* the command's vector */
    EFFECT_CAMERA_SOURCE_SAVED_PITCH = 0x060,      /* angle: saved pitch, current yaw and roll */
    EFFECT_CAMERA_SOURCE_MAP_CENTER = 0x080,
    EFFECT_CAMERA_SOURCE_CURRENT = 0x0c0,
    EFFECT_CAMERA_SOURCE_SAVED = 0x100,
    EFFECT_CAMERA_SOURCE_CASTER = 0x140,
    EFFECT_CAMERA_SOURCE_TARGET_AVERAGE = 0x180, /* position: the mean of all unit targets */
    EFFECT_CAMERA_SOURCE_CURSOR_TILE = 0x1c0,
} effect_camera_source_e;

enum { EFFECT_CAMERA_SOURCE_MASK = 0x1e0 };

/* Bits 0x1e00 of an effect timeline camera command, kept in g_battle_effect_camera_rotation_mode,
 * _position_mode and _zoom_mode while the move runs: how the track goes from its start to its
 * target over the command's duration. */
typedef enum effect_camera_mode {
    EFFECT_CAMERA_MODE_IDLE = 0x0000,
    EFFECT_CAMERA_MODE_SNAP = 0x0200,
    EFFECT_CAMERA_MODE_COSINE = 0x0400,
    EFFECT_CAMERA_MODE_COSINE_Q12 = 0x0600,
    EFFECT_CAMERA_MODE_LINEAR = 0x0800,
    EFFECT_CAMERA_MODE_ADD_ACCELERATING = 0x0a00, /* add a step that grows from 0 to the target */
    EFFECT_CAMERA_MODE_ADD = 0x0c00,              /* add the target every frame */
    EFFECT_CAMERA_MODE_ADD_DECELERATING = 0x0e00, /* add a step that shrinks from the target to 0 */
    EFFECT_CAMERA_MODE_SHAKE_GROWING = 0x1000,    /* random offsets around the start */
    EFFECT_CAMERA_MODE_SHAKE = 0x1200,
    EFFECT_CAMERA_MODE_SHAKE_DECAYING = 0x1400,
} effect_camera_mode_e;

enum { EFFECT_CAMERA_MODE_MASK = 0x1e00 };

/* 21-keyframe camera table of the parent timeline's two phases. A keyframe
 * moves the tracks its 16-bit command names (EFFECT_CAMERA_TRACK_*)
 * toward its values, arriving at end_frame; a track takes its next keyframe
 * only once its previous move has ended. */
typedef struct effect_camera_keyframes_21 {
    s16 end_frame[21];      /* frame the keyframe's moves arrive, counted from the start of the table's phase */
    vec3s16_t rotation[21]; /* pitch, yaw, roll */
    vec3s16_t position[21];
    vec3s16_t zoom[21];
    s16 command[21]; /* EFFECT_CAMERA_TRACK_* | effect_camera_source_e | effect_camera_mode_e */
    s16 last_keyframe_index;
} effect_camera_keyframes_21_t;

/* The camera table battle_camera_find_active_keyframe found its keyframe in: the high half of the
 * index it reports. */
enum {
    EFFECT_CAMERA_TABLE_PHASE1 = 0,
    EFFECT_CAMERA_TABLE_CHILD = 1,
    EFFECT_CAMERA_TABLE_PHASE2 = 2,
};

/* timeline section */
/* The child timeline, eight bytes into the timeline section
 * (g_battle_effect_child_timeline): its duration and its channels.
 * effect_timeline_t names the section's other parts. */
typedef struct effect_child_timeline {
    u16 _unused_000;
    u16 duration; /* 0x002; frames: effect_timeline_t.child.duration */
    effect_keyframe_table_t particle[EFFECT_PARTICLE_CHANNEL_COUNT]; /* 0x004 */
    effect_child_sound_track_t sound[EFFECT_SOUND_TRACK_COUNT];      /* 0x284 */
    effect_palette_track_t surroundings;                             /* 0x326 */
    effect_palette_track_t caster;                                   /* 0x3ee */
    effect_palette_track_t target;                                   /* 0x4b6 */
    effect_background_track_t screen;                                /* 0x57e */
} effect_child_timeline_t;
typedef char effect_child_timeline_size_must_be_1706[sizeof(effect_child_timeline_t) == 1706 ? 1 : -1];

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

/* Parent and child timeline pacing curves, two four-bit values per byte.
 * Even frames use the low nibble; pacing values 1-9 are applied. */
typedef struct effect_timing_curves {
    u8 parent_timeline[300]; /* 0x000 */
    u8 child_timeline[300];  /* 0x12c */
} effect_timing_curves_t;
typedef char effect_timing_curves_size_must_be_600[sizeof(effect_timing_curves_t) == 600 ? 1 : -1];

extern effect_child_timeline_t* g_battle_effect_child_timeline;
extern effect_timeline_t* g_battle_effect_timeline;
extern const effect_timing_curves_t* g_battle_effect_timing_curves;

/* timeline state */
/* Steppers use the named tracks; resets clear each complete slot array.
 * The reset extents include slots that no track stepper reads. */
enum {
    EFFECT_CHILD_TRACK_RESET_SLOT_COUNT = 13,
    EFFECT_PARENT_TRACK_RESET_SLOT_COUNT = 26,
};

/* Child timeline counters: twelve active tracks and one extra reset slot. */
typedef union effect_child_track_counters {
    s16 reset_slots[EFFECT_CHILD_TRACK_RESET_SLOT_COUNT];
    struct {
        s16 particle[EFFECT_PARTICLE_CHANNEL_COUNT];
        s16 sound[EFFECT_SOUND_TRACK_COUNT];
        s16 surroundings;
        s16 caster;
        s16 target;
        s16 screen;
        s16 _unused_18; /* extra 16-bit value cleared by the child reset */
    } tracks;
} effect_child_track_counters_t;

typedef union effect_child_track_steps {
    s16 reset_slots[EFFECT_CHILD_TRACK_RESET_SLOT_COUNT];
    struct {
        u16 particle[EFFECT_PARTICLE_CHANNEL_COUNT]; /* loaded unsigned by opcode 28 */
        u8 _unused_0a[0x10];                         /* remaining slots are reset but not stepped */
    } tracks;
} effect_child_track_steps_t;

typedef struct effect_child_timeline_state {
    effect_child_track_counters_t keyframe;  /* 0x00; record +0x2a */
    effect_child_track_counters_t remaining; /* 0x1a; record +0x44 */
    effect_child_track_steps_t step;         /* 0x34; record +0x5e */
} effect_child_timeline_state_t;
typedef char effect_child_timeline_state_size_must_be_78[sizeof(effect_child_timeline_state_t) == 78 ? 1 : -1];

/* Parent counters: ten particle tracks, fourteen color/sound tracks and
 * two extra 16-bit values. The reset clears all 26, not just the 24 stepped tracks. */
typedef union effect_parent_track_counters {
    s16 reset_slots[EFFECT_PARENT_TRACK_RESET_SLOT_COUNT];
    struct {
        s16 particle[2][EFFECT_PARTICLE_CHANNEL_COUNT];
        s16 surroundings[2];
        s16 caster[2];
        s16 target[2];
        s16 screen[2];
        s16 sound[EFFECT_SOUND_TRACK_COUNT][2];
        s16 _unused_30[2]; /* extra 16-bit values cleared by the parent reset */
    } tracks;
} effect_parent_track_counters_t;

typedef union effect_parent_track_steps {
    s16 reset_slots[EFFECT_PARENT_TRACK_RESET_SLOT_COUNT];
    struct {
        s16 particle[2][EFFECT_PARTICLE_CHANNEL_COUNT];
        u8 _unused_14[0x10]; /* reset only */
        /* The sound tracks' step slots, at the same offset as the counters'
         * sound tracks. Opcode 29 passes the address of each high byte to the
         * sound helper, which never reads it. */
        u8 sound[EFFECT_SOUND_TRACK_COUNT][2][2]; /* record +0xba */
        s16 _unused_30[2];                        /* extra 16-bit values cleared by the parent reset */
    } tracks;
} effect_parent_track_steps_t;

typedef struct effect_parent_timeline_state {
    s16 spawn_countdown;                      /* 0x00; record +0x2a */
    u16 spawned_count;                        /* 0x02; record +0x2c */
    effect_parent_track_counters_t keyframe;  /* 0x04; record +0x2e */
    effect_parent_track_counters_t remaining; /* 0x38; record +0x62 */
    effect_parent_track_steps_t step;         /* 0x6c; record +0x96 */
    u8 _unused_a0[6];                         /* record +0xca..0xcf; untouched by both timeline views */
} effect_parent_timeline_state_t;

/* Opcodes 28 and 29 interpret the same record storage differently. */
typedef union effect_timeline_state {
    effect_child_timeline_state_t child;
    effect_parent_timeline_state_t parent;
} effect_timeline_state_t;
typedef char effect_timeline_state_size_must_be_166[sizeof(effect_timeline_state_t) == 166 ? 1 : -1];

/* The target and frame travel with the counters of the active timeline. */
typedef struct effect_record_timeline {
    s16 target_index;              /* 0x00; index into effect-context targets */
    s16 frame;                     /* 0x02 */
    effect_timeline_state_t state; /* 0x04; opcode 28 child or opcode 29 parent */
} effect_record_timeline_t;
typedef char effect_record_timeline_size_must_be_170[sizeof(effect_record_timeline_t) == 170 ? 1 : -1];

/* camera */
/* The effect camera's rotation, position and zoom tracks: each has its current value, the start
 * and target of the running move, its mode (effect_camera_mode_e), duration and frame counter,
 * and the copy that EFFECT_CAMERA_SOURCE_SAVED reads. */
extern VECTOR g_battle_effect_camera_position_current;
extern s32 g_battle_effect_camera_position_duration;
extern s32 g_battle_effect_camera_position_frame;
extern u32 g_battle_effect_camera_position_mode;
extern VECTOR g_battle_effect_camera_position_saved;
extern VECTOR g_battle_effect_camera_position_start;
extern VECTOR g_battle_effect_camera_position_target;
extern SVECTOR g_battle_effect_camera_rotation_current;
extern s32 g_battle_effect_camera_rotation_duration;
/* Reset by battle_camera_execute_angle_command. */
extern s32 g_battle_effect_camera_rotation_frame;
extern u32 g_battle_effect_camera_rotation_mode;
extern SVECTOR g_battle_effect_camera_rotation_saved;
extern SVECTOR g_battle_effect_camera_rotation_start;
extern SVECTOR g_battle_effect_camera_rotation_target;
extern VECTOR g_battle_effect_camera_zoom_current;
extern s32 g_battle_effect_camera_zoom_duration;
extern s32 g_battle_effect_camera_zoom_frame;
extern u32 g_battle_effect_camera_zoom_mode;
extern VECTOR g_battle_effect_camera_zoom_saved;
extern VECTOR g_battle_effect_camera_zoom_start;
extern VECTOR g_battle_effect_camera_zoom_target;

void battle_effect_seed_camera_tracks(void);
void battle_effect_init_camera_and_screen_data(void);

#endif
