#ifndef STATUS_H
#define STATUS_H

/* Status effects: the status IDs and sets, the status data and check sets, changing a unit's statuses and the per-turn
 * upkeep, and the display flags and bubbles a status shows. */

#include "fft/gfx.h"
#include "psx/libgte.h"
#include "psx/types.h"

struct action_data;
struct unit_stats;
struct unit_runtime;

/* status IDs */
enum {
    STATUS_COUNT = 40,
    STATUS_BYTE_COUNT = 5,
    STATUS_TIMED_COUNT = 16 /* Poison through Death Sentence keep a CT byte each */
};

/* Canonical status IDs. The owning byte is id >> 3 and its MSB-first mask is
 * 0x80 >> (id & 7); IDs are not byte masks. Entry 0 has no display name but
 * remains addressable in the target's status tables. */
typedef enum status_id {
    STATUS_ID_UNNAMED_00 = 0,
    STATUS_ID_CRYSTAL = 1,
    STATUS_ID_DEAD = 2,
    STATUS_ID_UNDEAD = 3,
    STATUS_ID_CHARGING = 4,
    STATUS_ID_JUMP = 5, /* "Jumping" in the name text */
    STATUS_ID_DEFENDING = 6,
    STATUS_ID_PERFORMING = 7,
    STATUS_ID_PETRIFY = 8,
    STATUS_ID_INVITE = 9, /* "Invitation" in the name text */
    STATUS_ID_DARKNESS = 10,
    STATUS_ID_CONFUSION = 11,
    STATUS_ID_SILENCE = 12,
    STATUS_ID_BLOOD_SUCK = 13,
    STATUS_ID_UNNAMED_0E = 14, /* no name text and no data inflicts it; it darkens the unit */
    STATUS_ID_TREASURE = 15,
    STATUS_ID_OIL = 16,
    STATUS_ID_FLOAT = 17,
    STATUS_ID_RERAISE = 18,
    STATUS_ID_TRANSPARENT = 19,
    STATUS_ID_BERSERK = 20,
    STATUS_ID_CHICKEN = 21,
    STATUS_ID_FROG = 22,
    STATUS_ID_CRITICAL = 23,
    STATUS_ID_POISON = 24,
    STATUS_ID_REGEN = 25,
    STATUS_ID_PROTECT = 26,
    STATUS_ID_SHELL = 27,
    STATUS_ID_HASTE = 28,
    STATUS_ID_SLOW = 29,
    STATUS_ID_STOP = 30,
    STATUS_ID_WALL = 31,
    STATUS_ID_FAITH = 32,
    STATUS_ID_INNOCENT = 33,
    STATUS_ID_CHARM = 34,
    STATUS_ID_SLEEP = 35,
    STATUS_ID_DONT_MOVE = 36,
    STATUS_ID_DONT_ACT = 37,
    STATUS_ID_REFLECT = 38,
    STATUS_ID_DEATH_SENTENCE = 39
} status_id_e;

#define STATUS_BYTE_INDEX(id)  ((id) >> 3)
#define STATUS_BYTE_MASK(id)   ((u8)(0x80U >> ((id) & 7)))
#define STATUS_PACKED_MASK(id) ((u32)STATUS_BYTE_MASK(id) << (STATUS_BYTE_INDEX(id) * 8))

/* Event infliction helpers number bits LSB-first, unlike stored status IDs. */
#define STATUS_LSB_INDEX(id) (((id) & ~7) | (7 - ((id) & 7)))
#define STATUS_LSB_MASK(id)  ((u8)(1U << ((id) & 7)))

/* The special-status callback table reserves index 0; timed slots begin at Poison. */
#define STATUS_HANDLER_INDEX(id) ((id) + 1)
#define STATUS_CT_INDEX(id)      ((id) - STATUS_ID_POISON)

/* status sets */
/* A unit's innate, immunity and current statuses, STATUS_BYTE_COUNT bytes
 * each. job_data_copy copies the three sets as one 15-byte block, so
 * they stay one aggregate that a whole-block byte copy can span; status readers
 * use the named sets. */
typedef struct status_sets {
    u8 innate[5];
    u8 immunity[5];
    u8 current[5];
} status_sets_t;
typedef char status_sets_size_must_be_15[(sizeof(status_sets_t) == 15) ? 1 : -1];

/* Static innate, immunity and starting-status masks embedded in item and job
 * records. This differs from status_sets_t, whose third set is the
 * mutable current-status mask. */
typedef struct initial_status_sets {
    u8 innate[5];
    u8 immunity[5];
    u8 starting[5];
} initial_status_sets_t;
typedef char initial_status_sets_size_must_be_15[(sizeof(initial_status_sets_t) == 15) ? 1 : -1];

