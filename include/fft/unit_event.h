#ifndef UNIT_EVENT_H
#define UNIT_EVENT_H

/* Event library unit routines for BATTLE and WORLD: status staging, facing animation and event-unit lookups. */

#include "psx/libgte.h"
#include "psx/types.h"

/* unit_status_staging_t.jump_float_flags: a staged change removes the status from some unit. */
enum {
    UNIT_STATUS_STAGING_JUMP_REMOVED = 0x1,
    UNIT_STATUS_STAGING_FLOAT_REMOVED = 0x2,
};

typedef enum unit_status_staging_state {
    UNIT_STATUS_STAGING_NONE = 0,
    UNIT_STATUS_STAGING_SAVED = 1,
    UNIT_STATUS_STAGING_RESTORED = 2, /* blocks a second restore */
} unit_status_staging_state_e;

/* unit_status_staging_t.inflict_status_mode of one unit: the change the
 * InflictStatus event thread stages and unit_apply_staged_status_data
 * applies. */
typedef enum unit_inflict_status_mode {
    UNIT_INFLICT_STATUS_MODE_NONE = 0,
    UNIT_INFLICT_STATUS_MODE_REVIVE = 1, /* 1 HP, Critical added, Dead removed */
    UNIT_INFLICT_STATUS_MODE_CRYSTAL = 2,
    UNIT_INFLICT_STATUS_MODE_POISON = 3,
} unit_inflict_status_mode_e;

/* Rows of g_unit_status_staging_masks, the per-byte status removal masks
 * applied while an event stages a unit: three eight-byte sets, of which the
 * first STATUS_BYTE_COUNT bytes are used. */
typedef enum unit_status_staging_mask {
    UNIT_STATUS_STAGING_MASK_BATTLE_STARTED = 0,
    UNIT_STATUS_STAGING_MASK_PRE_BATTLE = 1,
    /* Monster skillsets, both Altima forms and the six undead jobs. */
    UNIT_STATUS_STAGING_MASK_PRE_BATTLE_MONSTER_OR_UNDEAD = 2,
} unit_status_staging_mask_e;

enum { UNIT_STATUS_STAGING_MASK_COUNT = 3 };

/* unit_animation_state_t.rotate_mode: how the facing nibble steps toward target_facing. */
typedef enum unit_rotate_mode {
    UNIT_ROTATE_MODE_SHORTEST = 0,  /* the direction with fewer steps */
    UNIT_ROTATE_MODE_INCREMENT = 1, /* facing + 1 each step */
    UNIT_ROTATE_MODE_DECREMENT = 2, /* facing - 1 each step */
} unit_rotate_mode_e;

#endif
