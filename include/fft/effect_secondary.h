#ifndef EFFECT_SECONDARY_H
#define EFFECT_SECONDARY_H

/* BATTLE secondary effects: their records, animations, handlers, particle groups and effect slots. */

#include "fft/effect.h"
#include "fft/effect_file.h"
#include "fft/gfx.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* secondary effects */
/* Sixteen secondary effects and 32 billboard slots; each pool adds record zero as its chain sentinel. */
enum {
    EFFECT_SECONDARY_SLOT_COUNT = 16,
    EFFECT_SECONDARY_RECORD_COUNT = EFFECT_SECONDARY_SLOT_COUNT + 1,
    EFFECT_SLOT_COUNT = 32,
    EFFECT_SLOT_RECORD_COUNT = EFFECT_SLOT_COUNT + 1
};

/* The stage a secondary effect's handler runs (effect_secondary_data_t.phase). */
typedef enum effect_secondary_phase {
    EFFECT_SECONDARY_PHASE_ENDED = 0, /* the slot is free */
    EFFECT_SECONDARY_PHASE_INITIALIZING = 1,
    EFFECT_SECONDARY_PHASE_EXECUTING = 2,
    EFFECT_SECONDARY_PHASE_FINALIZING = 3
} effect_secondary_phase_e;

/* One running secondary effect: a 0x54-byte record of
 * g_effect_secondary_data, ids 1-16 with record 0 the
 * sentinel. The secondary effects are the ones resident in BATTLE.BIN (weapon
 * hits, projectiles, the charges, the level-up and so on): each is an
 * animation id run by its handler of g_effect_secondary_handlers
 * through the phases of effect_secondary_phase_e. Records chain by id.
 * The particles a handler draws are effect slots (effect_slot_t), a
 * separate pool, whose ids it keeps in slot_ids or in its own state. */
typedef struct effect_secondary_data {
    u8 previous_slot_id; /* 0x00; the record before it on the active chain, or the next free record */
    u8 next_slot_id;     /* 0x01 */
    u8 own_slot_id;      /* 0x02; index of this record in g_effect_secondary_data */
    u8 function_id;      /* 0x03; index of its handler in g_effect_secondary_handlers */
    u16 animation_id;    /* 0x04; effect_secondary_animation_id_e */
    /* 0x06; the animation's own: the charge's element mask, then its effect_element_index_e; the
     * weapon hit's element index; the caster's sprite palette; the frames a reflect shield stays */
    u16 parameter;
    effect_secondary_phase_e phase; /* 0x08 */
    s16 timer;                      /* 0x0c; frames the handler has run */
    u8 _unused_0e[2];
    effect_target_t caster; /* 0x10; copied from the init record's caster */
    union {
        effect_target_t record; /* from the init record's target */
        struct {
            s16 _unused_00;
            s16 x; /* 0x1c; fall-dust position, 1/16 of the real coordinate */
            s16 z; /* 0x1e */
            s16 y; /* 0x20 */
        } position;
    } target;           /* 0x1a */
    s32 used_weapon_id; /* 0x24; init record used_weapon_id */
    s32 target_count;   /* 0x28; init record target_count (projectile setup) */
    /* 0x2c; live effect slots of its particle group: effect_secondary_start_group counts them up and
     * effect_secondary_update_active_slots down; the handlers finish at 0. */
    s16 active_count;
    s16 finish_timer;               /* 0x2e; counts frames after the last slot finished */
    u8 slot_ids[EFFECT_SLOT_COUNT]; /* 0x30; its effect slots, 0 for none; cleared when the record is allocated */
    void* allocation;               /* 0x50; the handler's work block; freed when the record is removed */
} effect_secondary_data_t;
typedef char effect_secondary_data_size_must_be_84[(sizeof(effect_secondary_data_t) == 84) ? 1 : -1];

typedef s32 (*effect_secondary_callback_t)(void);

extern struct effect_secondary_data* g_effect_secondary_current;

/* Set by effect_secondary_update each frame: the ordering table the secondary effects add their primitives to, and
 * the display buffer (0 or 1) whose primitives they fill. */
extern u32* g_effect_polygon_depth_data;
extern s32 g_effect_buffer_index;