/* status data */
/* One record per status id in g_status_effect_data. */
typedef struct status_effect_data {
    u8 _unused_00[2];
    u8 order;
    u8 ct;
    u8 flags_1;
    u8 flags_2;
    u8 cancels[STATUS_BYTE_COUNT];    /* 0x06; statuses removed when this one is inflicted */
    u8 cant_stack[STATUS_BYTE_COUNT]; /* 0x0b; current statuses that block this one */
} status_effect_data_t;
typedef char status_effect_data_size_must_be_16[(sizeof(status_effect_data_t) == 16) ? 1 : -1];

/* Flags that build the shared status-check sets during main initialization
 * (status_check_set_e). The records also set bits 0x08, 0x10, 0x20 and
 * 0x40 of flags_1 and 0x20 of flags_2, which nothing reads. */
typedef enum status_effect_flags_1 {
    STATUS_EFFECT_FLAG_1_KO = 0x01,
    STATUS_EFFECT_FLAG_1_UNIT_LOST = 0x02,
    STATUS_EFFECT_FLAG_1_CHECK_SET_2 = 0x04,
    STATUS_EFFECT_FLAG_1_FREEZE_CT = 0x80
} status_effect_flags_1_e;

typedef enum status_effect_flags_2 {
    STATUS_EFFECT_FLAG_2_IMMORTAL_IMMUNITY = 0x01,
    STATUS_EFFECT_FLAG_2_FORMATION_IMMUNITY = 0x02,
    STATUS_EFFECT_FLAG_2_CHECK_SET_7 = 0x04,
    STATUS_EFFECT_FLAG_2_SCRIPT_FILTER_EXCLUDED = 0x08,
    STATUS_EFFECT_FLAG_2_MOUNT_REMOVAL = 0x10,
    STATUS_EFFECT_FLAG_2_IGNORE_ATTACKS = 0x20,
    STATUS_EFFECT_FLAG_2_PREVENT_REACTION = 0x80
} status_effect_flags_2_e;

/* Rows of g_status_check_sets. status_check_data_init builds rows 0
 * to 9 from the flag bytes of g_status_effect_data and writes row 10
 * itself. Each enumerator's comment lists the statuses its row holds in this
 * release, then what the row is used for. */
typedef enum status_check_set {
    /* Crystal, Dead, Petrify, Invite, Blood Suck, Treasure. A unit with none of them still fights for its side. */
    STATUS_CHECK_SET_KO = 0,
    /* Crystal, Invite, Treasure. On an importance-3 unit of either team it decides the battle. */
    STATUS_CHECK_SET_UNIT_LOST = 1,
    /* Crystal, Treasure. Built; no code reads it. */
    STATUS_CHECK_SET_UNREAD_2 = 2,
    /* Crystal, Petrify, Treasure, Stop, Sleep. The unit's CT does not advance. */
    STATUS_CHECK_SET_FREEZE_CT = 3,
    /* Crystal, Dead, Jump, Petrify, Confusion, Blood Suck, Treasure, Berserk, Chicken, Frog, Stop, Sleep, Don't Act.
     * No reaction, no learning on hit, no movement-ability benefit. */
    STATUS_CHECK_SET_PREVENT_REACTION = 4,
    /* Crystal, Dead, Undead, Petrify, Invite, Confusion, Blood Suck, Treasure, Chicken, Frog, Charm, Sleep,
     * Don't Act, Death Sentence. Added to the immunities of an immortal unit with a save- or load-formation flag. */
    STATUS_CHECK_SET_IMMORTAL_IMMUNITY = 5,
    /* Crystal, Dead, Undead, Petrify, Invite, Blood Suck, Treasure, Chicken, Frog, Charm, Death Sentence.
     * Added with set 5, and never inflicted on an immortal unit. */
    STATUS_CHECK_SET_FORMATION_IMMUNITY = 6,
    /* Darkness, Confusion, Silence, Transparent, Berserk, Chicken, Frog, Stop, Charm, Sleep, Don't Move, Don't Act.
     * Built; no code reads it. */
    STATUS_CHECK_SET_UNREAD_7 = 7,
    /* Crystal, Dead, Undead, Jump, Petrify, Invite, Treasure, Critical. Script unit filters 3 and 5 leave out a
     * unit with one of them (filter 5 lets Undead through). */
    STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED = 8,
    /* Confusion, Berserk, Poison, Stop, Sleep, Don't Move, Don't Act, Death Sentence. Removed from a mount when its
     * rider ends a move. */
    STATUS_CHECK_SET_MOUNT_REMOVAL = 9,
    /* Crystal, Dead, Petrify, Blood Suck, Treasure, Berserk, Chicken, Frog, Charm; set in code, not from the status
     * data. A chocobo with one of them cannot be mounted. */
    STATUS_CHECK_SET_UNMOUNTABLE = 10,
    STATUS_CHECK_SET_COUNT = 11
} status_check_set_e;

/* Bit flags in status_infliction_data_t.type and the active-ability copy. The
 * target status applicator checks these in descending-bit precedence. */
typedef enum status_infliction_type {
    STATUS_INFLICTION_TYPE_CANCEL = 0x10,
    STATUS_INFLICTION_TYPE_SEPARATE = 0x20,
    STATUS_INFLICTION_TYPE_RANDOM_ONE = 0x40,
    STATUS_INFLICTION_TYPE_ALL_OR_NOTHING = 0x80
} status_infliction_type_e;

