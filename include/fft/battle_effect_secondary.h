#ifndef BATTLE_EFFECT_SECONDARY_H
#define BATTLE_EFFECT_SECONDARY_H

/* BATTLE secondary effects: hit sparks, poofs, charge rings and traps. */

#include "fft/battle_effect.h"
#include "fft/battle_unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Sixteen secondary effects may run at once; record zero anchors their chains. */
enum {
    EFFECT_SECONDARY_SLOT_COUNT = 16,
    EFFECT_SECONDARY_RECORD_COUNT = EFFECT_SECONDARY_SLOT_COUNT + 1,
};

/* One running secondary effect: a 0x54-byte record of
 * g_battle_effect_secondary_data (0x801b8b9c), ids 1-16 with record 0 the
 * sentinel. The secondary effects are the ones resident in BATTLE.BIN (weapon
 * hits, projectiles, the charges, the level-up and so on): each is an
 * animation id run by its handler of g_battle_effect_secondary_handlers
 * through the phases of effect_secondary_phase_e. Records chain by id.
 * The particles a handler draws are effect slots (effect_slot_t), a
 * separate pool, whose ids it keeps in slot_ids or in its own state. */
typedef struct effect_secondary_data {
    u8 previous_slot_id; /* 0x00; the record before it on the active chain, or the next free record */
    u8 next_slot_id;     /* 0x01 */
    u8 own_slot_id;      /* 0x02; index of this record in g_battle_effect_secondary_data */
    u8 function_id;      /* 0x03; index of its handler in g_battle_effect_secondary_handlers */
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
    /* 0x2c; live effect slots of its particle group: battle_effect_start_group counts them up and
     * battle_effect_update_active_slots down; the handlers finish at 0. */
    s16 active_count;
    s16 finish_timer;  /* 0x2e; counts frames after the last slot finished */
    u8 slot_ids[0x20]; /* 0x30; its effect slots, 0 for none; cleared when the record is allocated */
    void* allocation;  /* 0x50; the handler's work block; freed when the record is removed */
} effect_secondary_data_t;
typedef char effect_secondary_data_size_must_be_84[(sizeof(effect_secondary_data_t) == 84) ? 1 : -1];

/* The secondary animation parameter that makes the projectile handler draw the thrown item's own
 * graphic instead of a model. */
enum { EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC = -1 };

/* Four-byte records at 0x801b84dc indexed by animation id
 * (effect_secondary_animation_id_e); byte 0 selects the
 * g_battle_effect_secondary_handlers handler. */
typedef struct effect_secondary_animation {
    u8 function_id; /* 0x00 */
    u8 _padding_01; /* 0x01; aligns parameter */
    /* 0x02; the projectile handlers: effect_model_e in the low byte, or
     * EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC; the charge handler: its parameter row */
    s16 parameter;
} effect_secondary_animation_t;

/* The inertia threshold every secondary effect handler runs its slots under, with a gravity of
 * (0, ONE, 0); an effect file's script sets its own pair (opcode 27). */
enum { EFFECT_SECONDARY_INERTIA_THRESHOLD = 560 };

/* One 14-byte parameter row for the spell-charge secondary effect. */
typedef struct effect_charge_params {
    s16 speed;
    s16 spread_range;
    s16 spread_offset;
    s16 duration;
    s16 radius;
    u8 spawn_interval;
    u8 _unused_0b;
    u8 max_trails;
    u8 fade_row;
} effect_charge_params_t;
typedef char effect_charge_params_size_must_be_14[sizeof(effect_charge_params_t) == 14 ? 1 : -1];

/* Work area at 0x801bade0 shared by the secondary effect handlers; the
 * spell-charge, summon-charge and zodiac-poof handlers each lay it out as
 * their own state. */
typedef union effect_secondary_state {
    effect_charge_state_t charge;
    effect_ring_state_t summon_ring;
    effect_trap_state_24_t zodiac_poof;
} effect_secondary_state_t;

/* Index into g_battle_effect_secondary_animations: the secondary effects resident in BATTLE.BIN. Those
 * the code does not start by number come from two tables: the weapon type's projectile
 * (g_battle_effect_weapon_projectile_animations) and the ability's charge animation set
 * (g_battle_effect_secondary_by_charge_animation). */
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
    EFFECT_SECONDARY_ANIMATION_VENOM_TRAP = 14,
    EFFECT_SECONDARY_ANIMATION_SPLASH = 15,
    EFFECT_SECONDARY_ANIMATION_THROWN_ITEM = 16, /* Item and Throw: the item's graphic or the shuriken model */
    EFFECT_SECONDARY_ANIMATION_RED_ORBS = 17,    /* the Charge abilities and Accumulate */
    EFFECT_SECONDARY_ANIMATION_EQUIP_BREAK = 18, /* abilities 0x8a-0x91 */
    EFFECT_SECONDARY_ANIMATION_TELEPORT = 19,
    EFFECT_SECONDARY_ANIMATION_GOLEM = 20,
    /* the null handler; g_battle_effect_ending_secondary_function_id is this record's function byte */
    EFFECT_SECONDARY_ANIMATION_ENDING = 21,
} effect_secondary_animation_id_e;