s32 effect_secondary_init(s32 animation_id, s32 parameter, const struct effect_context* source);
s32 effect_secondary_update(void);
effect_secondary_phase_e effect_secondary_get_phase(u8 secondary_id);
void effect_secondary_finalize(u8 secondary_id);

/* animations */
/* Index into g_effect_secondary_animations: the secondary effects resident in BATTLE.BIN. Those
 * the code does not start by number come from two tables: the weapon type's projectile
 * (g_effect_weapon_projectile_animations) and the ability's charge animation set
 * (g_effect_secondary_by_charge_animation). */
typedef enum effect_secondary_animation_id {
    EFFECT_SECONDARY_ANIMATION_NONE = 0,
    EFFECT_SECONDARY_ANIMATION_BOW_ARROW = 1,       /* bows: the arrow model on an arc */
    EFFECT_SECONDARY_ANIMATION_SPELL_CHARGE = 2,    /* charge animation set 1 */
    EFFECT_SECONDARY_ANIMATION_CROSSBOW_BOLT = 3,   /* crossbows: the arrow model on a straight line */
    EFFECT_SECONDARY_ANIMATION_SPELL_CHARGE_04 = 4, /* the charge with parameter row 1; no table names it */
    EFFECT_SECONDARY_ANIMATION_SUMMON_CHARGE = 5,   /* charge animation set 2 */
    EFFECT_SECONDARY_ANIMATION_THROWN_STONE = 6,
    EFFECT_SECONDARY_ANIMATION_WEAPON_HIT = 7,
    EFFECT_SECONDARY_ANIMATION_WEAPON_HIT_08 = 8, /* the weapon hit handler again; nothing starts it */
    EFFECT_SECONDARY_ANIMATION_FALL_DUST = 9,
    EFFECT_SECONDARY_ANIMATION_REFLECT = 10,
    EFFECT_SECONDARY_ANIMATION_ZODIAC_POOF = 11,
    EFFECT_SECONDARY_ANIMATION_DEATH_SMOKE = 12,
    EFFECT_SECONDARY_ANIMATION_LEVEL_UP = 13,
    EFFECT_SECONDARY_ANIMATION_TRAP = 14,
    EFFECT_SECONDARY_ANIMATION_SPLASH = 15,
    EFFECT_SECONDARY_ANIMATION_THROWN_ITEM = 16, /* Item and Throw: the item's graphic or the shuriken model */
    EFFECT_SECONDARY_ANIMATION_RED_ORBS = 17,    /* the Charge abilities and Accumulate */
    EFFECT_SECONDARY_ANIMATION_EQUIP_BREAK = 18, /* abilities 0x8a-0x91 */
    EFFECT_SECONDARY_ANIMATION_TELEPORT = 19,
    EFFECT_SECONDARY_ANIMATION_GOLEM = 20,
    /* the null handler */
    EFFECT_SECONDARY_ANIMATION_ENDING = 21
} effect_secondary_animation_id_e;

/* The secondary animation parameter that makes the projectile handler draw the thrown item's own
 * graphic instead of a model. */
enum { EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC = -1 };

/* elements */
/* What effect_get_element_index makes of an element mask: the position of its first element in
 * g_effect_element_masks, 0 for no element. It picks the CLUT of the weapon hit and
 * spell charge particles and the effect of a thrown ball. */
typedef enum effect_element_index {
    EFFECT_ELEMENT_INDEX_NONE = 0,
    EFFECT_ELEMENT_INDEX_FIRE = 1,
    EFFECT_ELEMENT_INDEX_WATER = 2,
    EFFECT_ELEMENT_INDEX_EARTH = 3,
    EFFECT_ELEMENT_INDEX_WIND = 4,
    EFFECT_ELEMENT_INDEX_HOLY = 5,
    EFFECT_ELEMENT_INDEX_DARK = 6,
    EFFECT_ELEMENT_INDEX_ICE = 7,
    EFFECT_ELEMENT_INDEX_LIGHTNING = 8,
    EFFECT_ELEMENT_INDEX_COUNT = 9
} effect_element_index_e;

s32 effect_get_element_index(s32 elements);

/* particle groups */
/* Index into g_effect_secondary_particle_configs: the particle groups of the secondary effect
 * handlers, named after the handler that spawns each. */