/* Six-byte status-infliction records, g_status_infliction_data; byte 0
 * selects how the five-byte status set is applied. */
typedef struct status_infliction_data {
    u8 type; /* status_infliction_type_e bit flags */
    u8 statuses[5];
} status_infliction_data_t;
typedef char status_infliction_data_size_must_be_6[(sizeof(status_infliction_data_t) == 6) ? 1 : -1];

extern const status_effect_data_t g_status_effect_data[STATUS_COUNT];
extern status_infliction_data_t g_status_infliction_data[];

/* Built status masks, indexed by status_check_set_e. */
extern u8 g_status_check_sets[STATUS_CHECK_SET_COUNT][STATUS_BYTE_COUNT];

void status_check_data_init(void);
s32 status_unit_set_test(const struct unit_stats* unit, status_check_set_e status_set);

/* status changes */
/* How status_unit_change changes inflicted_status: OR, AND-not or
 * replacement. It then merges status_sets.innate into status_sets.current. */
typedef enum status_change { STATUS_ADD = 0, STATUS_REMOVE = 1, STATUS_REPLACE = 2 } status_change_e;

/* Values accepted by status_action_state_set. */
typedef enum status_action_state {
    STATUS_ACTION_STATE_NONE = 0,
    STATUS_ACTION_STATE_CHARGING = 5,
    STATUS_ACTION_STATE_JUMPING = 6,
    STATUS_ACTION_STATE_DEFENDING = 7,
    /* The Performing status bit (0x01) shifted left 3, which KEEP_PERFORMING relies on. */
    STATUS_ACTION_STATE_PERFORMING = 8,
    STATUS_ACTION_STATE_KEEP_PERFORMING = 255
} status_action_state_e;

/* Packed highest-order action status: a one-based index, or zero for none. */
enum {
    STATUS_ACTION_RESULT_INDEX_MASK = 0x7f,
    STATUS_ACTION_RESULT_REMOVAL = 0x80,
    STATUS_ACTION_RESULT_SCRIPT_FILTER_EXCLUDED = 0x100
};

void status_unit_change(struct unit_stats* unit, s32 status_byte_index, u8 status_flag, s32 change_type);
void status_current_update(struct unit_stats* unit);
s32 status_ct_set(struct unit_stats* unit, s32 status_id, s32 removing);
void status_unit_init(struct unit_stats* unit);
void status_action_state_set(struct unit_stats* unit, u8 action_state);
void status_clear_action_state(struct unit_stats* unit);
s32 status_inflict_defending(s32 unit_id);
s32 action_charge_check_and_cancel(struct unit_stats* unit, s32 cancel);
s32 status_action_priority_find(const struct action_data* action);
s32 status_modify_inflictions(s32 removal_only);
s32 status_resolve_unit_changes(s32 unit_id, s32 removal_only);
void status_apply_unit_action_removal(struct unit_stats* unit);
s32 status_prepare_transparent_removal(struct unit_stats* unit);
void status_remove_transparent_if_jump_used(struct unit_stats* unit);
void status_clear_all(struct unit_stats* unit);

/* per-turn upkeep */
void status_update_expiring(s32 unit_id);
s32 status_prepare_terrain_poison(struct unit_stats* unit);
s32 status_prepare_poison_and_regen(struct unit_stats* unit);

/* status display */
/* Requests for the renderer's pending status flags. */
typedef enum status_display_change {
    STATUS_DISPLAY_CHANGE_REMOVE = 0,
    STATUS_DISPLAY_CHANGE_ADD = 1,
    STATUS_DISPLAY_CHANGE_KEEP = 2 /* leave pending changes untouched */
} status_display_change_e;

s32 status_init_display_flags(s32 battle_id);
s32 status_queue_display_flag_change(s32 handler_index, s32 change_type, s32 battle_id);
void status_queue_display_flag_change_if_executing(s32 status_id, s32 change_type, s32 battle_id);
void status_current_graphics_queue(s32 battle_id, s32 change_type);

/* status bubbles */
/* unit_runtime_t.status_bubble_id: the bubbles the code names. The others follow the statuses of
 * g_status_bubble_status_masks. */
typedef enum status_bubble {
    STATUS_BUBBLE_DEAD = 0, /* the death counter, or the RERAISE image under Reraise */
    STATUS_BUBBLE_CHARGING = 9,
    STATUS_BUBBLE_RERAISE = 10,
    STATUS_BUBBLE_DEATH_SENTENCE = 20, /* its counter */
    STATUS_BUBBLE_SOURCE_UNIT = 21,    /* the source unit's marker, not a status */
    STATUS_BUBBLE_COUNT = 22
} status_bubble_e;

void sprite_status_bubble_active_update(struct unit_runtime* unit);
void sprite_status_bubble_draw(struct unit_runtime* unit, const DVECTOR* unused_screen_coords);

#endif
