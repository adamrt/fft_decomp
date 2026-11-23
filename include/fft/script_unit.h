#ifndef SCRIPT_UNIT_H
#define SCRIPT_UNIT_H

/* The event library's units in BATTLE and WORLD: status staging records and operations, facing and pending
 * animations, and unit lookups and coordinates. */

#include "fft/status.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* status staging */
/* unit_status_staging_t.jump_float_flags: a staged change removes the status from some unit. */
enum { UNIT_STATUS_STAGING_JUMP_REMOVED = 0x1, UNIT_STATUS_STAGING_FLOAT_REMOVED = 0x2 };

/* unit_status_staging_t.state of one unit. */
typedef enum unit_status_staging_state {
    UNIT_STATUS_STAGING_NONE = 0,
    UNIT_STATUS_STAGING_SAVED = 1,
    UNIT_STATUS_STAGING_RESTORED = 2 /* blocks a second restore */
} unit_status_staging_state_e;

/* unit_status_staging_t.inflict_status_mode of one unit: the change the
 * InflictStatus event thread stages and apply_staged_status
 * applies. */
typedef enum unit_inflict_status_mode {
    UNIT_INFLICT_STATUS_MODE_NONE = 0,
    UNIT_INFLICT_STATUS_MODE_REVIVE = 1, /* 1 HP, Critical added, Dead removed */
    UNIT_INFLICT_STATUS_MODE_CRYSTAL = 2,
    UNIT_INFLICT_STATUS_MODE_POISON = 3
} unit_inflict_status_mode_e;

/* Shared event snapshot for all 21 simulation slots. battle_unit_status_staging_save
 * writes the saved fields; apply_staged_status restores them and
 * applies intervening status differences through BATTLE callbacks. */
typedef struct unit_status_staging {
    u8 innate[BATTLE_UNIT_SLOT_COUNT][STATUS_BYTE_COUNT];
    u8 current[BATTLE_UNIT_SLOT_COUNT][STATUS_BYTE_COUNT];    /* 0x069 */
    u8 inflicted[BATTLE_UNIT_SLOT_COUNT][STATUS_BYTE_COUNT];  /* 0x0d2 */
    u8 removed[BATTLE_UNIT_SLOT_COUNT][STATUS_BYTE_COUNT];    /* 0x13b */
    u8 added[BATTLE_UNIT_SLOT_COUNT][STATUS_BYTE_COUNT];      /* 0x1a4 */
    u8 status_ct[BATTLE_UNIT_SLOT_COUNT][STATUS_TIMED_COUNT]; /* 0x20d */
    u8 charged_ability_ct[BATTLE_UNIT_SLOT_COUNT];            /* 0x35d */
    u8 death_counter[BATTLE_UNIT_SLOT_COUNT];                 /* 0x372 */
    u8 team_flags[BATTLE_UNIT_SLOT_COUNT];                    /* 0x387 */
    u8 state[BATTLE_UNIT_SLOT_COUNT];                         /* 0x39c; 0 not staged, 1 saved, 2 restored */
    u8 inflict_status_mode[BATTLE_UNIT_SLOT_COUNT];           /* 0x3b1; unit_inflict_status_mode_e */
    u16 hp[BATTLE_UNIT_SLOT_COUNT];                           /* 0x3c6 */
    u16 jump_float_flags;                                     /* 0x3f0; UNIT_STATUS_STAGING_*_REMOVED */
} unit_status_staging_t;
typedef char unit_status_staging_size_must_be_1010[sizeof(unit_status_staging_t) == 1010 ? 1 : -1];

s32 battle_unit_status_staging_save(s32 battle_unit_id);
void battle_unit_status_staging_clear(void);
void battle_unit_status_staging_run(u16 unit_id, u16 restore);

/* facing and animation */
/* unit_animation_state_t.rotate_mode: how the facing nibble steps toward target_facing. */
typedef enum unit_rotate_mode {
    UNIT_ROTATE_MODE_SHORTEST = 0,  /* the direction with fewer steps */
    UNIT_ROTATE_MODE_INCREMENT = 1, /* facing + 1 each step */
    UNIT_ROTATE_MODE_DECREMENT = 2  /* facing - 1 each step */
} unit_rotate_mode_e;

/* Per-unit facing/animation state written by the event interpreters and
 * advanced every frame by map_unit_update_pending_animations.
 * Seven bytes per unit, 21 units. */
typedef struct unit_animation_state {
    u8 target_facing;
    u8 rotate_mode;     /* unit_rotate_mode_e */
    u8 frames_per_step; /* facing steps once step_counter reaches this */
    u8 step_counter;
    u8 rotating;       /* nonzero while stepping toward target_facing */
    u8 initial_facing; /* facing captured when animation states reset (0xff when
                          absent); RotateUnit direction 0x14 rotates back to it */
    u8 delay;          /* frames to wait before the first step */
} unit_animation_state_t;
typedef char unit_animation_state_size_must_be_7[sizeof(unit_animation_state_t) == 7 ? 1 : -1];

extern u16 g_unit_graphics_load_pending;                 /* unit graphics load result: 0 done, 1 pending */
extern unit_animation_state_t g_unit_animation_states[]; /* event-script rotation state for each map unit slot */

/* WalkToAnim: requested animation applied after movement stops; 0 means none. */
extern u16 g_unit_pending_animation_ids[];
extern u16 g_unit_was_moving_latch[]; /* a unit with a pending animation was moving on the last update */

void map_unit_update_pending_animations(void);
void map_unit_reset_animation_states(void);
void map_unit_set_event_animation(s32 map_unit_id, u32 animation);
void map_unit_set_event_facing(s32 map_unit_id, s32 facing);
s32 map_unit_check_camera_facing_by_event_unit_id(s32 event_unit_id);
s32 script_map_unit_is_moving(s32 map_unit_id);

/* event unit lookups */
extern SVECTOR g_unit_scratch_coords; /* a unit's map coordinates or projected screen position */

s32 battle_unit_get_id_by_event_unit_id(s32 event_unit_id);
void map_unit_copy_world_coords(u32 map_unit_id, SVECTOR* world_coords);
void map_unit_copy_world_vector(u32 map_unit_id, VECTOR* destination);
void map_unit_project_screen_position(u32 map_unit_id, SVECTOR* screen_coordinates);
void map_unit_copy_map_coords(u32 map_unit_id, SVECTOR* coords);

/* unit commands */
void battle_unit_increase_blue_team_brave_by_10(void);

#endif