typedef enum effect_particle_group {
    /* effect_secondary_weapon_hit_handler: its first slot set */
    EFFECT_PARTICLE_GROUP_WEAPON_HIT_A = 0,
    EFFECT_PARTICLE_GROUP_WEAPON_HIT_B = 1, /* its second slot set */
    EFFECT_PARTICLE_GROUP_ZODIAC_POOF = 3,
    EFFECT_PARTICLE_GROUP_DEATH_POOF = 4,
    EFFECT_PARTICLE_GROUP_LEVEL_UP = 5,
    EFFECT_PARTICLE_GROUP_FALL_DUST = 6,
    EFFECT_PARTICLE_GROUP_SPLASH = 7,
    EFFECT_PARTICLE_GROUP_TRAP = 8,
    EFFECT_PARTICLE_GROUP_WEAPON_HIT_B_MISSED = 9, /* the second slot set of a missed hit */
    EFFECT_PARTICLE_GROUP_RED_ORBS = 10,
    EFFECT_PARTICLE_GROUP_EQUIP_BREAK = 11,
    EFFECT_PARTICLE_GROUP_SPELL_CHARGE = 12,
    EFFECT_PARTICLE_GROUP_SPELL_CHARGE_13 = 13, /* given the height of group 12; nothing spawns it */
    EFFECT_PARTICLE_GROUP_SUMMON_CHARGE = 14,
    EFFECT_PARTICLE_GROUP_TELEPORT = 15,
    EFFECT_PARTICLE_GROUP_GOLEM = 16
} effect_particle_group_e;

/* One particle group of the secondary effects: a 0x2e-byte record of
 * g_effect_secondary_particle_configs. Its leading bytes schedule the
 * group's effect slots (effect_secondary_start_group); the rest is a fixed
 * emitter, from which effect_slot_spawn_motion draws each slot's
 * motion as the effect file emitter does a particle's. */
typedef struct effect_emitter_values {
    u8 animation_id;        /* 0x00; secondary groups: index into g_effect_secondary_particle_animation_data */
    u8 _unused_01;          /* 0x01 */
    u8 spawn_start_frame;   /* 0x02; secondary groups */
    u8 spawn_end_frame;     /* 0x03; secondary groups */
    u8 count;               /* 0x04; secondary groups: slots the group owns */
    u8 _unused_05;          /* 0x05 */
    u16 anchor_flags;       /* 0x06; effect_emitter_origin_e: CASTER and TARGET are honored */
    u16 motion_flags;       /* 0x08; effect_emitter_velocity_e */
    vec3s16_t position;     /* 0x0a */
    vec3s16_t spread;       /* 0x10 */
    vec3s16_t angle;        /* 0x16 */
    vec3s16_t angle_spread; /* 0x1c */
    ranges16_t weight;      /* 0x22 */
    s16 speed_min;          /* 0x26; Q9 */
    s16 speed_max;          /* 0x28; Q9 */
    u8 spawns_per_frame;    /* 0x2a; secondary groups: slots one frame may start */
    u8 _unused_2b;          /* 0x2b */
    s8 lifetime_min;        /* 0x2c */
    s8 lifetime_max;        /* 0x2d */
} effect_emitter_values_t;
typedef char effect_emitter_values_size_must_be_46[(sizeof(effect_emitter_values_t) == 46) ? 1 : -1];

/* The inertia threshold every secondary effect handler runs its slots under, with a gravity of
 * (0, ONE, 0); an effect file's script sets its own pair (opcode 27). */
enum { EFFECT_SECONDARY_INERTIA_THRESHOLD = 560 };

s32 effect_slot_spawn_motion(const effect_emitter_values_t* values, effect_motion_t* motion);

/* effect slots */
/* One playback step of the animation script referenced by an effect slot:
 * a hold count and the frame id written to the slot. */
typedef struct effect_anim_step {
    u8 hold;  /* 0x00 */
    u8 frame; /* 0x01 */
} effect_anim_step_t;
typedef char effect_anim_step_size_must_be_2[sizeof(effect_anim_step_t) == 2 ? 1 : -1];