/* secondary effects */
extern u16 g_battle_effect_secondary_active_tail;
extern s32 g_battle_effect_buffer_index;
extern DVECTOR g_battle_effect_charge_line_direction_offsets[];
extern u8 g_battle_effect_charge_line_fade_levels[];
extern effect_charge_params_t g_battle_effect_charge_line_params[2];
extern struct effect_secondary_data* g_battle_effect_current_secondary;
extern u32 g_battle_effect_current_slot_id;
extern effect_death_poof_state_t g_battle_effect_death_poof_states[];
extern u8 g_battle_effect_ending_secondary_function_id; /* function byte to store; 5 = nothing */
extern u16 g_battle_effect_secondary_free_head;
extern u32* g_battle_effect_polygon_depth_data;
extern effect_secondary_animation_t g_battle_effect_secondary_animations[];
/* Secondary-animation id per charge animation set, indexed by the low 16 bits of the
 * caller's packed argument. */
extern u8 g_battle_effect_secondary_by_charge_animation[];
extern effect_secondary_data_t g_battle_effect_secondary_data[];
extern effect_secondary_callback_t g_battle_effect_secondary_handlers[23];
extern effect_secondary_state_t g_battle_effect_secondary_state;
/* Brightness (out of 128) of each of a summon-charge ring's ten slots, newest point first. */
extern u8 g_battle_effect_summon_ring_brightness[];
extern u8 g_battle_effect_charge_secondary_id; /* id of the charge secondary effect started with the ability; 0 none */
/* Per-group trap emitter records (0x801b8564, stride 0x2e). The secondary-effect
 * handlers spawn while spawn_start_frame <= timer < spawn_end_frame and finish
 * once timer passes spawn_start_frame with no live slots;
 * battle_effect_update_active_slots walks count active slots. The level-up
 * handler seeds group 5's position.y with -8, then lowers it by 3 per frame. */
extern effect_emitter_values_t g_battle_effect_trap_particle_configs[17];
/* Projectile secondary animation of each weapon item type: BOW_ARROW for bows, CROSSBOW_BOLT for
 * crossbows, NONE for the rest. */
extern u8 g_battle_effect_weapon_projectile_animations[];
/* CLUT of the zodiac poof's particles, an index in the weapons row, for each
 * sprite palette (the secondary effect's parameter). */
extern u8 g_battle_effect_zodiac_poof_animations[];
s32 battle_effect_unused_07_secondary_handler(void);
s32 battle_effect_unused_0a_secondary_handler(void);
s32 battle_effect_unused_0b_secondary_handler(void);
s32 battle_effect_unused_0e_secondary_handler(void);
s32 battle_effect_unused_10_secondary_handler(void);
s32 battle_effect_none_secondary_handler(void);
s32 battle_effect_arrow_arc_secondary_handler(void);
s32 battle_effect_equip_break_secondary_handler(void);
s32 battle_effect_fall_dust_secondary_handler(void);
void battle_effect_finalize_secondary(u8 secondary_id);
effect_secondary_phase_e battle_effect_get_secondary_phase(u8 secondary_id);
s32 battle_effect_golem_secondary_handler(void);
s32 battle_effect_init_fall_dust_secondary(effect_secondary_init_t* source, VECTOR* position);
s32 battle_effect_init_projectile_secondary(s32 animation, struct effect_secondary_init* source);
s32 battle_effect_init_secondary(s32 animation_id, s32 parameter, struct effect_secondary_init* source);
s32 battle_effect_init_weapon_hit_secondary(s32 elements, effect_secondary_init_t* source, s32 unused_is_bow);
s32 battle_effect_level_up_secondary_handler(void);
s32 battle_effect_linear_projectile_secondary_handler(void);
s32 battle_effect_null_secondary_handler(void);
s32 battle_effect_red_orbs_secondary_handler(void);
s32 battle_effect_reflect_secondary_handler(void);
void battle_effect_start_weapon_hit_secondary(struct battle_unit* unit, s32 is_bow);
void battle_effect_set_secondary_death_smoke(battle_unit_t* unit);
void battle_effect_set_secondary_fall_dust(struct battle_unit* unit);
void battle_effect_set_secondary_level_up(struct battle_unit* unit);
void battle_effect_set_secondary_splash(struct battle_unit* unit);
void battle_effect_set_secondary_teleport(struct battle_unit* unit);
s32 battle_effect_set_secondary_teleport_by_runtime_id(u32 runtime_id);
void battle_effect_set_secondary_venom_trap(battle_unit_t* unit);
void battle_effect_set_secondary_zodiac_poof(battle_unit_t* unit);
s32 battle_effect_spell_charge_secondary_handler(void);
s32 battle_effect_splash_secondary_handler(void);
void battle_effect_start_action_projectile_secondary(battle_unit_t* unit);
s32 battle_effect_summon_charge_secondary_handler(void);
s32 battle_effect_teleport_secondary_handler(void);
s32 battle_effect_update_death_poof_secondary(s32 unused_arg0, s32 passthrough);
s32 battle_effect_update_secondary_effects(void);
s32 battle_effect_venom_trap_secondary_handler(void);
s32 battle_effect_weapon_hit_secondary_handler(void);
s32 battle_effect_zodiac_poof_secondary_handler(void);

#endif