typedef struct effect_anim_script {
    s16 count;                  /* 0x00 */
    effect_anim_step_t step[0]; /* 0x02; count steps follow */
} effect_anim_script_t;
typedef char effect_anim_script_size_must_be_2[sizeof(effect_anim_script_t) == 2 ? 1 : -1];

/* 0xcc-byte effect slots of g_effect_slots, chained through the leading next/prev
 * byte pair; slot 0 is the null sentinel. g_effect_slot_active_head heads the active chain
 * and g_effect_slot_free_head the free chain. RGB and the frame byte
 * are copied together by the billboard renderer; effect_slot_init_secondary_animation resets
 * the color and writes the 16-bit values at 0x88 and 0x8e. */
typedef struct effect_slot {
    u8 next;                            /* 0x00 */
    u8 prev;                            /* 0x01 */
    u8 _padding_02[2];                  /* aligns modes */
    DR_MODE modes[2][2];                /* 0x04; two draw modes per buffer */
    POLY_FT4 quads[2];                  /* 0x34; one quad per buffer */
    rgb24_t color;                      /* 0x84 */
    u8 frame;                           /* 0x87 */
    u16 clut;                           /* 0x88 */
    s16 step_index;                     /* 0x8a */
    s16 hold;                           /* 0x8c */
    s16 angle;                          /* 0x8e; billboard quad rotation */
    const effect_anim_script_t* script; /* 0x90 */
    s16 life;                           /* 0x94: negative runs until the script wraps */
    u8 _padding_96[0x02];               /* aligns motion */
    effect_motion_t motion;             /* 0x98 */
} effect_slot_t;
typedef char effect_slot_size_must_be_204[sizeof(effect_slot_t) == 204 ? 1 : -1];

extern effect_slot_t g_effect_slots[EFFECT_SLOT_RECORD_COUNT];
extern effect_sprite_section_t g_effect_secondary_particle_frame_data;

s32 effect_slot_update(s32 id);

/* handlers */
s32 effect_secondary_none_handler(void);
s32 effect_secondary_arrow_arc_handler(void);
s32 effect_secondary_weapon_hit_handler(void);
s32 effect_secondary_golem_handler(void);
s32 effect_secondary_spell_charge_handler(void);
s32 effect_secondary_null_handler(void);
s32 effect_secondary_red_orbs_handler(void);
s32 effect_secondary_unused_07_handler(void);
s32 effect_secondary_fall_dust_handler(void);
s32 effect_secondary_splash_handler(void);
s32 effect_secondary_unused_0a_handler(void);
s32 effect_secondary_unused_0b_handler(void);
s32 effect_secondary_trap_handler(void);
s32 effect_secondary_level_up_handler(void);
s32 effect_secondary_unused_0e_handler(void);
s32 effect_secondary_death_poof_handler(void);
s32 effect_secondary_unused_10_handler(void);
s32 effect_secondary_zodiac_poof_handler(void);
s32 effect_secondary_reflect_handler(void);
s32 effect_secondary_teleport_handler(void);
s32 effect_secondary_linear_projectile_handler(void);
s32 effect_secondary_equip_break_handler(void);
s32 effect_secondary_summon_charge_handler(void);

/* starting secondary effects */
s32 effect_secondary_init_projectile(s32 animation, const struct effect_context* source);
s32 effect_secondary_init_weapon_hit(s32 elements, const effect_context_t* source, s32 unused_is_bow);
s32 effect_secondary_init_fall_dust(const effect_context_t* source, const VECTOR* position);
void effect_secondary_build_init(struct unit* unit, struct effect_context* effect);
void effect_secondary_start_weapon_hit(struct unit* unit, s32 is_bow);
void effect_secondary_start_projectile(unit_t* unit);
void effect_secondary_set_zodiac_poof(unit_t* unit);
void effect_secondary_set_death_smoke(unit_t* unit);
void effect_secondary_set_level_up(struct unit* unit);
void effect_secondary_set_trap(unit_t* unit);
void effect_secondary_set_splash(struct unit* unit);
void effect_secondary_set_fall_dust(struct unit* unit);
void effect_secondary_set_teleport(struct unit* unit);
s32 effect_secondary_set_teleport_by_runtime_id(u32 runtime_id);

#endif
