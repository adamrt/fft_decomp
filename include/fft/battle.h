#ifndef FFT_BATTLE_H
#define FFT_BATTLE_H

#include "fft/ability_flags.h"
#include "fft/battle_ability.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_runtime.h"
#include "fft/battle_state.h"
#include "fft/battle_text.h"
#include "fft/battle_unit_position.h"
#include "fft/character_identity.h"
#include "fft/data.h"
#include "fft/map.h"
#include "fft/menu_types.h"
#include "fft/status.h"
#include "fft/thread.h"
#include "fft/unit_equipment.h"
#include "fft/unit_flags.h"
#include "fft/unit_slots.h"
#include "fft/unit_stats.h"
#include "fft/zodiac.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Source frame consumed at 0x80083e10: a 2-byte header followed by 4-byte
 * parts. Signed byte shifts precede the packed tile, dimension and flip bits.
 * The renderer's display record supplies the part count. */
typedef struct battle_gfx_source_part {
    s8 x_shift;
    s8 y_shift;
    u16 attributes;
} battle_gfx_source_part_t;

typedef struct battle_gfx_source_frame {
    /* Bits 0-2 are the part count minus one, bits 3-7 index the Y rotation
     * table g_battle_gfx_sprite_y_rotations (0x80094508). */
    u8 part_count_and_rotation;
    u8 flags;
    battle_gfx_source_part_t parts[1];
} battle_gfx_source_frame_t;

enum { BATTLE_EFFECT_FLAGS_0006 = 0x0006 };

/* 0x8013b6e4 initializes independent nine-tile strips at +0x1c and +0xac
 * in the same 0x13c-byte storage used by world_fade_tile_frame_t. */
typedef struct battle_scanline_frame {
    DR_MODE draw_mode; /* 0x00 */
    TILE center;       /* 0x0c */
    TILE backdrop[9];  /* 0x1c */
    TILE bar[9];       /* 0xac */
} battle_scanline_frame_t;

typedef char battle_scanline_frame_size_must_be_0x13c[(sizeof(battle_scanline_frame_t) == 0x13c) ? 1 : -1];

typedef enum battle_action_type {
    BATTLE_ACTION_TYPE_PSEUDO_STATUS = 0x01,
    BATTLE_ACTION_TYPE_STATUS_CHANGE = 0x08,
    BATTLE_ACTION_TYPE_MP_HEALING = 0x10,
    BATTLE_ACTION_TYPE_MP_DAMAGE = 0x20,
    BATTLE_ACTION_TYPE_HP_HEALING = 0x40,
    BATTLE_ACTION_TYPE_HP_DAMAGE = 0x80,
} battle_action_type_e;

typedef enum battle_action_miss_type {
    BATTLE_ACTION_MISS_TYPE_HIT = 0x00,
    BATTLE_ACTION_MISS_TYPE_ACCESSORY_EVADE = 0x01,
    BATTLE_ACTION_MISS_TYPE_RIGHT_HAND_EVADE = 0x02,
    BATTLE_ACTION_MISS_TYPE_LEFT_HAND_EVADE = 0x03,
    BATTLE_ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD = 0x04,
    BATTLE_ACTION_MISS_TYPE_NULLIFIED = 0x05,
    BATTLE_ACTION_MISS_TYPE_ACCURACY_MISS = 0x06,
    BATTLE_ACTION_MISS_TYPE_FORCED_FAILURE = 0x07,
    BATTLE_ACTION_MISS_TYPE_CANCELLED = 0x08,
    BATTLE_ACTION_MISS_TYPE_REFLECTED = 0x09,
    BATTLE_ACTION_MISS_TYPE_GOLEM_GUARD = 0x0a,
    BATTLE_ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD = 0x0b,
    BATTLE_ACTION_MISS_TYPE_BUGGED_REFLECTION = 0x0c,
    BATTLE_ACTION_MISS_TYPE_CATCH = 0x0d,
} battle_action_miss_type_e;

typedef enum battle_trap_id {
    BATTLE_TRAP_ID_DEGENERATOR = 0,
    BATTLE_TRAP_ID_DEATHTRAP = 1,
    BATTLE_TRAP_ID_SLEEPING_GAS = 2,
    BATTLE_TRAP_ID_STEEL_NEEDLE = 3,
    BATTLE_TRAP_ID_NONE = 0xff,
} battle_trap_id_e;

typedef enum battle_move_find_result_flags {
    BATTLE_MOVE_FIND_RESULT_ITEM = 0x01,
    BATTLE_MOVE_FIND_RESULT_TRAP = 0x02,
    BATTLE_MOVE_FIND_RESULT_OCCUPIED = 0x04,
} battle_move_find_result_flags_e;

typedef struct battle_move_find_result_data {
    u8 flags; /* battle_move_find_result_flags_e */
    u8 entry_index;
    u8 rare_item_id;
    u8 common_item_id;
    u8 trap_id; /* battle_trap_id_e */
    s8 occupying_unit_id;
} battle_move_find_result_data_t;

typedef char battle_move_find_result_data_size_must_be_6[(sizeof(battle_move_find_result_data_t) == 6) ? 1 : -1];

typedef enum battle_action_special_effect {
    BATTLE_ACTION_SPECIAL_EFFECT_SET_GOLEM = 0x0001,
    BATTLE_ACTION_SPECIAL_EFFECT_MORBOL = 0x0002,
    BATTLE_ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT = 0x0004,
    BATTLE_ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN = 0x0008,
    BATTLE_ACTION_SPECIAL_EFFECT_STEAL_ITEM = 0x0010,
    BATTLE_ACTION_SPECIAL_EFFECT_POACH = 0x0020,
    BATTLE_ACTION_SPECIAL_EFFECT_TEAM_CHANGE = 0x0040,
    BATTLE_ACTION_SPECIAL_EFFECT_LEVEL_UP = 0x0080,
    BATTLE_ACTION_SPECIAL_EFFECT_LEVEL_DOWN = 0x0100,
    BATTLE_ACTION_SPECIAL_EFFECT_NULLIFIED = 0x0200,
    BATTLE_ACTION_SPECIAL_EFFECT_ELEMENTAL_ABSORPTION = 0x0400,
    BATTLE_ACTION_SPECIAL_EFFECT_ELEMENTAL_WEAKNESS = 0x0800,
    BATTLE_ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN = 0x1000,
    BATTLE_ACTION_SPECIAL_EFFECT_KNOCKBACK = 0x4000,
    BATTLE_ACTION_SPECIAL_EFFECT_GOLEM_GUARD = 0x8000,
} battle_action_special_effect_e;

typedef enum battle_action_context {
    BATTLE_ACTION_CONTEXT_PRIMARY = 0,
    BATTLE_ACTION_CONTEXT_REACTION_OR_SIMULATION = 1,
} battle_action_context_e;

typedef enum battle_action_state {
    BATTLE_ACTION_STATE_EXECUTE = 0,
    BATTLE_ACTION_STATE_AI_SIMULATION = 1,
    BATTLE_ACTION_STATE_PREVIEW = 2,
} battle_action_state_e;

typedef enum battle_action_target_kind {
    BATTLE_ACTION_TARGET_TILE = 5,
    BATTLE_ACTION_TARGET_UNIT = 6,
} battle_action_target_kind_e;

typedef enum battle_turn_status_flags {
    BATTLE_TURN_STATUS_CT_FROZEN = 0x01,
    BATTLE_TURN_STATUS_INCAPACITATED = 0x02,
    BATTLE_TURN_STATUS_DEAD = 0x04,
    BATTLE_TURN_STATUS_SLEEP = 0x08,
    BATTLE_TURN_STATUS_HASTE = 0x10,
    BATTLE_TURN_STATUS_SLOW = 0x20,
    BATTLE_TURN_STATUS_BLOCKS_WAIT_MENU_MASK = 0x0f,
    BATTLE_TURN_STATUS_BLOCKS_TURN_MASK = 0x0d,
} battle_turn_status_flags_e;

typedef enum battle_turn_event {
    BATTLE_TURN_EVENT_UNIT_READY = 0x0100,
    BATTLE_TURN_EVENT_ABILITY_READY = 0x0200,
    BATTLE_TURN_EVENT_ACTION_RESULT = 0x0300,
    BATTLE_TURN_EVENT_UNKNOWN_0400 = 0x0400,
    BATTLE_TURN_EVENT_MIME = 0x0500,
    BATTLE_TURN_EVENT_NONE = 0xff00,
} battle_turn_event_e;

typedef enum battle_unit_equipment_stat_index {
    BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER = 0,
    BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER = 1,
    BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE = 2,
    BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE = 3,
    BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE = 4,
    BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE = 5,
    BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE = 6,
    BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE = 7,
    BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE = 8,
    BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE = 9,
    BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE = 10,
    BATTLE_UNIT_EQUIPMENT_STAT_COUNT = 11,
} battle_unit_equipment_stat_index_e;

/* Pending post-action displays are queued as one little-endian word in Misc
 * Unit Data 0x1b8..0x1bb.  The two dispatchers at 0x8007f5f8 and 0x800808b8
 * consume one bit at a time in ascending order. */
typedef enum battle_action_display_flag {
    BATTLE_ACTION_DISPLAY_FLAG_HP_DAMAGE = 0x00000001,
    BATTLE_ACTION_DISPLAY_FLAG_HP_HEALING = 0x00000002,
    BATTLE_ACTION_DISPLAY_FLAG_MP_DAMAGE = 0x00000004,
    BATTLE_ACTION_DISPLAY_FLAG_MP_HEALING = 0x00000008,
    BATTLE_ACTION_DISPLAY_FLAG_SPEED_DAMAGE = 0x00000010,
    BATTLE_ACTION_DISPLAY_FLAG_SPEED_GAIN = 0x00000020,
    BATTLE_ACTION_DISPLAY_FLAG_CT_DAMAGE = 0x00000040,
    BATTLE_ACTION_DISPLAY_FLAG_CT_GAIN = 0x00000080,
    BATTLE_ACTION_DISPLAY_FLAG_BRAVE_DAMAGE = 0x00000100,
    BATTLE_ACTION_DISPLAY_FLAG_BRAVE_GAIN = 0x00000200,
    BATTLE_ACTION_DISPLAY_FLAG_FAITH_DAMAGE = 0x00000400,
    BATTLE_ACTION_DISPLAY_FLAG_FAITH_GAIN = 0x00000800,
    BATTLE_ACTION_DISPLAY_FLAG_PA_DAMAGE = 0x00001000,
    BATTLE_ACTION_DISPLAY_FLAG_PA_GAIN = 0x00002000,
    BATTLE_ACTION_DISPLAY_FLAG_MA_DAMAGE = 0x00004000,
    BATTLE_ACTION_DISPLAY_FLAG_MA_GAIN = 0x00008000,
    BATTLE_ACTION_DISPLAY_FLAG_LEVEL_DOWN = 0x00010000,
    BATTLE_ACTION_DISPLAY_FLAG_LEVEL_UP = 0x00020000,
    BATTLE_ACTION_DISPLAY_FLAG_STOLEN = 0x00040000,
    BATTLE_ACTION_DISPLAY_FLAG_BROKEN = 0x00080000,
    BATTLE_ACTION_DISPLAY_FLAG_QUICK = 0x00100000,
    BATTLE_ACTION_DISPLAY_FLAG_CT_ZERO = 0x00200000,
    BATTLE_ACTION_DISPLAY_FLAG_MISSED = 0x00400000,
    BATTLE_ACTION_DISPLAY_FLAG_GUARDED = 0x00800000,
    BATTLE_ACTION_DISPLAY_FLAG_CAUGHT = 0x01000000,
    BATTLE_ACTION_DISPLAY_FLAG_GAINED_EXP = 0x02000000,
    BATTLE_ACTION_DISPLAY_FLAG_GAINED_JP = 0x04000000,
    BATTLE_ACTION_DISPLAY_FLAG_NO_TARGET = 0x08000000,
    BATTLE_ACTION_DISPLAY_FLAG_SILENCED = 0x10000000,
    BATTLE_ACTION_DISPLAY_FLAG_NO_MP = 0x20000000,
} battle_action_display_flag_e;

typedef union battle_action_display_flags {
    u32 word;
    u8 bytes[4];
} battle_action_display_flags_t;

typedef char battle_action_display_flags_size_must_be_4[(sizeof(battle_action_display_flags_t) == 4) ? 1 : -1];

/* The low 9 bits select a result graphic; bit 9 alters its motion for a
 * critical hit.  Values below 0x10 are also indices into the numeric-value
 * table at 0x800962d4. */
typedef enum battle_numeric_display_selector {
    BATTLE_NUMERIC_DISPLAY_HP_DAMAGE = 0x000,
    BATTLE_NUMERIC_DISPLAY_HP_HEALING = 0x001,
    BATTLE_NUMERIC_DISPLAY_MP_DAMAGE = 0x002,
    BATTLE_NUMERIC_DISPLAY_MP_HEALING = 0x003,
    BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE = 0x004,
    BATTLE_NUMERIC_DISPLAY_SPEED_GAIN = 0x005,
    BATTLE_NUMERIC_DISPLAY_CT_DAMAGE = 0x006,
    BATTLE_NUMERIC_DISPLAY_CT_GAIN = 0x007,
    BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE = 0x008,
    BATTLE_NUMERIC_DISPLAY_BRAVE_GAIN = 0x009,
    BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE = 0x00a,
    BATTLE_NUMERIC_DISPLAY_FAITH_GAIN = 0x00b,
    BATTLE_NUMERIC_DISPLAY_PA_DAMAGE = 0x00c,
    BATTLE_NUMERIC_DISPLAY_PA_GAIN = 0x00d,
    BATTLE_NUMERIC_DISPLAY_MA_DAMAGE = 0x00e,
    BATTLE_NUMERIC_DISPLAY_MA_GAIN = 0x00f,
    BATTLE_NUMERIC_DISPLAY_MISSED = 0x010,
    BATTLE_NUMERIC_DISPLAY_GUARDED = 0x020,
    BATTLE_NUMERIC_DISPLAY_CAUGHT = 0x030,
    BATTLE_NUMERIC_DISPLAY_GAINED_EXP = 0x040,
    BATTLE_NUMERIC_DISPLAY_GAINED_JP = 0x050,
    BATTLE_NUMERIC_DISPLAY_STATUS_INFLICTED = 0x060,
    BATTLE_NUMERIC_DISPLAY_STATUS_REMOVED = 0x070,
    BATTLE_NUMERIC_DISPLAY_LEVEL_DOWN = 0x080,
    BATTLE_NUMERIC_DISPLAY_LEVEL_UP = 0x090,
    BATTLE_NUMERIC_DISPLAY_STOLEN = 0x0a0,
    BATTLE_NUMERIC_DISPLAY_BROKEN = 0x0b0,
    BATTLE_NUMERIC_DISPLAY_QUICK = 0x0c0,
    BATTLE_NUMERIC_DISPLAY_CT_ZERO = 0x0d0,
    BATTLE_NUMERIC_DISPLAY_NO_TARGET = 0x0e0,
    BATTLE_NUMERIC_DISPLAY_SILENCED = 0x0f0,
    BATTLE_NUMERIC_DISPLAY_NO_MP = 0x100,
    BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK = 0x1ff,
    BATTLE_NUMERIC_DISPLAY_MODIFIER_CRITICAL = 0x200,
} battle_numeric_display_selector_e;

enum {
    BATTLE_ACTION_STATUS_DISPLAY_REMOVAL = 0x80,
    BATTLE_ACTION_STATUS_DISPLAY_IMAGE_MASK = 0x7f,
    BATTLE_ACTION_STATUS_DISPLAY_CAPACITY = 0x1b,
};

/* Image IDs used by the projected-action display. These index the 49-entry
 * texture-source table at 0x8014cf68; they are not battle status IDs. */
typedef enum battle_action_display_image_id {
    BATTLE_ACTION_DISPLAY_IMAGE_DEAD = 0x00,
    BATTLE_ACTION_DISPLAY_IMAGE_UNDEAD = 0x01,
    BATTLE_ACTION_DISPLAY_IMAGE_PETRIFY = 0x02,
    BATTLE_ACTION_DISPLAY_IMAGE_INVITE = 0x03,
    BATTLE_ACTION_DISPLAY_IMAGE_DARKNESS = 0x04,
    BATTLE_ACTION_DISPLAY_IMAGE_CONFUSION = 0x05,
    BATTLE_ACTION_DISPLAY_IMAGE_SILENCE = 0x06,
    BATTLE_ACTION_DISPLAY_IMAGE_BLOOD_SUCK = 0x07,
    BATTLE_ACTION_DISPLAY_IMAGE_OIL = 0x08,
    BATTLE_ACTION_DISPLAY_IMAGE_FLOAT = 0x09,
    BATTLE_ACTION_DISPLAY_IMAGE_RERAISE = 0x0a,
    BATTLE_ACTION_DISPLAY_IMAGE_TRANSPARENT = 0x0b,
    BATTLE_ACTION_DISPLAY_IMAGE_BERSERK = 0x0c,
    BATTLE_ACTION_DISPLAY_IMAGE_POISON = 0x0d,
    BATTLE_ACTION_DISPLAY_IMAGE_REGEN = 0x0e,
    BATTLE_ACTION_DISPLAY_IMAGE_PROTECT = 0x0f,
    BATTLE_ACTION_DISPLAY_IMAGE_SHELL = 0x10,
    BATTLE_ACTION_DISPLAY_IMAGE_HASTE = 0x11,
    BATTLE_ACTION_DISPLAY_IMAGE_SLOW = 0x12,
    BATTLE_ACTION_DISPLAY_IMAGE_STOP = 0x13,
    BATTLE_ACTION_DISPLAY_IMAGE_FAITH = 0x14,
    BATTLE_ACTION_DISPLAY_IMAGE_INNOCENT = 0x15,
    BATTLE_ACTION_DISPLAY_IMAGE_CHARM = 0x16,
    BATTLE_ACTION_DISPLAY_IMAGE_SLEEP = 0x17,
    BATTLE_ACTION_DISPLAY_IMAGE_DONT_MOVE = 0x18,
    BATTLE_ACTION_DISPLAY_IMAGE_DONT_ACT = 0x19,
    BATTLE_ACTION_DISPLAY_IMAGE_REFLECT = 0x1a,
    BATTLE_ACTION_DISPLAY_IMAGE_DEATH_SENTENCE = 0x1b,
    BATTLE_ACTION_DISPLAY_IMAGE_STOLEN = 0x1c,
    BATTLE_ACTION_DISPLAY_IMAGE_BROKEN = 0x1d,
    BATTLE_ACTION_DISPLAY_IMAGE_QUICK = 0x24,
    BATTLE_ACTION_DISPLAY_IMAGE_HP = 0x25,
    BATTLE_ACTION_DISPLAY_IMAGE_MP = 0x26,
    BATTLE_ACTION_DISPLAY_IMAGE_CT = 0x27,
    BATTLE_ACTION_DISPLAY_IMAGE_SPEED = 0x28,
    BATTLE_ACTION_DISPLAY_IMAGE_BRAVE = 0x29,
    BATTLE_ACTION_DISPLAY_IMAGE_FAITH_STAT = 0x2a,
    BATTLE_ACTION_DISPLAY_IMAGE_PHYSICAL_ATTACK = 0x2b,
    BATTLE_ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK = 0x2c,
    BATTLE_ACTION_DISPLAY_IMAGE_LEVEL = 0x2d,
    BATTLE_ACTION_DISPLAY_IMAGE_GIL = 0x2e,
    BATTLE_ACTION_DISPLAY_IMAGE_EXP = 0x2f,
    BATTLE_ACTION_DISPLAY_IMAGE_FROG = 0x30,
    BATTLE_ACTION_DISPLAY_IMAGE_NONE = 0xff,
} battle_action_display_image_id_e;

enum { BATTLE_ACTION_DISPLAY_IMAGE_COUNT = BATTLE_ACTION_DISPLAY_IMAGE_FROG + 1 };

typedef struct battle_action_display_image_rect {
    u8 u;
    u8 v;
    u8 width;
    u8 height;
} battle_action_display_image_rect_t;

typedef char
    battle_action_display_image_rect_size_must_be_4[(sizeof(battle_action_display_image_rect_t) == 4) ? 1 : -1];

/* Battle Stats 0x05 and Miscellaneous Unit Data 0x13d share this byte. */
typedef enum battle_team_flags {
    BATTLE_TEAM_MASK = 0x30,
    BATTLE_TEAM_OR_PLAYER_CONTROL_MASK = 0x38,
    BATTLE_TEAM_BLUE = 0x00,
    BATTLE_TEAM_RED = 0x10,
    BATTLE_TEAM_GREEN = 0x20,
    BATTLE_TEAM_LIGHT_BLUE = 0x30,
    BATTLE_TEAM_FLAG_IMMORTAL = 0x04,
    BATTLE_TEAM_FLAG_PLAYER_CONTROLLED = 0x08,
} battle_unit_team_flags_e;

/* Miscellaneous Unit Data 0x130 identifies which role 0x131 links. */
typedef enum battle_misc_mount_state {
    BATTLE_MISC_MOUNT_STATE_NONE = 0,
    BATTLE_MISC_MOUNT_STATE_RIDER = 1,
    BATTLE_MISC_MOUNT_STATE_MOUNT = 2,
} battle_misc_mount_state_e;

/* Battle Stats 0x182 links the two simulation records in a mounted pair. */
typedef enum battle_mount_info_flags {
    BATTLE_MOUNT_INFO_PARTNER_ID_MASK = 0x1f,
    BATTLE_MOUNT_INFO_FLAG_MOUNT = 0x40,
    BATTLE_MOUNT_INFO_FLAG_RIDER = 0x80,
    BATTLE_MOUNT_INFO_ROLE_MASK = 0xc0,
} battle_mount_info_flags_e;

/* Lifecycle states stored in Battle Stats 0x183. Zero and 0xff participate in
 * broader absence/sentinel checks and remain deliberately unnamed. */
typedef enum battle_unit_existence_state {
    BATTLE_UNIT_EXISTENCE_ACTIVE = 1,
    BATTLE_UNIT_EXISTENCE_PENDING_REMOVAL = 2,
    BATTLE_UNIT_EXISTENCE_DISABLED = 0x80,
} battle_unit_existence_state_e;

/* Miscellaneous Unit Data 0x13f is XORed with the sprite display flags. */
typedef enum battle_misc_display_flags {
    BATTLE_MISC_DISPLAY_HORIZONTAL_FLIP = 0x02,
    BATTLE_MISC_DISPLAY_VERTICAL_FLIP = 0x04,
} battle_misc_display_flags_e;

typedef enum battle_formation_index {
    BATTLE_FORMATION_INDEX_GENERATED_UNIT = 0xfe,
    BATTLE_FORMATION_INDEX_NONE = 0xff,
} battle_formation_index_e;

typedef enum battle_facing {
    BATTLE_FACING_SOUTH = 0x000,
    BATTLE_FACING_WEST = 0x400,
    BATTLE_FACING_NORTH = 0x800,
    BATTLE_FACING_EAST = 0xc00,
    BATTLE_FACING_QUARTER_TURN = 0x400,
    BATTLE_FACING_MASK = 0x0fff,
} battle_unit_facing_e;

typedef enum battle_ability_facing_modifier {
    BATTLE_ABILITY_FACING_FRONT = 0,
    BATTLE_ABILITY_FACING_SIDE = 1,
    BATTLE_ABILITY_FACING_BACK = 2,
} battle_ability_facing_modifier_e;

/*
 * Script variable 0x23, set by event instruction {3C} Weather.  The same
 * strength scale covers rain and snow; the map's palette-mod flag word says
 * which one is falling.  battle_map_get_effective_weather folds that flag in by
 * adding 3, so callers that compare against it see the snow values.
 * The equivalent GNS scale is fft_weather_e in libfft.
 */
typedef enum battle_weather {
    BATTLE_WEATHER_NONE = 0,
    BATTLE_WEATHER_NONE_ALT = 1,
    BATTLE_WEATHER_RAIN = 2,
    BATTLE_WEATHER_STORM = 3,
    BATTLE_WEATHER_STRONG_STORM = 4,
    BATTLE_WEATHER_SNOW = 5,
    BATTLE_WEATHER_SNOWSTORM = 6,
    BATTLE_WEATHER_STRONG_SNOWSTORM = 7,
    BATTLE_WEATHER_SNOW_OFFSET = 3,
} battle_map_weather_e;

/* Bits of the map weather flag word at 0x800b6698 ("weather palette mods"). */
typedef enum battle_map_weather_flags {
    BATTLE_MAP_WEATHER_FLAG_SNOW = 0x01,
    BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER = 0x02,
} battle_map_weather_flags_e;

typedef enum element_flags {
    BATTLE_ELEMENT_FIRE = 0x80,
    BATTLE_ELEMENT_LIGHTNING = 0x40,
    BATTLE_ELEMENT_ICE = 0x20,
    BATTLE_ELEMENT_WIND = 0x10,
    BATTLE_ELEMENT_EARTH = 0x08,
    BATTLE_ELEMENT_WATER = 0x04,
    BATTLE_ELEMENT_HOLY = 0x02,
    BATTLE_ELEMENT_DARK = 0x01,
} element_flags_e;

/* Shared ordering for job, item-attribute, and runtime unit affinity arrays.
 * Job records contain only the first four entries; item and runtime records
 * also contain Strengthen. */
typedef enum elemental_affinity_index {
    ELEMENTAL_AFFINITY_ABSORB = 0,
    ELEMENTAL_AFFINITY_NULLIFY = 1,
    ELEMENTAL_AFFINITY_HALF = 2,
    ELEMENTAL_AFFINITY_WEAK = 3,
    ELEMENTAL_AFFINITY_STRENGTHEN = 4,
    ELEMENTAL_AFFINITY_COUNT = 5,
} elemental_affinity_index_e;

/*
 * Values dispatched by battle_state_run_game_loop, verified against the
 * target's switch table.  The target has no explicit handler for value 0x32.
 */
typedef enum battle_game_state {
    BATTLE_GAME_STATE_FREE_CURSOR = 0x00,
    BATTLE_GAME_STATE_FREE_CURSOR_HELP = 0x01,
    BATTLE_GAME_STATE_HIGHLIGHT_UNITS = 0x02,
    BATTLE_GAME_STATE_OPEN_ACTION_MENUS = 0x03,
    BATTLE_GAME_STATE_IDLING_ACTION_MENUS = 0x04,
    BATTLE_GAME_STATE_MENU_TO_TARGETING = 0x05,
    BATTLE_GAME_STATE_ACTION_HELP_MENU = 0x06,
    BATTLE_GAME_STATE_DISPLAY_MOVE_AREA = 0x07,
    BATTLE_GAME_STATE_MINI_MENU = 0x08,
    BATTLE_GAME_STATE_MINI_MENU_HELP = 0x09,
    BATTLE_GAME_STATE_ACTIVE_TURN = 0x0a,
    BATTLE_GAME_STATE_STATUS_EXECUTE = 0x0b,
    BATTLE_GAME_STATE_UNIT_MOVE = 0x0c,
    BATTLE_GAME_STATE_CLOSE_MOVE_HELP = 0x0d,
    BATTLE_GAME_STATE_MOVE_RANGE_EXCEPTION = 0x0e,
    BATTLE_GAME_STATE_ILLEGAL_MOVE_MENU = 0x0f,
    BATTLE_GAME_STATE_UNIT_MOVING_SETUP = 0x10,
    BATTLE_GAME_STATE_UNIT_MOVING = 0x11,
    BATTLE_GAME_STATE_MOVE_CONFIRM_MENU = 0x12,
    BATTLE_GAME_STATE_WAIT_DIRECTION = 0x13,
    BATTLE_GAME_STATE_WAIT_MENU = 0x14,
    BATTLE_GAME_STATE_CRYSTAL_LEARN = 0x15,
    BATTLE_GAME_STATE_ACTION_EXECUTE_SETUP = 0x16,
    BATTLE_GAME_STATE_TARGETING_RANGE = 0x17,
    BATTLE_GAME_STATE_ILLEGAL_RANGE = 0x18,
    BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING = 0x19,
    BATTLE_GAME_STATE_ABILITY_PREVIEW_HELP = 0x1a,
    BATTLE_GAME_STATE_CONFIRM_ACTION = 0x1b,
    BATTLE_GAME_STATE_PRE_ATTACK_ANIMATION = 0x1c,
    BATTLE_GAME_STATE_ACTION_CAST = 0x1d,
    BATTLE_GAME_STATE_COMMENCE_ATTACK_PHASE = 0x1e,
    BATTLE_GAME_STATE_TARGET_SELECT_START = 0x1f,
    BATTLE_GAME_STATE_TARGET_SELECT = 0x20,
    BATTLE_GAME_STATE_TARGET_SELECT_DENIED = 0x21,
    BATTLE_GAME_STATE_TARGET_SELECT_CONFIRM = 0x22,
    BATTLE_GAME_STATE_TARGET_DISPLAY_START = 0x23,
    BATTLE_GAME_STATE_TARGET_DISPLAY = 0x24,
    BATTLE_GAME_STATE_AFTER_COMMAND = 0x25,
    BATTLE_GAME_STATE_JP_EXP_GAIN = 0x26,
    BATTLE_GAME_STATE_CHANGE_TURN = 0x27,
    BATTLE_GAME_STATE_LEARN_ABILITY_ON_HIT = 0x28,
    BATTLE_GAME_STATE_EFFECT_DAMAGE_DISPLAY = 0x29,
    BATTLE_GAME_STATE_OPEN_SP2_FILES = 0x2a,
    BATTLE_GAME_STATE_START_EFFECT_FILE_OPEN = 0x2b,
    BATTLE_GAME_STATE_SECONDARY_EFFECT = 0x2c,
    BATTLE_GAME_STATE_ACTION_EXECUTE = 0x2d,
    BATTLE_GAME_STATE_BATTLE_MESSAGE_DISPLAY = 0x2e,
    BATTLE_GAME_STATE_RESUME_ATTACK_PHASE = 0x2f,
    BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_LOAD = 0x30,
    BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_FINISH = 0x31,
    BATTLE_GAME_STATE_EFFECT = 0x33,
    BATTLE_GAME_STATE_EVENT = 0x34,
    BATTLE_GAME_STATE_MAP_JUMPING_OUT = 0x35,
    BATTLE_GAME_STATE_MAP_INITIALIZE = 0x36,
    BATTLE_GAME_STATE_MAP_JUMPING_IN = 0x37,
    BATTLE_GAME_STATE_MAP_JUMPING_OUT_2 = 0x38,
    BATTLE_GAME_STATE_MAP_INITIALIZE_2 = 0x39,
    BATTLE_GAME_STATE_MAP_JUMPING_IN_2 = 0x3a,
    BATTLE_GAME_STATE_CLOSE_BATTLE = 0x3b,
} battle_game_state_e;

typedef enum battle_equipment_categories_1 {
    BATTLE_EQUIPMENT_SET_1_SWORD = 0x10,
    BATTLE_EQUIPMENT_SET_1_KATANA = 0x04,
    BATTLE_EQUIPMENT_SET_1_AXE = 0x02,
} battle_unit_equipment_categories_1_e;

typedef enum battle_equipment_categories_2 {
    BATTLE_EQUIPMENT_SET_2_CROSSBOW = 0x10,
    BATTLE_EQUIPMENT_SET_2_SPEAR = 0x01,
    BATTLE_EQUIPMENT_SET_2_GUN = 0x20,
} battle_unit_equipment_categories_2_e;

typedef enum battle_equipment_categories_3 {
    BATTLE_EQUIPMENT_SET_3_ARMOR = 0x01,
    BATTLE_EQUIPMENT_SET_3_HAIR_ORNAMENT = 0x02,
    BATTLE_EQUIPMENT_SET_3_HELM = 0x08,
    BATTLE_EQUIPMENT_SET_3_SHIELD = 0x10,
    BATTLE_EQUIPMENT_SET_3_BAG = 0x40,
} battle_unit_equipment_categories_3_e;

typedef enum battle_equipment_categories_4 {
    BATTLE_EQUIPMENT_SET_4_PERFUME = 0x01,
} battle_unit_equipment_categories_4_e;

typedef enum battle_unit_equipped_flags {
    BATTLE_UNIT_EQUIPPED_FLAG_MATERIA_BLADE = 0x04,
    BATTLE_UNIT_EQUIPPED_FLAG_SWORD = 0x08,
} battle_unit_equipped_flags_e;

typedef enum battle_ai_command_kind {
    BATTLE_AI_COMMAND_MOVE = 0,
    BATTLE_AI_COMMAND_ACT = 1,
    BATTLE_AI_COMMAND_END_TURN = 2,
} battle_ai_command_kind_e;

typedef enum battle_menu_system_command {
    BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE = 5,
    BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT = 7,
} battle_menu_system_command_e;

/* Commands of 0x800e77b8, which stores RGB and controls GTE background color. */
typedef enum battle_background_color_command {
    BATTLE_BACKGROUND_COLOR_SET = 9,
    BATTLE_BACKGROUND_COLOR_GET = 10,
    BATTLE_BACKGROUND_COLOR_DISABLE = 11,
    BATTLE_BACKGROUND_COLOR_RESTORE = 12
} battle_map_background_color_command_e;

typedef enum battle_secondary_effect_phase {
    BATTLE_SECONDARY_EFFECT_INITIALIZING = 1,
    BATTLE_SECONDARY_EFFECT_EXECUTING = 2,
    BATTLE_SECONDARY_EFFECT_FINALIZING = 3
} battle_effect_secondary_phase_e;

typedef enum battle_unit_height_flags {
    BATTLE_UNIT_HEIGHT_NON_CHOCOBO = 0x20,
    BATTLE_UNIT_HEIGHT_SHARED_TILE = 0x40,
    BATTLE_UNIT_HEIGHT_UNAVAILABLE = 0x80,
} battle_unit_height_flags_e;

enum {
    BATTLE_AI_ABILITY_FLAG_1_TARGET_ENEMIES = 0x02,
    BATTLE_AI_ABILITY_FLAG_1_TARGET_ALLIES = 0x01,
    BATTLE_AI_ABILITY_FLAG_2_TARGET_MAP_TILES = 0x80,
    BATTLE_AI_ABILITY_FLAG_3_EXHAUSTIVE_ORIGIN_MASK = 0x3c,
    BATTLE_AI_ABILITY_FLAG_4_USABLE_BY_AI = 0x80,
};

/* Full-word masks, distinct from the byte-view FLAG_1 constants above.
 * Tests in 0x8019e5d8..0x8019ef24 establish the low-byte effect categories
 * and distinguish team preferences from the absolute ONLY restrictions. */
enum {
    BATTLE_AI_ABILITY_TARGET_ALLIES = 0x00000001,
    BATTLE_AI_ABILITY_TARGET_ENEMIES = 0x00000002,
    BATTLE_AI_ABILITY_UNEQUIP = 0x00000004,
    BATTLE_AI_ABILITY_AFFECTS_STATS = 0x00000008,
    BATTLE_AI_ABILITY_ADD_STATUS = 0x00000010,
    BATTLE_AI_ABILITY_CANCEL_STATUS = 0x00000020,
    BATTLE_AI_ABILITY_AFFECTS_MP = 0x00000040,
    BATTLE_AI_ABILITY_AFFECTS_HP = 0x00000080,
    BATTLE_AI_ABILITY_AFFECTED_BY_SILENCE = 0x00000100,
    BATTLE_AI_ABILITY_EVADEABLE = 0x00000200,
    BATTLE_AI_ABILITY_AFFECTED_BY_FAITH = 0x00000400,
    BATTLE_AI_ABILITY_RANDOM_USE = 0x00000800,
    BATTLE_AI_ABILITY_FOLLOW_TARGET = 0x00001000,
    BATTLE_AI_ABILITY_UNDEAD_REVERSE = 0x00002000,
    BATTLE_AI_ABILITY_REFLECTABLE = 0x00004000,
    BATTLE_AI_ABILITY_TARGET_MAP_TILES = 0x00008000,
    BATTLE_AI_ABILITY_LINEAR_TRAJECTORY = 0x00200000,
    BATTLE_AI_ABILITY_DIRECT_TRAJECTORY = 0x00400000,
    BATTLE_AI_ABILITY_ARC_TRAJECTORY = 0x00800000,
    BATTLE_AI_ABILITY_CONSUME_EVALUATED_ORIGINS = 0x01000000,
    BATTLE_AI_ABILITY_EXHAUSTIVE_ORIGIN_SCAN = 0x04000000,
    BATTLE_AI_ABILITY_REQUIRES_MONSTER_SKILL = 0x08000000,
    BATTLE_AI_ABILITY_ENEMIES_ONLY = 0x20000000,
    BATTLE_AI_ABILITY_ALLIES_ONLY = 0x40000000,
    BATTLE_AI_ABILITY_USABLE_BY_AI = 0x80000000,
};

/* Ordered class selection at 0x80174630–0x80174700, not movement ability
 * flags or the separate cost-table selector. Water names describe standing
 * height; the English ability labels in external references disagree. */
typedef enum battle_movement_class {
    BATTLE_MOVEMENT_CLASS_FLY = 0,
    BATTLE_MOVEMENT_CLASS_TELEPORT = 1,
    BATTLE_MOVEMENT_CLASS_FLOAT = 2,
    BATTLE_MOVEMENT_CLASS_LAVA = 3,
    BATTLE_MOVEMENT_CLASS_WATER_SURFACE = 4,
    BATTLE_MOVEMENT_CLASS_WATER_DEPTH_ONE = 5,
    BATTLE_MOVEMENT_CLASS_UNDERWATER = 6,
    BATTLE_MOVEMENT_CLASS_NORMAL = 7,
} battle_move_class_e;

typedef enum battle_effective_movement_flags {
    BATTLE_EFFECTIVE_MOVEMENT_SILENT_WALK = 0x02,
    BATTLE_EFFECTIVE_MOVEMENT_TELEPORT = 0x08,
    BATTLE_EFFECTIVE_MOVEMENT_IN_WATER = 0x10,
    BATTLE_EFFECTIVE_MOVEMENT_ON_WATER = 0x20,
    BATTLE_EFFECTIVE_MOVEMENT_FLOAT = 0x40,
    BATTLE_EFFECTIVE_MOVEMENT_FLY = 0x80,
} battle_move_effective_flags_e;

/* Selection policy for battle_unit_find_at_tile.  Status bits are inclusive
 * alternatives; the Chocobo bit is an additional requirement. */
typedef enum battle_unit_tile_filter {
    BATTLE_UNIT_TILE_FILTER_TREASURE = 0x01,
    BATTLE_UNIT_TILE_FILTER_DRY_LOW_SLOPE = 0x02,
    BATTLE_UNIT_TILE_FILTER_TRANSPARENT = 0x10,
    BATTLE_UNIT_TILE_FILTER_CRYSTAL = 0x40,
    BATTLE_UNIT_TILE_FILTER_MOUNTABLE_CHOCOBO = 0x80,
    BATTLE_UNIT_TILE_STATUS_FILTERS
    = BATTLE_UNIT_TILE_FILTER_TREASURE | BATTLE_UNIT_TILE_FILTER_TRANSPARENT | BATTLE_UNIT_TILE_FILTER_CRYSTAL,
} battle_unit_tile_filter_e;

/* Verified prefix only: BATTLE 0x801aa7c4 reads/writes a halfword at +6.
 * Neither the full record nor the individual bit meanings are known. */
typedef struct battle_effect_flag_prefix {
    u8 unknown_00[6];
    u16 flags;
} battle_effect_flag_prefix_t;

typedef char
    battle_effect_flag_prefix_flags_must_be_6[((unsigned long)&((battle_effect_flag_prefix_t*)0)->flags == 6) ? 1 : -1];

/* Shared command prefix at battle_stats_t +0x16e and AI action +0x08.
 * The charge evaluator at 0x80195f8c reads ability_id as unsigned; simulation
 * at 0x8019dd8c reads the same bits as signed. This is not the full command. */
typedef struct battle_action_command_prefix {
    u8 actor_id;
    u8 skillset_id;
    u16 ability_id;
} battle_action_command_prefix_t;

typedef char battle_action_command_prefix_size_must_be_4[(sizeof(battle_action_command_prefix_t) == 4) ? 1 : -1];

/* Ordinary SP/PA/MA/Brave/Faith action-result encoding. CT and EXP reuse
 * these bits with distinct sentinel or sign meanings and are excluded. */
typedef enum battle_action_stat_change {
    BATTLE_ACTION_STAT_CHANGE_VALUE_MASK = 0x7f,
    BATTLE_ACTION_STAT_CHANGE_INCREASE = 0x80,
} battle_action_stat_change_e;

/* Bit order mirrors the seven serialized battle_stats_t equipment slots. */
typedef enum battle_action_equipment_slot_mask {
    BATTLE_ACTION_EQUIPMENT_SLOT_HEAD = 0x80,
    BATTLE_ACTION_EQUIPMENT_SLOT_BODY = 0x40,
    BATTLE_ACTION_EQUIPMENT_SLOT_ACCESSORY = 0x20,
    BATTLE_ACTION_EQUIPMENT_SLOT_RIGHT_WEAPON = 0x10,
    BATTLE_ACTION_EQUIPMENT_SLOT_RIGHT_SHIELD = 0x08,
    BATTLE_ACTION_EQUIPMENT_SLOT_LEFT_WEAPON = 0x04,
    BATTLE_ACTION_EQUIPMENT_SLOT_LEFT_SHIELD = 0x02,
} battle_action_equipment_slot_mask_e;

/*
 * Temporary action results, 0x2c bytes at battle_stats_t+0x18c (0x18c..0x1b7).
 * The g_target_current_action / g_attacker_current_action pointers address this
 * block directly.  status_infliction/status_removal mirror the five
 * status_sets.current bytes.
 */
typedef struct battle_action_data {
    u8 hit;          /* 0x00; 1 hit, 0 miss */
    u8 critical;     /* 0x01 */
    u8 miss_type;    /* 0x02; battle_action_miss_type_e */
    u8 item_lost;    /* 0x03; potion / break / steal / draw out */
    u16 hp_damage;   /* 0x04 */
    u16 hp_healing;  /* 0x06 */
    u16 mp_damage;   /* 0x08 */
    u16 mp_healing;  /* 0x0a */
    s16 gil_change;  /* 0x0c; stolen / lost */
    u16 reaction_id; /* 0x0e */
    /* Target readers and writers consistently access 0x10-0x11 as one halfword. */
    u16 special_effect;       /* 0x10; battle_action_special_effect_e */
    u8 sp_change;             /* 0x12 */
    u8 ct_change;             /* 0x13 */
    u8 pa_change;             /* 0x14 */
    u8 ma_change;             /* 0x15 */
    u8 brave_change;          /* 0x16 */
    u8 faith_change;          /* 0x17 */
    u8 status_change;         /* 0x18 */
    u8 equipment_destroyed;   /* 0x19; battle_action_equipment_slot_mask_e; 0xfe is a selection sentinel */
    u8 stolen_item_id;        /* 0x1a */
    u8 status_infliction[5];  /* 0x1b */
    u8 status_removal[5];     /* 0x20 */
    u8 attack_type;           /* 0x25; battle_action_type_e */
    u16 last_received_attack; /* 0x26; reaction data, varies by ability */
    u8 exp_change;            /* 0x28; Steal EXP */
    u8 jp_change;             /* 0x29; Move-JP Up / JP stolen */
    /* Written as a halfword everywhere (battle_formula_apply_status lhu/sh,
     * arrow_guard_usability, battle_formula_apply_catch, force_attack_miss). */
    u16 attack_accuracy; /* 0x2a */
} battle_action_data_t;

typedef char battle_action_data_size_must_be_0x2c[(sizeof(battle_action_data_t) == 0x2c) ? 1 : -1];

/* One 4-byte entry of the 40-entry AT list built by battle_action_calculate_at_list and
 * kept in key order by battle_action_sort_at_list. */
typedef struct battle_at_entry {
    u8 unit;      /* 0x00; unit id, biased by the entry kind */
    u8 flags;     /* 0x01; low byte of the charging ability id */
    u16 sort_key; /* 0x02; insertion key for battle_action_sort_at_list */
} battle_at_entry_t;

typedef char battle_at_entry_size_must_be_4[(sizeof(battle_at_entry_t) == 4) ? 1 : -1];

/* Jump ability record, 12 of them at g_main_jump_ability_data: horizontal
 * range and vertical reach. */
typedef struct battle_loader_jump_record {
    u8 range;    /* 0x00 */
    u8 vertical; /* 0x01 */
} battle_loader_jump_record_t;

typedef char battle_loader_jump_record_size_must_be_2[(sizeof(battle_loader_jump_record_t) == 2) ? 1 : -1];

extern battle_loader_jump_record_t g_main_jump_ability_data[12];

/* The strike loader fills this 40-byte work record; it is distinct from
 * battle_stats_t.action. Reaction targeting writes redirected tile fields at
 * +0x1c..+0x1e before the strike is processed. */
typedef struct battle_strike_work {
    u8 actor_id;
    u8 target_count;
    u8 target_list[16];
    u8 animate_on_miss_flag;
    u8 control_value_19f;
    s16 last_attack_id;
    u8 ability_formula;
    u8 reaction_occurred;
    u8 continue_attack;
    u8 current_hit_number;
    u16 reaction_id_1a;
    u8 target_new_x;
    u8 target_new_y;
    u8 target_new_map_level;
    u8 used_weapon_id;
    u16 reaction_ability_id; /* 0x20; misc 0x1ac */
    /* 0x22; misc 0x1ae. g_current_ability.knockback_flags & 0x7f, stored with
     * the knockback destination. */
    u8 knockback_flags;
    u8 can_earn_experience;
    u8 unknown_24[4];
} battle_strike_work_t;

typedef char battle_strike_work_must_be_40[sizeof(battle_strike_work_t) == 40 ? 1 : -1];
typedef char battle_strike_targets_must_be_2[((unsigned long)&((battle_strike_work_t*)0)->target_list == 2) ? 1 : -1];
typedef char
    battle_strike_continue_must_be_0x18[((unsigned long)&((battle_strike_work_t*)0)->continue_attack == 0x18) ? 1 : -1];

/*
 * Battle Stats are the simulation-side unit records.  They occupy 0x1c0
 * bytes, with 21 consecutive slots beginning at 0x801908cc in BATTLE.BIN.
 * Do not confuse this with Misc unit_t Data, the renderer-side record that
 * points back to battle_stats_t at offset 0x134.
 *
 * Unidentified areas intentionally remain explicit padding until evidence
 * supports a name and type.
 */
/* Packed day-of-year/zodiac word at unit offset 0x008. The halfword masks
 * in SCUS 0x8005ac1c and the bitfield stores at 0x8005b0d0 establish both views. */
typedef union battle_unit_birthday {
    u16 value;
    struct {
        u16 birthday : 9;
        u16 unknown : 3;
        u16 zodiac : 4;
    } fields;
} battle_unit_birthday_t;

typedef char battle_birthday_size_must_be_2[(sizeof(battle_unit_birthday_t) == 2) ? 1 : -1];

/* SCUS 0x8005b5dc copies these three consecutive sets as one 15-byte block.
 * Keep a named aggregate so whole-block byte copies do not walk beyond the
 * first five-byte array. Individual status readers still use named sets. */
typedef struct battle_status_sets {
    u8 innate[5];
    u8 immunity[5];
    u8 current[5];
} battle_status_sets_t;

typedef char battle_status_sets_size_must_be_15[(sizeof(battle_status_sets_t) == 15) ? 1 : -1];

/* battle_stats_t.entd_slot value for an unused or removed slot. */
enum {
    BATTLE_ENTD_SLOT_NONE = 0xff,
};

typedef struct battle_stats {
    u8 character_identity;            /* 0x000 */
    u8 entd_slot;                     /* 0x001; 0xff means absent */
    u8 formation_index;               /* 0x002 */
    u8 job_id;                        /* 0x003 */
    u8 sprite_palette;                /* 0x004 */
    u8 team_flags;                    /* 0x005 */
    u8 unit_flags;                    /* 0x006; mixed sex, monster, guest, and formation flags */
    u8 death_counter;                 /* 0x007 */
    battle_unit_birthday_t birthday;  /* 0x008; zodiac is the high nibble */
    u16 innate_abilities[4];          /* 0x00a */
    u8 primary_skillset;              /* 0x012 */
    u8 secondary_skillset;            /* 0x013 */
    u16 reaction_ability;             /* 0x014 */
    u16 support_ability;              /* 0x016 */
    u16 movement_ability;             /* 0x018 */
    u8 equipment[7];                  /* 0x01a; unit_equipment_slot_e */
    u8 experience;                    /* 0x021 */
    u8 level;                         /* 0x022 */
    u8 original_brave;                /* 0x023 */
    u8 brave;                         /* 0x024 */
    u8 original_faith;                /* 0x025 */
    u8 faith;                         /* 0x026 */
    u8 transparent_removal_flag;      /* 0x027 */
    u16 hp;                           /* 0x028 */
    u16 max_hp;                       /* 0x02a */
    u16 mp;                           /* 0x02c */
    u16 max_mp;                       /* 0x02e */
    u8 base_attributes[3];            /* 0x030; unit_attribute_index_e */
    u8 equipment_attributes[3];       /* 0x033; unit_attribute_index_e */
    u8 attributes[3];                 /* 0x036; unit_attribute_index_e */
    u8 ct;                            /* 0x039 */
    u8 move;                          /* 0x03a */
    u8 jump;                          /* 0x03b */
    u8 equipment_stats[11];           /* 0x03c; battle_unit_equipment_stat_index_e */
    u8 x;                             /* 0x047 */
    battle_unit_position_t position;  /* 0x048 */
    u8 equipment_categories[4];       /* 0x04a */
    battle_status_sets_t status_sets; /* 0x04e; innate / immunity / current */
    u8 status_ct[16];                 /* 0x05d */
    u8 elemental_affinity[5];         /* 0x06d; elemental_affinity_index_e */
    u8 raw_stats[0x19];               /* 0x072 */
    u8 reaction_abilities[4];         /* 0x08b; battle_unit_reaction_set_N_e */
    u8 support_abilities[4];          /* 0x08f */
    u8 movement_abilities[3];         /* 0x093 */
    u8 unlocked_jobs[3];              /* 0x096 */
    u8 learned_abilities[57];         /* 0x099..0x0d1; three bytes per learned-ability row */
    u8 job_levels[10];                /* 0x0d2; two packed job-level nibbles per byte */
    u16 job_points[20];               /* 0x0dc; SCUS party-to-battle copy at 0x8005b0d0 */
    u16 total_job_points[20];         /* 0x104; contiguous with current JP, through 0x12b */
    /* main_unit_store_character_names (0x8005c984) copies 16/16/8/8
     * bytes to these four destinations. All names use the game's encoding. */
    u8 name[16];                       /* 0x12c */
    u8 job_name[16];                   /* 0x13c */
    u8 primary_skillset_name[8];       /* 0x14c */
    u8 secondary_skillset_name[8];     /* 0x154 */
    u8 ko_count;                       /* 0x15c */
    u8 charged_ability_ct;             /* 0x15d */
    u8 graphic_variant;                /* 0x15e; copied from job data and consumed by WORLD formation graphics */
    u8 spritesheet_id;                 /* 0x15f */
    u8 job_portrait_palette;           /* 0x160 */
    u8 unit_id;                        /* 0x161 */
    u8 base_job_skillset;              /* 0x162 */
    u8 war_trophy;                     /* 0x163 */
    u8 bonus_money_modifier;           /* 0x164 */
    u8 ai_target_x;                    /* 0x165 */
    u8 ai_target_y;                    /* 0x166 */
    u8 ai_flags;                       /* 0x167 */
    u8 prioritized_target;             /* 0x168 */
    u8 ai_targeting_flags_1;           /* 0x169; copied to battle_ai_unit_decision_t +0x06 */
    u8 ai_targeting_flags_2;           /* 0x16a; battle_ai_targeting_flags_2_e */
    u8 entd_unknown_right;             /* 0x16b */
    u16 quote_name_id;                 /* 0x16c */
    u8 action_actor_id;                /* 0x16e */
    u8 last_skillset_id;               /* 0x16f */
    s16 last_ability_id;               /* 0x170 */
    u16 calculator_type_ability;       /* 0x172 */
    u16 calculator_multiplier_ability; /* 0x174 */
    u16 used_item_or_equipment;        /* 0x176 */
    u8 action_target_kind;             /* 0x178; battle_action_target_kind_e */
    u8 action_target_id;               /* 0x179 */
    s16 action_target_x;               /* 0x17a */
    s16 action_target_elevation;       /* 0x17c */
    s16 action_target_y;               /* 0x17e */
    u8 death_on_chocobo;               /* 0x180 */
    u8 action_tile_flags;              /* 0x181 */
    u8 mount_info;                     /* 0x182 */
    u8 existence;                      /* 0x183; battle_unit_existence_state_e */
    u8 equipped_flags;                 /* 0x184; battle_unit_equipped_flags_e */
    u8 _pad185;
    u8 has_turn;                 /* 0x186 */
    u8 movement_taken;           /* 0x187 */
    u8 action_taken;             /* 0x188 */
    u8 ability_outcome;          /* 0x189 */
    u8 misc_unit_id;             /* 0x18a */
    u8 ability_ct;               /* 0x18b */
    battle_action_data_t action; /* 0x18c through 0x1b7 */
    u8 auto_battle_setting;      /* 0x1b8 */
    u8 auto_battle_target;       /* 0x1b9 */
    u8 initial_team_flags;       /* 0x1ba */
    u8 inflicted_status[5];      /* 0x1bb */
} battle_stats_t;

typedef char battle_stats_size_must_be_0x1c0[(sizeof(battle_stats_t) == 0x1c0) ? 1 : -1];

typedef char battle_stats_innate_status_must_be_0x4e[((unsigned long)&((battle_stats_t*)0)->status_sets.innate == 0x4e)
        ? 1
        : -1];

typedef char battle_stats_status_immunity_must_be_0x53
    [((unsigned long)&((battle_stats_t*)0)->status_sets.immunity == 0x53) ? 1 : -1];

typedef char
    battle_stats_current_status_must_be_0x58[((unsigned long)&((battle_stats_t*)0)->status_sets.current == 0x58) ? 1
                                                                                                                 : -1];

/* Word-view masks for the high motion byte at misc-unit +0x83. */
typedef enum battle_motion_flags {
    BATTLE_MOTION_FLAG_SUPPRESS_SFX_AND_LANDING_EFFECTS = 0x02000000,
    BATTLE_MOTION_FLAG_SUPPRESS_PALETTE_UPDATE = 0x04000000,
    BATTLE_MOTION_FLAG_ALTERNATE_SHORT_HOP = 0x08000000,
    BATTLE_MOTION_FLAG_FLOAT = 0x10000000,
} battle_motion_flags_e;

typedef union battle_move_destination {
    u32 word;
    struct {
        u8 destination_x;
        u8 destination_y;
        u8 destination_z;
        u8 motion_flags;
    } bytes;
} battle_move_destination_t;

/* One 0x30-byte sprite animation state. Misc unit data holds four from
 * 0x1d8: the unit sprite's own state (spelled out field by field in
 * battle_unit_misc_data_t) and three weapon/effect battle_unit_sprite_block_t
 * records. The first 0x24 bytes are common to both; the unit state keeps its
 * saved SHP/SEQ pointers at 0x24/0x28 and its display pointer at 0x2c. */
typedef struct battle_unit_anim_state {
    u16 trigger;                               /* 0x00 */
    u16 graphic_type;                          /* 0x02; 0 for the unit sprite (battle_unit_init_misc_data) */
    u16 animation_id;                          /* 0x04 */
    u16 script_pos;                            /* 0x06 */
    u16 frame;                                 /* 0x08 */
    u16 wait;                                  /* 0x0a */
    u16 counter_0c;                            /* 0x0c */
    u16 saved_animation_id;                    /* 0x0e */
    u16 saved_script_pos;                      /* 0x10 */
    u16 wait_bias;                             /* 0x12 */
    u16 frame_bias;                            /* 0x14 */
    u16 loop_count;                            /* 0x16 */
    u16 flags;                                 /* 0x18 */
    u8 _pad1a[2];                              /* 0x1a */
    s32* shp;                                  /* 0x1c; SHP frame table: 0x008 normal, 0x348 submerged */
    struct battle_gfx_shp0* seq;               /* 0x20 */
    u8* saved_shp;                             /* 0x24 */
    u8* saved_seq;                             /* 0x28 */
    battle_gfx_sprite_display_data_t* display; /* 0x2c */
} battle_unit_anim_state_t;

/* WEP/EFF SHP data: the first frame of each weapon type, then the frame
 * pointers from 0x40. */
typedef struct battle_gfx_weapon_shp {
    u16 first_frame[0x20];                /* 0x00; indexed by weapon type */
    battle_gfx_source_frame_t* frames[1]; /* 0x40 */
} battle_gfx_weapon_shp_t;

/* Weapon/effect sprite animation block, misc unit data 0x208, 0x238 and
 * 0x268. It shares the first 0x24 bytes of battle_unit_anim_state_t but keeps
 * its display pointer at 0x24 (battle_gfx_run_wep_eff_seq_script, battle_gfx_draw_unit_sprite_layers,
 * battle_unit_init_misc_data). Its SEQ table has no header: seq[animation] is the script. */
typedef struct battle_unit_sprite_block {
    u16 trigger;                               /* 0x00 */
    u16 graphic_type;                          /* 0x02; 1 weapon (WEP1), 2 effect (EFF) */
    u16 animation_id;                          /* 0x04 */
    u16 script_pos;                            /* 0x06 */
    u16 frame;                                 /* 0x08 */
    u16 wait;                                  /* 0x0a */
    u16 counter_0c;                            /* 0x0c */
    u16 saved_animation_id;                    /* 0x0e */
    u16 saved_script_pos;                      /* 0x10 */
    u16 wait_bias;                             /* 0x12 */
    u16 frame_bias;                            /* 0x14 */
    u16 loop_count;                            /* 0x16 */
    u16 flags;                                 /* 0x18 */
    u8 _pad1a[2];                              /* 0x1a */
    battle_gfx_weapon_shp_t* shp;              /* 0x1c */
    u8** seq;                                  /* 0x20 */
    battle_gfx_sprite_display_data_t* display; /* 0x24 */
    u8 _unknown_28[8];                         /* 0x28 */
} battle_unit_sprite_block_t;

typedef char battle_unit_sprite_block_size_must_be_0x30[(sizeof(battle_unit_sprite_block_t) == 0x30) ? 1 : -1];

typedef struct battle_ai_command_action {
    u8 unit_id;
    u8 skillset;
    u16 ability_id;
    u16 calculator_type;
    u16 calculator_multiplier;
    u8 item_id;
    u8 unknown_09;
    u8 targeting_type;
    u8 target_id;
    u16 target_x;
    u16 target_elevation;
    u16 target_y;
    u8 unknown_12[2];
} battle_ai_command_action_t;

typedef char battle_ai_command_action_size_must_be_20[sizeof(battle_ai_command_action_t) == 20 ? 1 : -1];

/* The setup dispatcher writes this at Misc +0x158 and copies 24 bytes into
 * each history slot. ACT copies the 20-byte payload from AI action +0x08;
 * MOVE writes only x/elevation/y, and END_TURN leaves the payload unchanged. */
typedef struct battle_ai_command {
    battle_ai_command_kind_e kind;
    union {
        u16 halfwords[10];
        struct {
            u16 x;
            u16 elevation;
            u16 y;
            u8 unknown_06[14];
        } move;
        battle_ai_command_action_t action;
    } data;
} battle_ai_command_t;

typedef char battle_ai_command_size_must_be_24[sizeof(battle_ai_command_t) == 24 ? 1 : -1];

/* Manual targeting and WAIT_DIRECTION reuse the AI command storage.
 * The facing word overlaps the action's item/type/target-ID bytes. Cursor
 * coordinate readers use signed halfwords over the action's target fields. */
typedef union battle_unit_command_state {
    battle_ai_command_t ai;
    struct {
        u8 unknown_00[12];
        s32 facing_hint;
        SVECTOR target_panel; /* vx = panel x, vy = map level, vz = panel y */
    } cursor;
} battle_unit_command_state_t;

typedef char battle_misc_command_state_size_must_be_24[sizeof(battle_unit_command_state_t) == 24 ? 1 : -1];

/* Misc unit_t Data byte read as a byte or as a halfword over the next byte. */
typedef union battle_gfx_sprite_display_flags {
    u8 byte;
    u16 half;
} battle_gfx_sprite_display_flags_t;

/* Walk speed halfword pair, read and stored as one word. */
typedef union battle_move_walk_speed {
    s32 word;
    struct {
        u16 speed;
        u16 copy;
    } halves;
} battle_move_walk_speed_t;

/* Misc unit_t Data halfword read both signed and unsigned. */
typedef union battle_unit_misc_halfword {
    s16 s;
    u16 u;
} battle_unit_misc_halfword_t;

/* Renderer-side Misc-unit status mirror. These packed masks do not use the
 * canonical battle_status_id_e byte order. */
typedef enum battle_misc_status_flags_1_4 {
    /* Misc +0x140. Bit 0x00000001 remains unknown. */
    BATTLE_MISC_STATUS_CONFUSION = 0x00000080,
    BATTLE_MISC_STATUS_SLEEP = 0x00000040,
    BATTLE_MISC_STATUS_PETRIFY = 0x00000020,
    BATTLE_MISC_STATUS_STOP = 0x00000010,
    BATTLE_MISC_STATUS_CRITICAL = 0x00000008,
    BATTLE_MISC_STATUS_DEAD = 0x00000004,
    BATTLE_MISC_STATUS_MOUNTED = 0x00000002,

    /* Misc +0x141. */
    BATTLE_MISC_STATUS_FAITH = 0x00008000,
    BATTLE_MISC_STATUS_BERSERK = 0x00004000,
    BATTLE_MISC_STATUS_CURSED = 0x00002000,
    BATTLE_MISC_STATUS_HASTE = 0x00001000,
    BATTLE_MISC_STATUS_SLOW = 0x00000800,
    BATTLE_MISC_STATUS_DEFENDING = 0x00000400,
    BATTLE_MISC_STATUS_CHARGING = 0x00000200,
    BATTLE_MISC_STATUS_PERFORMING = 0x00000100,

    /* Misc +0x142. */
    BATTLE_MISC_STATUS_OIL = 0x00800000,
    BATTLE_MISC_STATUS_POISON = 0x00400000,
    BATTLE_MISC_STATUS_PROTECT = 0x00200000,
    BATTLE_MISC_STATUS_SHELL = 0x00100000,
    BATTLE_MISC_STATUS_DONT_ACT = 0x00080000,
    BATTLE_MISC_STATUS_DONT_MOVE = 0x00040000,
    BATTLE_MISC_STATUS_BLOOD_SUCK = 0x00020000,
    BATTLE_MISC_STATUS_INNOCENT = 0x00010000,

    /* Misc +0x143. Bit 0x80000000 remains unknown. */
    BATTLE_MISC_STATUS_UNDEAD = 0x40000000,
    BATTLE_MISC_STATUS_REGEN = 0x20000000,
    BATTLE_MISC_STATUS_RERAISE = 0x10000000,
    BATTLE_MISC_STATUS_DARKNESS = 0x08000000,
    BATTLE_MISC_STATUS_DEATH_SENTENCE = 0x04000000,
    BATTLE_MISC_STATUS_SILENCE = 0x02000000,
    BATTLE_MISC_STATUS_CHARM = 0x01000000,

    BATTLE_MISC_STATUS_PALETTE_MOD_MASK = 0x60c26020,
    BATTLE_MISC_STATUS_BUBBLE_MASK = 0x1fffc2c4,
    BATTLE_MISC_STATUS_PREVENT_FACING_MASK = 0x00080074,
    BATTLE_MISC_STATUS_ANIMATION_SELECTION_MASK = 0x00003fff,
} battle_misc_status_flags_1_4_e;

typedef enum battle_misc_status_flags_5_6 {
    /* Misc +0x144. */
    BATTLE_MISC_STATUS_CRYSTAL = 0x00000001,
    BATTLE_MISC_STATUS_CHICKEN = 0x00000002,
    BATTLE_MISC_STATUS_FROG = 0x00000004,
    BATTLE_MISC_STATUS_TREASURE = 0x00000008,
    BATTLE_MISC_STATUS_TRANSPARENT = 0x00000010,
    BATTLE_MISC_STATUS_FLOAT = 0x00000020,
    BATTLE_MISC_STATUS_JUMP = 0x00000040,
    BATTLE_MISC_STATUS_POACHED = 0x00000080,

    /* Misc +0x145. Other bits remain unknown. */
    BATTLE_MISC_STATUS_MORBOL = 0x00000100,
    BATTLE_MISC_STATUS_JUMP_HEIGHT_ACTIVE = 0x00000200,

    BATTLE_MISC_STATUS_TRANSFORMATION_MASK = 0x0000000f,
} battle_misc_status_flags_5_6_e;

/* Reviewed direct spritesheet IDs and exclusive range boundaries; the
 * remaining IDs are not yet a complete semantic domain. */
enum {
    BATTLE_SPRITESHEET_ID_RAMZA_CHAPTER_1 = 0x01,
    BATTLE_SPRITESHEET_ID_RAMZA_END = 0x04,
    BATTLE_SPRITESHEET_ID_ITEM = 0x1e,
    BATTLE_SPRITESHEET_ID_FRAME = 0x1f,
    BATTLE_SPRITESHEET_ID_ALTIMA_FIRST_FORM = 0x41,
    BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM = 0x49,
    BATTLE_SPRITESHEET_ID_CHOCOBO = 0x86,
    BATTLE_SPRITESHEET_ID_SKELETON = 0x8b,
    BATTLE_SPRITESHEET_ID_GHOUL = 0x8c,
    BATTLE_SPRITESHEET_ID_MORBOL = 0x92,
    BATTLE_SPRITESHEET_ID_CRYSTAL = 0x9b,
    BATTLE_SPRITESHEET_ID_CHICKEN = 0x9c,
    BATTLE_SPRITESHEET_ID_FROG = 0x9d,
    BATTLE_SPRITESHEET_ID_TREASURE = 0x9e,
};

typedef struct battle_action_reward_display {
    u8 earned_experience;     /* +0x0 */
    u8 earned_jp;             /* +0x1 */
    u8 level_for_display;     /* +0x2; nonzero selects the "Level UP!" display */
    u8 job_level_for_display; /* +0x3 */
} battle_action_reward_display_t;

typedef char battle_action_reward_display_size_must_be_4[(sizeof(battle_action_reward_display_t) == 4) ? 1 : -1];

/* Formation row read by battle_unit_init_deployed_units_data: five 4-byte
 * map coordinate rows at 0x8017f388. */
typedef struct battle_deployed_coords {
    u8 party_id;               /* +0: unit to deploy */
    u8 x;                      /* +1 */
    u8 y;                      /* +2 */
    u8 facing_elevation_flags; /* +3; bit 0x10 marks unit absent before graphics/status setup */
} battle_deployed_coords_t;

/*
 * Misc unit_t Data has 16 renderer-side slots, each 0x440 bytes. This
 * declaration spans the full slot; unverified regions remain padding.
 * The record links to its simulation-side battle_stats_t at 0x134.
 */
/* Halfword tile destination written by battle_unit_find_relocation_tile into battle_unit_misc_data_t.dismount. */
typedef struct battle_dismount_coords {
    s16 x;     /* 0x0 */
    s16 level; /* 0x2 */
    s16 y;     /* 0x4 */
} battle_dismount_coords_t;

typedef struct battle_unit_misc_data {
    struct battle_unit_misc_data* previous; /* 0x000; previous Misc unit_t Data */
    u8 unit_id;                             /* 0x004; Misc unit_t Data ID */
    u8 spritesheet_vram_slot;               /* 0x005 */
    u8 spritesheet_id;                      /* 0x006 */
    u8 stored_palette;                      /* 0x007 */
    u16 state_frame_counter;                /* 0x008; incremented while renderer action states wait */
    u16 ability_in_use;                     /* 0x00a; always accessed as a halfword */
    u16 requested_animation;                /* 0x00c */
    u16 vram_spritesheet_id;                /* 0x00e */
    u16 vram_palette_id;                    /* 0x010 */
    /* Read-modify-written as a halfword over 0x012..0x013
     * (battle_gfx_update_sprite_transparency_flag). */
    battle_gfx_sprite_display_flags_t sprite_display_flags; /* 0x012 */
    /* 0x014; SEQ opcode 0xe2, indexes g_battle_gfx_animation_layer_priorities. */
    u16 layer_priority;
    u8 _pad016[2];
    VECTOR real; /* 0x018; vx = x, vy = height, vz = map depth */
    /* Per-frame real-coordinate velocity; the jump-start distortion animation
     * (0x8008a35c) copies all four words as one block. */
    VECTOR velocity; /* 0x028 */
    /* walk_speed and its copy are read and stored as one word
     * (battle_unit_set_idle_animation_for_movement, battle_move_init_knockback). */
    battle_move_walk_speed_t walk_speed; /* 0x038 */
    s32 step_speed; /* 0x03c; interpolation speed of the current step; 0x2000 when a move/climb starts, raised by
                       jump gravity */
    SVECTOR screen; /* 0x040; vx = x, vy = height, vz = map depth */
    u8 _pad048[8];
    /* Two more SVECTORs (vx = x, vy = height, vz = map depth) at 0x050 and
     * 0x060, used to create vectors for effects processing.
     * battle_unit_shift_forward_or_backward and
     * battle_unit_add_signed_byte_to_height read-modify-write the first;
     * increment_another_coords_by_misc_id(_copy) accumulate into the
     * second. */
    SVECTOR effect_vector; /* 0x050 */
    /* Screen offset added to the projected sprite position (0x80086b44) by
     * SEQ opcode 0xf9; cleared with both effect vectors by
     * battle_unit_place_in_new_location. vx/vy are the screen X/Y offsets; vz is
     * only ever cleared with them. */
    SVECTOR screen_offset;   /* 0x058 */
    SVECTOR effect_vector_2; /* 0x060 */
    u8 _pad068[4];
    /* Camera-relative facing, ((camera yaw + facing) & 0xfff) / 1024 and / 256,
     * stored at 0x80085c0c; copied from mount to rider at 0x80069174/0x80069180;
     * read as signed halfwords by battle_get_alternate_facing_quadrant_* (% 4) and
     * battle_unit_get_facing_field_0x6e_nibble_by_misc_id (% 0x10). */
    battle_unit_misc_halfword_t camera_facing_quadrant;  /* 0x06c */
    battle_unit_misc_halfword_t camera_facing_sixteenth; /* 0x06e */
    u16 facing;                                          /* 0x070 */
    u16 attack_facing;                                   /* 0x072 */
    u16 depth_height_offset;                             /* 0x074 */
    u16 mounted_height_offset;                           /* 0x076 */
    u16 float_bob_phase;                                 /* 0x078; advanced by g_animation_speed at 0x8007ea98 */
    s16 special_graphic_y_offset;                        /* 0x07a */
    u8 map_x;                                            /* 0x07c */
    u8 map_y;                                            /* 0x07d */
    u8 map_z;                                            /* 0x07e */
    u8 centre_tile_offset;                               /* 0x07f */
    battle_move_destination_t movement;
    u8 previous_map_x;          /* 0x084 */
    u8 previous_map_y;          /* 0x085 */
    u8 previous_map_z;          /* 0x086 */
    u8 distortion_animation_id; /* 0x087; dispatcher 0x8008b234 */
    s32 distortion_phase;       /* 0x088; distortion animation state (0x8008a35c, 0x80089640) */
    s32 distortion_timer;       /* 0x08c; frames left in the jump-start rise (0x8008a35c) */
    s32 distortion_target;      /* 0x090; target real.vy or frame threshold of the distortion step */
    u16 previous_facing;        /* 0x094 */
    u8 destination_edge_height; /* 0x096; destination slope corner, as g_battle_move_destination_edge_height */
    u8 current_edge_height;     /* 0x097; current slope corner, as g_battle_move_current_edge_height */
    u32 movement_path_offset;   /* 0x098; relative to this record */
    /*
     * Movement path (0x9c/0x9d). A count of 0xfe means 0x9d..0x9f hold an explicit
     * x/y/z instead of a step list (initialize_units_coordinates_animation_facing); a
     * non-zero count means the unit is still moving (unit_moving_check_by_misc_id,
     * process_unit_movement); set_unit_movement_flag ORs 0x10 into a step.
     */
    u8 movement_path_count; /* 0x09c */
    u8 movement_path[0x7c]; /* 0x09d..0x118; step: direction | 0x20 higher elevation | jump length */
    u8 movement_flags;      /* 0x119; battle_move_effective_flags_e bits */
    u8 _pad11a;
    u8 mount_byte;                /* 0x11b */
    u8 movement_value;            /* 0x11c */
    u8 last_path_count;           /* 0x11d; receives prior movement-path count at 0x8006d7b8 */
    u8 current_unit_id_plus_one;  /* 0x11e */
    u8 previous_unit_id_plus_one; /* 0x11f */
    s16 item_get_camera_x;        /* 0x120 */
    s16 item_get_camera_y;        /* 0x122 */
    u8 _pad124[4];
    u32 otag_depth_index;              /* 0x128 */
    s32 status_bubble_gte_flag;        /* 0x12c; RotTrans flag output while positioning the status bubble */
    u8 mount_state;                    /* 0x130; battle_misc_mount_state_e */
    u8 mount_partner_misc_id;          /* 0x131; linked rider or mount misc ID */
    u8 previous_mount_state;           /* 0x132; battle_misc_mount_state_e */
    u8 previous_mount_partner_misc_id; /* 0x133 */
    battle_stats_t* battle_data;       /* 0x134; battle_stats_t backlink */
    u16 used_ability_id;               /* 0x138 */
    u8 used_item_or_weapon_id;         /* 0x13a */
    u8 equipped_weapon_type;           /* 0x13b */
    u8 ability_ct_resolved;            /* 0x13c */
    u8 team_flags;                     /* 0x13d */
    u8 palette_modifier;               /* 0x13e */
    u8 horizontal_flip_flag;           /* 0x13f */
    u32 status_flags_1_4;              /* 0x140; battle_misc_status_flags_1_4_e */
    u32 status_flags_5_6;              /* 0x144; battle_misc_status_flags_5_6_e */
    /* 0x148..0x157: statuses to add/remove;
     * battle_unit_update_float_and_jump_height tests the pending removal of Jump. */
    u32 statuses_to_add_1_4;                   /* 0x148 */
    u32 statuses_to_add_5_6;                   /* 0x14c */
    u32 statuses_to_remove_1_4;                /* 0x150 */
    u32 statuses_to_remove_5_6;                /* 0x154 */
    battle_unit_command_state_t command_state; /* 0x158..0x16f */
    s32 sp2_ability_id;                        /* 0x170: ability whose SP2 file battle_open_sp2 loads */
    s32 ability_preview_phase;                 /* 0x174: ability preview branch selector */
    /* 0x178; battle_target_calculate_for_menu_types result stored by
     * battle_target_select_tile: 0/1 preview, 2 stop, -1 back to the menu. */
    s32 target_select_result;
    s32 attack_phase_state;    /* 0x17c: 3 == commence, else pre-phase */
    s32 pending_attack_result; /* 0x180: attack result code; -1 signals death */
    /* Death-by-dismount destination (Miscellaneous unit_t Data 0x184/0x186/
     * 0x188); battle_unit_set_map_coords_after_death_dismount copies them to
     * map_x/map_z/map_y.  attack_result_animation_update passes &dismount. */
    battle_dismount_coords_t dismount; /* 0x184 */
    u8 _pad18a[2];
    /* Current action data, 0x18c..0x1d7. */
    u8 action_18c;           /* 0x18c; "Reaction ID / Attacking unit ID? - Used Ability ID" */
    u8 target_count;         /* 0x18d; attacker_face_targets, update_anim_display_for_all_targets */
    u8 target_list[0x10];    /* 0x18e; misc ids, 0xff terminated */
    u8 animate_on_miss_flag; /* 0x19e; 1 when animate-on-miss is false, cleared on hit */
    u8 control_value_19f;    /* 0x19f; 0x01 math skill */
    u16 last_attack_id;      /* 0x1a0 */
    u8 ability_formula;      /* 0x1a2; can poach? */
    u8 reaction_occurred;    /* 0x1a3 */
    u8 continue_attack;      /* 0x1a4 */
    u8 current_hit_number;   /* 0x1a5 */
    u8 reaction_id_1a6;      /* 0x1a6 */
    u8 _unknown_1a7;
    u8 target_new_x;         /* 0x1a8; post-action knockback destination (transfer_target_coordinates) */
    u8 target_new_y;         /* 0x1a9 */
    u8 target_new_map_level; /* 0x1aa */
    u8 used_weapon_id;       /* 0x1ab */
    s16 reaction_ability_id; /* 0x1ac; 0x1b8 (Reflect) suppresses auto-facing at 0x80072838 */
    u8 knockback_flags;      /* 0x1ae; battle_strike_work_t.knockback_flags */
    u8 can_earn_experience;  /* 0x1af */
    battle_action_reward_display_t action_rewards; /* 0x1b0..0x1b3 */
    u8 command_ready; /* 0x1b4; 0x800753b8 sets 1 after AI setup completes; gates command dispatch */
    /* 0x1b5; strikes run after the first while continue_attack is set; zeroed
     * for each new action. */
    u8 continue_attack_count;
    /* 0x1b6; command kind stored before target select (0x0c enemy, 0x0e ally
     * required) or target display (0x0d, 0x0f). */
    u8 target_select_command;
    u8 status_display_image_count;                      /* 0x1b7; capped at 0x1b */
    battle_action_display_flags_t action_display_flags; /* 0x1b8..0x1bb */
    /* Action-display image IDs, not canonical status IDs.  Bit 0x80 marks a
     * removal; the dispatcher consumes entries from the end. */
    u8 status_display_image_list[BATTLE_ACTION_STATUS_DISPLAY_CAPACITY]; /* 0x1bc..0x1d6 */
    u8 _pad1d7;
    /* The unit_t sprite data block starts at 0x1d8; the trigger is written
     * as a halfword by animate_and_set_enemy_level_data_by_misc_id.
     * 0x1d8..0x207 is the unit's battle_unit_anim_state_t, spelled out here. */
    u16 sprite_graphic_trigger;    /* 0x1d8 */
    u16 sprite_graphic_type;       /* 0x1da */
    u16 encoded_animation;         /* 0x1dc: half-shifted animation id */
    u16 animation_script_pos;      /* 0x1de: frame-command byte counter (anim state script_pos) */
    u16 animation_frame;           /* 0x1e0: current frame (anim state frame) */
    u16 animation_countdown;       /* 0x1e2: frames remaining in current attack state */
    u16 secondary_animation_state; /* 0x1e4 */
    u16 saved_animation_id;        /* 0x1e6 */
    u16 saved_script_pos;          /* 0x1e8 */
    u16 animation_wait_bias;       /* 0x1ea */
    u16 animation_frame_bias;      /* 0x1ec */
    u16 animation_loop_count;      /* 0x1ee */
    u16 animation_flags;           /* 0x1f0; toggled by script opcodes 0xec/0xeb */
    u8 _pad1f2[2];                 /* 0x1f2 */
    /* unit_t sprite SHP/SEQ pointers (0x1f4/0x1f8); poach_morbol_transformation
     * rewrites both for the Malboro spritesheet. */
    battle_gfx_unit_shp_frame_tables_t* shp_data;             /* 0x1f4 */
    u8* seq_data;                                             /* 0x1f8 */
    battle_gfx_unit_shp_frame_tables_t* saved_shp_data;       /* 0x1fc; pre-transformation SHP pointer (0x8008363c) */
    u8* saved_seq_data;                                       /* 0x200; pre-transformation SEQ pointer */
    battle_gfx_sprite_display_data_t* sprite_display_section; /* 0x204 */
    battle_unit_sprite_block_t sprite_blocks[3];              /* 0x208..0x297 */
    u8 shadow_graphic_trigger;                                /* 0x298; set_/unset_shadow_graphic_trigger_by_misc_id */
    u8 shadow_dirty;   /* 0x299; bit 0 set when a step lands, cleared on shadow redraw */
    u8 shadow_counter; /* 0x29a; shadow animation counter (battle_gfx_draw_unit_shadow) */
    u8 _pad29b;
    SVECTOR shadow_quad[4]; /* 0x29c; shadow corners written by battle_gfx_calculate_sprite_shadow_from_tile_slope */
    /*
     * Numerical display / status text (0x2bc..0x2d8).
     * prep_for_displaying_earned_exp_jp and activate_numerical_sprite_data test
     * the activation flag, zero the animation progress and select FRAME.BIN in
     * the three numeric battle_gfx_sprite_display_data_t slots.
     */
    u8 numeric_display_active; /* 0x2bc */
    u8 _pad2bd;
    u16 numeric_display_selector;                          /* 0x2be; battle_numeric_display_selector_e */
    u16 numeric_display_value;                             /* 0x2c0; number to display */
    s16 numeric_display_progress;                          /* 0x2c2; animation progress, capped at 0x15 */
    battle_gfx_sprite_display_data_t* numeric_displays[3]; /* 0x2c4, 0x2c8, 0x2cc */
    u8 item_ability_display;                               /* 0x2d0; item ability display (byte store) */
    u8 _pad2d1;                                            /* 0x2d1 */
    s8 item_get_x_offset;                                  /* 0x2d2; setup_item_get_rendering reads (s8) */
    s8 item_get_y_offset;                                  /* 0x2d3 */
    u8 _pad2d4[4];
    battle_gfx_sprite_display_data_t* item_display; /* 0x2d8; item_t/Equip display pointer */
    /* Status bubble (0x2dc..0x2e7);
     * battle_gfx_update_status_bubble_graphic_trigger sets the flag and clears
     * the timer with a halfword store, so the timer is declared u16. */
    u8 status_bubble_active;         /* 0x2dc */
    u8 status_bubble_id;             /* 0x2dd */
    u8 status_bubble_x;              /* 0x2de */
    u8 status_bubble_y;              /* 0x2df */
    u16 status_bubble_timer;         /* 0x2e0 */
    u16 status_bubble_alternate_row; /* 0x2e2; non-zero selects the second texture row */
    void* status_bubble_display;     /* 0x2e4; pointer into the 0x410 status bubble data */
    /* 0x2e8; enables the six-copy position trail seeded by
     * battle_gfx_init_position_vector_copies. */
    u32 position_copies_active;
    /* Six vectors used during sprite rotation/scaling render (0x2ec);
     * battle_gfx_init_position_vector_copies seeds all six from screen_x/z/y. */
    SVECTOR display_svectors[6]; /* 0x2ec..0x31b */
    u8 _unknown_31c[0x124];      /* 0x31c..0x43f: sprite display sections */
} battle_unit_misc_data_t;

typedef char battle_misc_data_size_must_be_0x440[(sizeof(battle_unit_misc_data_t) == 0x440) ? 1 : -1];

typedef char
    battle_misc_step_speed_must_be_0x03c[((unsigned long)&((battle_unit_misc_data_t*)0)->step_speed == 0x03c) ? 1 : -1];

typedef char battle_misc_distortion_target_must_be_0x090
    [((unsigned long)&((battle_unit_misc_data_t*)0)->distortion_target == 0x090) ? 1 : -1];

typedef char battle_misc_animation_frame_must_be_0x1e0
    [((unsigned long)&((battle_unit_misc_data_t*)0)->animation_frame == 0x1e0) ? 1 : -1];

typedef char battle_misc_animation_flags_must_be_0x1f0
    [((unsigned long)&((battle_unit_misc_data_t*)0)->animation_flags == 0x1f0) ? 1 : -1];

typedef char battle_misc_sprite_blocks_must_be_0x208
    [((unsigned long)&((battle_unit_misc_data_t*)0)->sprite_blocks == 0x208) ? 1 : -1];

typedef char
    battle_misc_shadow_must_be_0x299[((unsigned long)&((battle_unit_misc_data_t*)0)->shadow_dirty == 0x299) ? 1 : -1];

typedef char
    battle_misc_command_must_be_0x158[((unsigned long)&((battle_unit_misc_data_t*)0)->command_state == 0x158) ? 1 : -1];

typedef char battle_misc_facing_hint_must_be_0x164
    [((unsigned long)&((battle_unit_misc_data_t*)0)->command_state.cursor.facing_hint == 0x164) ? 1 : -1];

typedef char battle_misc_target_x_must_be_0x168
    [((unsigned long)&((battle_unit_misc_data_t*)0)->command_state.cursor.target_panel == 0x168) ? 1 : -1];

/* Block view of the numeric display fields at misc unit data 0x2bc..0x2cf,
 * taken as one pointer by the action result display builders
 * (battle_gfx_build_next_action_result_display, 0x800808b8, battle_gfx_animate_post_action_text). */
typedef struct battle_action_result_display {
    u8 active;                                     /* 0x00 */
    u8 _pad01;                                     /* 0x01 */
    u16 selector;                                  /* 0x02; battle_numeric_display_selector_e */
    u16 value;                                     /* 0x04 */
    s16 progress;                                  /* 0x06 */
    battle_gfx_sprite_display_data_t* displays[3]; /* 0x08 */
} battle_action_result_display_t;

typedef char battle_action_result_display_size_must_be_0x14[(sizeof(battle_action_result_display_t) == 0x14) ? 1 : -1];

/* Kept while source files migrate to the more specific type name. */

/* In-use words at 0x800b7304, stride 0x440: each sits one word ahead of its
 * g_battle_unit_misc_data record, in the previous record's last word. */
typedef struct battle_unit_misc_slot_flag {
    s32 in_use;
    u8 _unknown_04[0x43c];
} battle_unit_misc_slot_flag_t;

typedef char battle_misc_slot_flag_size_must_be_0x440[(sizeof(battle_unit_misc_slot_flag_t) == 0x440) ? 1 : -1];

/* Provisional: war-trophy search result filled by
 * battle_unit_find_war_trophies_and_bonus_money. REQUIRE links the same routine
 * as battle_unit_find_war_trophies_and_bonus_money into
 * g_require_reward_war_trophy_search_result. */
typedef struct battle_war_result {
    u8 item_ids[BATTLE_UNIT_SLOT_COUNT]; /* 0x00 */
    u8 item_count;                       /* 0x15 */
    u8 _unknown_16[2];                   /* 0x16 */
    s32 bonus_money;                     /* 0x18; sum of bonus-money modifiers * 100 */
    s32 level_sum_money;                 /* 0x1c; g_enemy_level_sum * 100 */
    s32 level_bonus_money;               /* 0x20; (highest enemy level - lowest level) * 100 */
} battle_war_result_t;

typedef char battle_war_result_size_must_be_0x24[(sizeof(battle_war_result_t) == 0x24) ? 1 : -1];

/* Ten-byte halfword block the secondary-effect initializers copy from
 * battle_effect_secondary_init_t into battle_effect_secondary_data_t. */
typedef struct battle_effect_secondary_block {
    s16 values[5];
} battle_effect_secondary_block_t;

/* Secondary-effect target slots at 0x801b8b9c, stride 0x54. The matching phase
 * accessors establish phase at +0x08; battle_effect_update_secondary_effects
 * reads +0x00/+0x03. battle_effect_allocate_secondary_slot links slots through
 * +0x00/+0x01 and clears the 0x20-byte work area at +0x30. The initializers
 * (battle_effect_init_secondary and its projectile/fall-dust variants) fill
 * +0x04..+0x2b. Ending setup at 0x801ae2c8 stores halfwords at +0x04/+0x0c;
 * the golem-family handlers use +0x0c as their counter. Unused regions below
 * are deliberately unnamed. */
typedef struct battle_effect_secondary_data {
    u8 previous_slot_id; /* 0x00 */
    u8 next_slot_id;     /* 0x01 */
    u8 own_slot_id;      /* 0x02; index of this slot in g_battle_effect_secondary_data */
    u8 function_id;      /* 0x03 */
    u16 animation_id;    /* 0x04 */
    s16 parameter;       /* 0x06; caster sprite palette, or the projectile animation's high half */
    s32 phase;           /* 0x08 */
    s16 timer;           /* 0x0c */
    u8 _unknown_0e[2];
    battle_effect_secondary_block_t caster_block; /* 0x10; from the init record's caster block */
    union {
        battle_effect_secondary_block_t block; /* from the init record's target block */
        struct {
            s16 _unknown_1a;
            s16 x; /* 0x1c; fall-dust position, 1/16 of the real coordinate */
            s16 z; /* 0x1e */
            s16 y; /* 0x20 */
        } position;
        struct {
            s16 target_type;   /* 0 identifies a unit target (battle_effect_target_t) */
            u8 target_id;      /* 0x1c; read by the Splash handler 0x801b2a88 */
            u8 target_byte_1d; /* 0x1d; the arrow and projectile handlers compare it with 3 and 4..5 */
        } fields;
    } target;           /* 0x1a */
    s32 used_weapon_id; /* 0x24; init record used_weapon_id */
    s32 target_count;   /* 0x28; init record target_count (projectile setup) */
    /* 0x2c; slots started by battle_effect_start_group, decremented as they
     * finish (0x801b0cf0); handlers finish at 0. */
    s16 active_count;
    s16 finish_timer;  /* 0x2e; counts frames after the last slot finished */
    u8 slot_ids[0x20]; /* 0x30; effect slots started for this effect; cleared when the slot is linked */
    void* allocation;  /* 0x50; freed and cleared when the slot is removed (0x801ad944) */
} battle_effect_secondary_data_t;

typedef char battle_secondary_effect_data_size_must_be_0x54[(sizeof(battle_effect_secondary_data_t) == 0x54) ? 1 : -1];

/* 0xc8-byte stack record filled by battle_effect_build_secondary_init and
 * read by the secondary-effect initializers, which copy its two ten-byte
 * blocks whole. */
typedef struct battle_effect_secondary_init {
    u16 target_count;        /* 0x00 */
    u8 palette_target_count; /* 0x02; entries in palette_target_ids */
    u8 math_skill_flag;      /* 0x03; strike work control_value_19f */
    union {
        struct {
            s16 target_type; /* 0x04; 0 unit, 1 tile (battle_effect_target_t) */
            u8 target_id;    /* 0x06 */
            /* 0x07; 0 normal, 1 critical, 3..5 by action miss_type, 6 break
             * (set per target by battle_effect_build_secondary_init_from_action) */
            u8 result_animation;
        } fields;
        battle_effect_secondary_block_t block;
    } target; /* 0x04; first of up to 16 ten-byte target blocks */
    u8 _unknown_0e[0x96];
    union {
        struct {
            s16 target_type; /* 0xa4; copied whole from g_battle_effect_targets[16] */
            u8 caster_id;    /* 0xa6 */
        } fields;
        battle_effect_secondary_block_t block;
    } caster; /* 0xa4 */
    /* 0xae; living units that are neither targets nor the caster; copied to
     * g_battle_effect_palette_target_misc_ids. */
    u8 palette_target_ids[14];
    battle_effect_target_t target_tile; /* 0xbc; post-action destination tile */
    u16 used_weapon_id;                 /* 0xc6 */
} battle_effect_secondary_init_t;

typedef char battle_secondary_effect_init_size_must_be_0xc8[(sizeof(battle_effect_secondary_init_t) == 0xc8) ? 1 : -1];

/* Four-byte records at 0x801b84dc indexed by animation id; byte 0 selects the
 * g_battle_effect_secondary_handlers handler. */
typedef struct battle_effect_secondary_animation {
    u8 function_id; /* 0x00 */
    u8 _unknown_01; /* 0x01 */
    s16 parameter;  /* 0x02; lh 0x801b84de[id * 4], read by the arrow/projectile handlers */
} battle_effect_secondary_animation_t;

typedef struct battle_unit_height_data {
    u8 x;
    u8 y;
    u8 elevation;
    u8 unit_flags;
    u8 unit_height;
    u8 walking_height;
    u8 total_height;
    u8 unknown_07; /* not written by battle_calculate_unit_height_data */
} battle_unit_height_data_t;

typedef char battle_unit_height_data_size_must_be_8[sizeof(battle_unit_height_data_t) == 8 ? 1 : -1];

typedef struct battle_gfx_render_unit {
    u8 _pad0[0x12];
    u16 sprite_flags;
    u8 _pad14[0x10c];
    SVECTOR camera_relative_position;
    u32 otag_depth;
    u8 _pad12c[0x1a6];
    s8 item_get_x_offset; /* 0x2d2; battle_unit_misc_data_t item_get_x_offset */
    s8 item_get_y_offset; /* 0x2d3 */
    u8 _pad2d4[4];
    battle_gfx_sprite_display_data_t* found_item_display;
    u8 _pad2dc[0x10];
    SVECTOR display_svectors[6];
} battle_gfx_render_unit_t;

/*
 * AI data block, 0x19c8 bytes at 0x8019f3c4 (g_battle_ai_data_base).
 * Unidentified ranges stay explicit padding.  Scalar aliases such as
 * g_battle_ai_acting_unit_decision_ptr name the same bytes.  Sites spell fields
 * as members; an alias remains only where the member spelling lets GCC derive
 * the address from another address in this block (one shared base register)
 * while the target reloads %hi/%lo at that site.
 */

/* X / map elevation / Y / zero, written in that order by transfer_unit_coordinates_to_ai
 * and read as one word for equality tests. */
typedef union battle_ai_coords {
    u32 word;
    struct {
        u8 x;
        u8 elevation;
        u8 y;
        u8 zero;
    } bytes;
} battle_ai_coords_t;

/* Ability AI behaviour flags 1..4. Consumers read both the full word and its
 * constituent bytes; use the corresponding masks above for each view. */
typedef union battle_ai_ability_flags {
    u32 word;
    struct {
        u8 flags_1;
        u8 flags_2;
        u8 flags_3;
        u8 flags_4;
    } bytes;
} battle_ai_ability_flags_t;

/* Shared prefix of the baseline and suspended AI unit snapshots.
 * Byte 2 holds Death Sentence CT only in the compact suspended record;
 * the baseline uses status_ct[15] and leaves this byte untouched. */
typedef struct battle_ai_status_snapshot {
    u8 entd_slot;
    u8 death_counter;
    u8 death_sentence_ct; /* see 0x8019e214 */
    u8 current_status[5];
} battle_ai_status_snapshot_t;

/* Selected unit state saved at 0x8019e160 and restored at 0x8019e378.
 * HP occupies unaligned bytes 0x1b..0x1c; max HP/MP are not saved. */
typedef struct battle_ai_extended_snapshot {
    battle_ai_status_snapshot_t status; /* 0x00 */
    u8 status_ct[16];                   /* 0x08 */
    u8 team_flags;                      /* 0x18 */
    u8 faith;                           /* 0x19 */
    u8 transparent_removal_flag;        /* 0x1a */
    u8 hp_bytes[2];                     /* 0x1b */
    u8 unknown_1d;
    u16 mp;                /* 0x1e */
    u8 charged_ability_ct; /* 0x20 */
    u8 base_attributes[3]; /* 0x21; unit_attribute_index_e */
    u8 attributes[3];      /* 0x24; unit_attribute_index_e */
    u8 ct;                 /* 0x27 */
    u8 x;                  /* 0x28 */
    u8 position_bytes[2];  /* 0x29; packed unit position */
    u8 has_turn;           /* 0x2b */
    u8 movement_taken;
    u8 action_taken;
    u8 auto_battle_setting; /* 0x2e */
    u8 auto_battle_target;
    u8 initial_team_flags;
    u8 inflicted_status[5]; /* 0x31 */
    u8 equipment[7];        /* 0x36 */
    u8 brave;               /* 0x3d */
    u8 mount_info;
    u8 unknown_3f;
} battle_ai_extended_snapshot_t;

typedef char battle_ai_status_snapshot_must_be_8[sizeof(battle_ai_status_snapshot_t) == 8 ? 1 : -1];

typedef char battle_ai_extended_snapshot_must_be_64[sizeof(battle_ai_extended_snapshot_t) == 64 ? 1 : -1];

typedef char battle_ai_snapshot_hp_must_be_0x1b[((unsigned long)&((battle_ai_extended_snapshot_t*)0)->hp_bytes == 0x1b)
        ? 1
        : -1];

typedef char battle_ai_snapshot_equipment_must_be_0x36
    [((unsigned long)&((battle_ai_extended_snapshot_t*)0)->equipment == 0x36) ? 1 : -1];

/* AI ability +0x04..0x0b: byte parameters and an aligned status-tail word.
 * 0x8019e9f4 loads status bytes 1..4 together; reading only one byte would
 * silently lose the high status bits tested by the decision evaluator. */
typedef union battle_ai_ability_parameters {
    struct {
        u8 range;
        u8 aoe;
        u8 item_id;
        u8 status_infliction[5];
    } bytes;
    struct {
        u32 _first;
        u32 status_tail;
    } words;
} battle_ai_ability_parameters_t;

/* Pack canonical status IDs 8..39 into the aligned word covering status
 * bytes 1..4 of battle_ai_ability_parameters_t. */
#define BATTLE_AI_STATUS_TAIL_PACKED_MASK(id)                                                                          \
    ((u32)BATTLE_STATUS_BYTE_MASK(id) << ((BATTLE_STATUS_BYTE_INDEX(id) - 1) * 8))

/* Considered ability, base+0x00. */
typedef struct battle_ai_considered_ability {
    u8 skillset;                               /* 0x00 */
    u8 ct;                                     /* 0x01 */
    s16 ability_id;                            /* 0x02; read with both lh and lhu */
    battle_ai_ability_parameters_t parameters; /* 0x04 */
    battle_ai_ability_flags_t ai_flags;        /* 0x0c */
    u8 element;                                /* 0x10 */
    u8 mp_cost;                                /* 0x11 */
    u8 _pad12[2];
} battle_ai_considered_ability_t;

typedef char battle_ai_considered_ability_size_must_be_0x14[(sizeof(battle_ai_considered_ability_t) == 0x14) ? 1 : -1];

typedef char battle_ai_status_tail_must_be_at_offset_8
    [((unsigned long)&((battle_ai_considered_ability_t*)0)->parameters.words.status_tail == 8) ? 1 : -1];

/*
 * One considered/recorded action, 0x20 bytes.  The current action lives at
 * base+0x14, the best so far at +0x34, eight ranked outcomes at +0x54, the
 * inverted-priority scratch at +0x154 and the selected action at +0x17fc.
 */
typedef struct battle_ai_action_data {
    u8 target_flags_set; /* 0x00; set to 1; name provisional */
    u8 reflected_action; /* 0x01; set while evaluating reflected-origin candidates */
    u8 _pad02;
    u8 wait_facing_hint;       /* 0x03; direction 0..3, 4 keeps current, 5 chooses during Wait */
    battle_ai_coords_t coords; /* 0x04 */
    u8 unit_id;                /* 0x08 */
    u8 skillset;               /* 0x09 */
    u16 ability_id;            /* 0x0a; stored with sh */
    u16 calculator_type;       /* 0x0c */
    u16 calculator_multiplier; /* 0x0e */
    u8 item_id;                /* 0x10 */
    u8 _pad11;
    u8 targeting_type;    /* 0x12; 5 tile-targeted, 6 unit-targeted */
    u8 target_id;         /* 0x13 */
    u16 target_x;         /* 0x14; read with lhu */
    u16 target_elevation; /* 0x16; 0 or 1 map layer */
    u16 target_y;         /* 0x18 */
    u8 _pad1a[2];
    u8 rank_byte;        /* 0x1c; first comparison key at 0x80196db0; meaning unresolved. */
    u8 base_hit_percent; /* 0x1d */
    u16 priority;        /* 0x1e */
} battle_ai_action_data_t;

typedef char battle_ai_action_data_size_must_be_0x20[(sizeof(battle_ai_action_data_t) == 0x20) ? 1 : -1];

/* unit_t AI ability list entry, 4 bytes, 0x22 per unit. */
typedef struct battle_ai_ability_entry {
    union {
        u16 packed_id; /* bits 0..9: ability ID; bits 10..15: physical unit slot */
        struct {
            u8 low;
            u8 unit_and_high;
        } bytes;
    } id;
    /* The AI updates the flag bits with lhu/sh over both bytes, so the
     * halfword view covers skillset and usage_flags together. */
    union {
        u16 packed; /* skillset | usage_flags << 8 */
        struct {
            u8 skillset;    /* 0x02; 0xff terminates the list (chose_move_from_move_list) */
            u8 usage_flags; /* 0x03; battle_ai_ability_entry_flag_e */
        } bytes;
    } skillset_flags;
} battle_ai_ability_entry_t;

typedef char battle_ai_ability_entry_must_be_4[sizeof(battle_ai_ability_entry_t) == 4 ? 1 : -1];

/* Per-compact-unit weapon summary. The builder at 0x8019a2f0 clears two
 * words, then merges weapon properties through the byte view. */
typedef union battle_ai_weapon_data {
    u32 words[2];
    struct {
        u8 range;
        u8 flags_1;
        u8 flags_2;
        u8 flags_3;
        u8 weapon_id;
        u8 element;
        u8 field_06[2];
    } bytes;
} battle_ai_weapon_data_t;

typedef char battle_ai_weapon_data_must_be_8[sizeof(battle_ai_weapon_data_t) == 8 ? 1 : -1];

/* Per-unit AI behavior flags copied from ENTD data to decision +0x04. The
 * three transient status bits are rebuilt during action simulation. Bit 0x80
 * is deliberately unnamed: refresh and simulation give it overlapping uses. */
typedef enum battle_ai_decision_flags {
    BATTLE_AI_DECISION_TRANSIENT_DEAD = 0x01,
    BATTLE_AI_DECISION_TRANSIENT_PETRIFIED = 0x02,
    BATTLE_AI_DECISION_TRANSIENT_JUMP = 0x04,
    BATTLE_AI_DECISION_SPECIAL_BEHAVIOR = 0x08,
    BATTLE_AI_DECISION_AGGRESSIVE = 0x10,
    BATTLE_AI_DECISION_STAY_NEAR_COORDINATES = 0x20,
    BATTLE_AI_DECISION_FOCUS_TARGET = 0x40,
} battle_ai_decision_flags_e;

/* Targeting policy copied from ENTD AI Flags 2 to decision +0x07. The low
 * policy bits survive battle_ai_refresh_unit_decision_flags; the remaining
 * target-state bits are rebuilt there before action simulation. */
typedef enum battle_ai_targeting_flags_2 {
    BATTLE_AI_TARGET_HP_BELOW_HALF = 0x01,
    BATTLE_AI_TARGET_MP_CONSTRAINED = 0x02,
    BATTLE_AI_TARGET_CONSERVE_CT = 0x04,
    BATTLE_AI_TARGET_TEAM_SUPPORT_UNAVAILABLE = 0x08,
    BATTLE_AI_TARGET_UNTARGETABLE = 0x10,
    BATTLE_AI_TARGET_DEAD_WITHOUT_RERAISE = 0x20,
    BATTLE_AI_TARGET_DEAD_WITH_RERAISE = 0x40,
    BATTLE_AI_TARGET_HP_BELOW_THREE_QUARTERS = 0x80,
} battle_ai_targeting_flags_2_e;

typedef enum battle_ai_target_setting_flags {
    BATTLE_AI_TARGET_SETTING_REFRESH_UNIT_STATUS_FLAGS = 0x01000000,
    BATTLE_AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS = 0x02000000,
    BATTLE_AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE = 0x40000000,
} battle_ai_target_setting_flags_e;

/*
 * Per-unit AI decision record, 0x10 bytes at base+0x182c (unit*16 + 0x182c).
 * g_battle_ai_acting_unit_decision_ptr points at the acting unit's record.
 */
typedef struct battle_ai_unit_decision {
    battle_ai_coords_t target; /* 0x00..0x03; x, level, y, 0 */
    u8 flags;                  /* 0x04; battle_ai_decision_flags_e */
    u8 main_target_id;         /* 0x05 */
    u8 targeting_flags_1; /* 0x06; 0x08 selected reflected action; 0x04 helpful-ability ratio >= 77/128, cleared for
                             the lowest-ratio nonenemy (0x8019537c..0x80195408) */
    u8 targeting_flags_2; /* 0x07; battle_ai_targeting_flags_2_e */
    u8 enemy_flag;        /* 0x08 */
    u8 highest_mp_cost;   /* 0x09 */
    u8 lowest_mp_cost;    /* 0x0a */
    u8 mp_ability_mod;    /* 0x0b */
    u8 silence_mod;       /* 0x0c */
    u8 evade_mod;         /* 0x0d */
    u8 lowest_range;      /* 0x0e; plus unit move */
    u8 highest_range;     /* 0x0f; plus unit move */
} battle_ai_unit_decision_t;

typedef char battle_ai_unit_decision_size_must_be_0x10[(sizeof(battle_ai_unit_decision_t) == 0x10) ? 1 : -1];

/* AI +0x0c78..0x0cb7. The target's 21-byte save at +0xca2 overlaps
 * the low three bytes of target_setting_flags at +0xcb4. Loops at
 * 0x8019b184..0x8019b1b8 and 0x8019b260..0x8019b274 establish the span.
 * The CONSUME_EVALUATED_ORIGINS flag lies in the fourth byte, outside the saved array.
 * Preserve both views and word alignment; this is not a 16-byte backup. */
typedef union battle_ai_targetability {
    struct {
        u8 unit_active[BATTLE_UNIT_SLOT_COUNT];
        u8 unit_targetable[BATTLE_UNIT_SLOT_COUNT];
        u8 saved_prefix[16];
        u8 _pad3a[2];
        u32 target_setting_flags;
    } live;
    struct {
        u8 _pad00[42];
        u8 unit_targetable_saved[BATTLE_UNIT_SLOT_COUNT];
        u8 _pad3f;
    } snapshot;
} battle_ai_targetability_t;

typedef char battle_ai_targetability_size_must_be_64[(sizeof(battle_ai_targetability_t) == 64) ? 1 : -1];

/* Five-byte range/targeting work entry at 0x80192dd8, 256 entries per level.
 * Movement-cost propagation uses remaining_range as the residual budget;
 * the AI reads it at 0x801995f4 and 0x801996ac. */
typedef struct battle_target_panel {
    u8 remaining_range;
    /* Targeting frontier/targeted mark; pathing stores the step number here. */
    u8 mark;
    /* Index 0..15 of the compact movement record on the tile (0x80174e84),
     * not a battle id. */
    u8 unit_record_index;
    /* Remaining range after landing on a rideable unit (0x80175fa4); cleared on
     * the acting tile by 0x80178dac; pathing reuses it on 512+ panels as the step number. */
    u8 ride_remaining_range;
    /* Largest single height step along the path, the equal-budget tie-breaker
     * (0x80175958, 0x80177794); 0xff initial value in mode 1. AI propagation
     * sets the target to 0 and old actor tile to 0xff. */
    u8 max_height_delta;
} battle_target_panel_t;

typedef char battle_targeting_panel_size_must_be_5[(sizeof(battle_target_panel_t) == 5) ? 1 : -1];

typedef struct battle_ai_data {
    battle_ai_considered_ability_t considered_ability; /* 0x0000 */
    battle_ai_action_data_t current_action;            /* 0x0014 */
    battle_ai_action_data_t best_action;               /* 0x0034 */
    battle_ai_action_data_t ranked_actions[8];         /* 0x0054 */
    battle_ai_action_data_t inverted_priority_action;  /* 0x0154 */
    u8 tile_foe_proximity[2][18][16];                  /* 0x0174; level/y/x, strides
                                                        * 288/16/1 at 0x80196e68..0x80196ebc */
    u8 tile_target_distance[2][18][16];                /* 0x03b4; level/y/x, strides 288/16/1 */
    u16 tile_priority[2][18][16];                      /* 0x05f4; level/y/x, byte strides 576/32/2 */
    /* Scenario/level/y; row bits 15..0 represent x=0..15. */
    u16 reachable_tiles[3][2][18]; /* 0x0a74; byte strides 72/36/2 */
    u16 targetable_tiles[0x24];    /* 0x0b4c */
    u16 movable_tiles[0x24];       /* 0x0b94 */
    u16 attack_origin_tiles[0x24]; /* 0x0bdc */
    u16 walkable_tiles[0x24];      /* 0x0c24 */
    u8 _pad0c6c;
    u8 initial_targeting_state; /* 0x0c6d */
    u8 action_selection_phase;  /* 0x0c6e; 0x801971b8 selects scorer/distance resume; 0x80197ff4 uses 0..4 */
    u8 _pad0c6f;
    u8 outcome_evaluation_state; /* 0x0c70 */
    u8 movement_decision_state;  /* 0x0c71 */
    u8 search_resume_flag;       /* 0x0c72; distance-search initialization/resume flag
                                  * at 0x801994f8; reused as a phase flag at 0x8019cd9c. */
    u8 _pad0c73;
    battle_ai_coords_t considered_unit_coords; /* 0x0c74 */
    battle_ai_targetability_t targetability;   /* 0x0c78 */
    /* Included target/strike pairs, not unique units; 0xff disables counting. */
    u16 total_hit_percent;            /* 0x0cb8 */
    u8 hit_counter;                   /* 0x0cba */
    u8 attack_origin_tile_count;      /* 0x0cbb */
    u8 movable_tile_count;            /* 0x0cbc */
    u8 ability_counter;               /* 0x0cbd */
    u8 ability_targets_enemies_or_mp; /* 0x0cbe; provisional name: any usable ability has (flags & 0x42) == 0x40 */
    u8 crystal_treasure_status;       /* 0x0cbf; 1 crystal, 2 treasure */
    u8 _pad0cc0[4];
    battle_ai_coords_t candidate_coords[3]; /* 0x0cc4; indexed by movement_scenario */
    u8 _pad0cd0[4];
    u8 unit_action_records[16][0x14]; /* 0x0cd4 */
    u16 throw_ability_id;             /* 0x0e14 */
    u8 throw_weapon_id;               /* 0x0e16 */
    /* All slots with entd_slot != 0xff, capped at 255 by 0x80199d20. */
    u8 present_unit_average_max_hp;                  /* 0x0e17 */
    u8 unit_acts_before_me[BATTLE_UNIT_SLOT_COUNT];  /* 0x0e18..0x0e2c; all slots cleared at 0x8019d408–0x8019d41c */
    u8 movement_scenario;                            /* 0x0e2d; 0..2 */
    u8 acting_unit_id;                               /* 0x0e2e */
    u8 acting_unit_battle_id;                        /* 0x0e2f */
    battle_ai_coords_t acting_unit_coords;           /* 0x0e30 */
    battle_ai_unit_decision_t* acting_unit_decision; /* 0x0e34 */
    u8 acting_unit_move;                             /* 0x0e38 */
    u8 acting_unit_team;                             /* 0x0e39 */
    u8 map_max_x;                                    /* 0x0e3a */
    u8 map_max_y;                                    /* 0x0e3b */
    union {
        u8 bytes[8];
        u16 by_team[4];
    } team_golem;           /* 0x0e3c; halfword scoring at 0x8019d37c */
    u8 status_to_cancel[5]; /* 0x0e44; usable actor abilities' aggregate cancellation mask, built at 0x80195410 */
    /* Resumable reflected-origin search, 0x8019b7b8–0x8019bb20.
     * Candidate x/y = 2 * reflector position - intended target position. */
    u8 reflector_unit_index;             /* 0x0e49 */
    u8 reflected_target_unit_index;      /* 0x0e4a */
    u8 reflected_candidate_level;        /* 0x0e4b */
    u8 ability_effect_on_self;           /* 0x0e4c; 0 none, 1 usable, 2 adverse */
    u8 weapon_range_flag;                /* 0x0e4d */
    u8 useful_on_caster_flag;            /* 0x0e4e */
    u8 acting_unit_remaining_clockticks; /* 0x0e4f */
    u8 unit_status_records[16][8];       /* 0x0e50 */
    u8 saved_ability_ct;                 /* 0x0ed0 */
    u8 saved_current_status;             /* 0x0ed1 */
    u8 saved_inflicted_status;           /* 0x0ed2 */
    u8 _pad0ed3;
    u8 main_ai_state;             /* 0x0ed4 */
    u8 autobattle_state;          /* 0x0ed5 */
    u8 charging_state;            /* 0x0ed6 */
    u8 find_highest_target_state; /* 0x0ed7; check_if_map_allows_use_and_find_highest_target's progress variable */
    u8 reflected_origin_phase;    /* 0x0ed8; cleared at 0x8019ba40, set at 0x8019ba78 */
    u8 highest_priority_state;    /* 0x0ed9 */
    u8 max_possibilities;         /* 0x0eda; byte-truncated combination count, then scan x at 0x8019bf2c */
    u8 y_counter;                 /* 0x0edb */
    u8 map_level_counter;         /* 0x0edc */
    /* Calculator search counters survive suspension at 0x8019bbbc–0x8019bf2c. */
    u8 math_ability_id;       /* 0x0edd; low byte of the selected ability entry */
    u8 math_type_index;       /* 0x0ede; 0..3 */
    u8 math_multiplier_index; /* 0x0edf; 4..7 */
    /* Turn-controller globals saved/restored by 0x8019d37c–0x8019db80. */
    s32 saved_turn_state_0;    /* 0x0ee0; snapshot of 0x8018f518 */
    s32 saved_turn_state_1;    /* 0x0ee4; snapshot of 0x8018f51c */
    s32 saved_turn_state_2;    /* 0x0ee8; snapshot of 0x8018f520 */
    u16 simulated_turn_events; /* 0x0eec; interturn loop stops at 256 */
    u8 progress_0eee;          /* 0x0eee */
    u8 _pad0eef;
    battle_ai_ability_entry_t ability_lists[16][0x22]; /* 0x0ef0; 0x88 bytes per unit */
    /* Blood Suck/Frog IDs start without unit bits; 0x8019729c/0x80197510
     * OR the acting physical slot into bits 10..15 before loading the entry. */
    battle_ai_ability_entry_t hardcoded_status_abilities[2]; /* 0x1770 */
    battle_ai_weapon_data_t unit_weapon_data[16];            /* 0x1778 */
    battle_stats_t* acting_unit;                             /* 0x17f8 */
    battle_ai_action_data_t selected_action;                 /* 0x17fc */
    /* 0x8019aea0 compares/copies coordinate words; 0x80194da4 consumes
     * x/elevation/y to clear reachable tiles. The list's broader policy is unknown. */
    battle_ai_coords_t coords_181c[4];                                /* 0x181c; x == 0xff terminates the list */
    battle_ai_unit_decision_t unit_decisions[BATTLE_UNIT_SLOT_COUNT]; /* 0x182c */
    /* 0x80195ed4 saves the complete decision before retreat; 0x80195efc
     * restores it only after the child finishes, not when it suspends. */
    battle_ai_unit_decision_t saved_acting_unit_decision; /* 0x197c */
    u8 unit_battle_ids[BATTLE_UNIT_SLOT_COUNT];           /* 0x198c */
    u8 unit_behaviour[BATTLE_UNIT_SLOT_COUNT];            /* 0x19a1; 0x11 coward, 0x0e/0x0c aggressive */
    u8 autobattle_setting;                                /* 0x19b6 */
    u8 decision_state;                                    /* 0x19b7 */
    u8 random_use_threshold;                              /* 0x19b8; 0x80195778..0x80195788 stores
                                                           * floor(128 * random-use entries / eligible entries).
                                                           * A zero denominator leaves the prior byte unchanged. */
    u8 water_penalty;                                     /* 0x19b9 */
    u8 valuable_target_hit;                               /* 0x19ba; a counted hit on unit_targetable sets this
                                                           * at 0x8019e010..0x8019e020. */
    u8 targetable_override;                               /* 0x19bb */
    u8 ability_processing_done;                           /* 0x19bc */
    u8 action_taken;                                      /* 0x19bd */
    u8 movement_taken;                                    /* 0x19be */
    u8 _pad19bf[8];
    u8 field_19c7; /* 0x19c7; last documented byte */
} battle_ai_data_t;

typedef char battle_ai_data_size_must_be_0x19c8[(sizeof(battle_ai_data_t) == 0x19c8) ? 1 : -1];

typedef char battle_ai_saved_targetability_must_be_0xca2
    [((unsigned long)&((battle_ai_data_t*)0)->targetability.snapshot.unit_targetable_saved == 0xca2) ? 1 : -1];

typedef char battle_ai_target_flags_must_be_0xcb4
    [((unsigned long)&((battle_ai_data_t*)0)->targetability.live.target_setting_flags == 0xcb4) ? 1 : -1];

/* Acting-unit scratch block at 0x1f800000, pointer at 0x8018f4e0.
 * Only target-proven bytes are named. */
typedef struct battle_move_pathfind_scratch {
    u8 _pad00[2];
    u8 jump_times_two; /* 0x02; 0x3e when flying / ignoring height */
    u8 _pad03;
    u8 jump_or_1f;     /* 0x04 */
    u8 jump_half;      /* 0x05 */
    u8 move;           /* 0x06 */
    u8 x;              /* 0x07; battle_stats_t 0x47 */
    u8 y;              /* 0x08; battle_stats_t 0x48 */
    u8 high_elevation; /* 0x09 */
    u8 target_x;       /* 0x0a */
    u8 target_y;       /* 0x0b */
    u8 _pad0c;
    u8 unit_id;              /* 0x0d; mount id when riding */
    u8 move_type;            /* 0x0e; battle_move_class_e */
    u8 move_mod;             /* 0x0f */
    u8 movement_3;           /* 0x10 */
    u8 cannot_enter_water;   /* 0x11 */
    u8 will_drown;           /* 0x12 */
    u8 will_sink;            /* 0x13 */
    u8 cannot_stay_on_water; /* 0x14 */
    u8 mountable_chocobo;    /* 0x15 */
    u8 can_ride;             /* 0x16 */
    u8 _pad17;
    u8 map_max_x;       /* 0x18 */
    u8 map_max_y;       /* 0x19 */
    u8 field_1a;        /* 0x1a; byte at 0x8018f4fc */
    u8 unit_size;       /* 0x1b */
    u8 movement_set_3;  /* 0x1c */
    u8 fly_or_teleport; /* 0x1d */
    u8 _pad1e[4];
    u8 stepping_stone;      /* 0x22 */
    u8 ai_propagation_mode; /* 0x23; set only by AI target propagation (0x80178224 clears it) */
    u8 movement_set_2;      /* 0x24 */
    u8 can_pass_lava;       /* 0x25 */
} battle_move_pathfind_scratch_t;

/* One 17-byte per-unit menu record, one per battle-unit slot.
 * battle_menu_reset_unit_records (0x8013f8b4) proves the 17-byte stride and
 * 21 entries and marks byte 1 unset (0xFF); battle_menu_restore_selection_for_selected_unit and
 * battle_menu_record_selection_for_selected_unit (0x8014088c) treat byte 0 as
 * the unit's remembered selection for a tracked menu. */
typedef struct battle_menu_record {
    u8 bytes[17];
} battle_menu_record_t;

typedef char battle_menu_record_size_must_be_0x11[(sizeof(battle_menu_record_t) == 0x11) ? 1 : -1];

/* One row of the 31-entry menu table at g_battle_menu_id_records; its first 12 rows are the
 * menus whose selection is remembered per unit.
 * battle_menu_restore_selection_for_selected_unit and battle_menu_record_selection_for_selected_unit
 * prove the 8-byte stride and read only the leading menu id;
 * battle_menu_resolve_selection walks all 31 rows. */
typedef struct battle_menu_id_record {
    u16 menu_id;
    u16 pad;
    /* 0x04: per-menu command remap table. map[0] handles the 0xFF selection
     * and map[n + 1] any other value; named by battle_menu_resolve_selection,
     * its only reader. The other readers use menu_id alone. */
    u8* map;
} battle_menu_id_record_t;

typedef char battle_menu_id_record_size_must_be_0x8[(sizeof(battle_menu_id_record_t) == 8) ? 1 : -1];

/* One five-byte targeting panel entry, 0x200 of them, indexed in parallel with
 * the map tile data. Nine routines walk the table with `+= 5` byte arithmetic. */
typedef struct targeting_panel_entry {
    s8 a;
    s8 b;
    s8 c[3];
} targeting_panel_entry_t;

typedef char targeting_panel_entry_size_must_be_5[(sizeof(targeting_panel_entry_t) == 5) ? 1 : -1];

/* Eight 0x0a-byte map texture-animation runtime slots at 0x800911f4, distinct
 * from the 32 mesh texture-animation instructions and their handle table.
 * The slot constants and the serialized/runtime distinction are defined with
 * the map format declarations in map.h. */
typedef struct map_texture_animation_state {
    u8 active;
    u8 elapsed_frames;
    u8 frame_duration;
    u8 padding_03;
    u16 polygon_group;
    u16 first_polygon;
    u16 last_polygon;
} map_texture_animation_state_t;

typedef char map_texture_animation_state_size_must_be_0xa[(sizeof(map_texture_animation_state_t) == 0xa) ? 1 : -1];

/* Per-unit palette bookkeeping at 0x8009b27c, indexed by Misc Unit Data ID
 * with an eight-byte stride; only the leading gate byte is used. */
typedef struct battle_palette_update_entry {
    u8 pending; /* 0x00 */
    u8 pad01[7];
} battle_palette_update_entry_t;

/* The menu threads' view of one 0x3c-byte menu record (world_menu_entry_t).
 * BATTLE and event-overlay menu threads receive an entry of
 * g_battle_menu_thread_menu_data as parameter 1 (battle_menu_start_mini_menu_display_thread
 * passes &g_battle_menu_thread_menu_data[10]) and read 0x0a as flags and
 * 0x30 as their redraw-flag pointer, where WORLD's record keeps window_y and
 * text_binding; other fields use world_menu_entry_t's names.
 * battle_menu_build_idle_action_menu (0x8013cf58) proves the stride and stores
 * window_x at 0x08 (0x80 plus a per-entry byte picked from the unit's screen
 * position). The halfword at 0x38 holds the entry's current selection
 * (menu struct 0x38): battle_menu_restore_selection_for_selected_unit and
 * battle_menu_record_selection_for_selected_unit move it to and from
 * battle_menu_record_t byte 0, battle_menu_confirm_auto_battle_selection_thread
 * (0x801416a0) reads it, and battle_menu_clear_idle_action_menu_entry_flags
 * (0x8013f76c) clears it. */
typedef struct battle_menu_idle_action_entry {
    u8 unknown_00[4];
    s16 inner_width;  /* 0x04; also stored to g_menu_inner_window_width */
    s16 inner_height; /* 0x06 */
    s16 window_x;     /* 0x08 */
    u16 flags_0a;     /* 0x0a; the menu threads clear bit 0 on entry */
    u8 unknown_0c[2];
    s16 window_height; /* 0x0e; set with inner_height by battle_menu_build_skillset_entries */
    u8 unknown_10[6];
    s16 overall_height; /* 0x16; set with inner_height by battle_menu_build_skillset_entries */
    u8 unknown_18[4];
    s16 text_id;     /* 0x1c; text entry drawn into the window */
    s16 last_option; /* 0x1e; highest option index (battle_menu_update_selection_from_input) */
    u16 value_20;    /* 0x20; set from the system-function table by battle_menu_run_system_function_thread */
    u8 unknown_22[6];
    void (*thread_entry)(void); /* 0x28; menu thread started by battle_menu_start_entry_thread */
    u8 unknown_2c[4];
    s16* refresh_flag; /* 0x30; the menu threads redraw while *refresh_flag == 1 */
    u8 unknown_34[4];
    s16 selected_index; /* 0x38 */
    u8 unknown_3a[2];
} battle_menu_idle_action_entry_t;

typedef char battle_idle_action_menu_entry_refresh_must_be_0x30
    [((unsigned long)&((battle_menu_idle_action_entry_t*)0)->refresh_flag == 0x30) ? 1 : -1];

typedef char
    battle_idle_action_menu_entry_size_must_be_0x3c[(sizeof(battle_menu_idle_action_entry_t) == 0x3c) ? 1 : -1];

/* Provisional 0x14-byte system-function record, table at 0x801692c0
 * (battle_menu_run_system_function_thread, battle_menu_dispatch_system_function). */
typedef struct battle_system_function {
    s16 value_00;         /* 0x00; copied to g_battle_menu_current_id */
    u16 text_id;          /* 0x02; menu entry text id */
    s16 menu_entry_index; /* 0x04; g_battle_menu_thread_menu_data index */
    u16 value_06;         /* 0x06; copied to g_battle_menu_system_function_row_actions */
    u16 value_08;         /* 0x08; menu entry value_20 */
    u8 _pad0a[2];
    void (*thread_entry)(void); /* 0x0c; started as thread 8 */
    s16 alternate_id;           /* 0x10; replaces the id when navigation messages are not On */
    u8 _pad12[2];
} battle_system_function_t;

typedef char battle_system_function_size_must_be_0x14[(sizeof(battle_system_function_t) == 0x14) ? 1 : -1];

/* Array view: as a scalar, GCC hoists the menu-index reload above its store
 * in battle_menu_run_system_function_thread. */
extern battle_system_function_t g_battle_system_function_table[];

/* Menu allocator slot (16 at g_battle_menu_allocator_slot_records); buffer is
 * -1 when the slot is free. The slots hand out runs of the 53 256-byte pages
 * of g_battle_menu_buffer_arena. */
typedef struct battle_menu_allocation {
    u8* buffer;     /* 0x00 */
    s32 page_count; /* 0x04 */
} battle_menu_allocation_t;

typedef char battle_menu_allocation_size_must_be_8[(sizeof(battle_menu_allocation_t) == 8) ? 1 : -1];

extern battle_menu_allocation_t g_battle_menu_allocator_slot_records[16];
extern u8 g_battle_menu_buffer_arena[53][0x100];

/* One 12-byte entry of a menu number table drawn by
 * battle_menu_draw_numeric_display_entries; kind 0 and 1 select the plain
 * and coloured number renderers. */
typedef struct menu_number_entry {
    u16 x;      /* 0x00 */
    u16 y;      /* 0x02 */
    s16* value; /* 0x04 */
    s16 digits; /* 0x08; digit count, 0x400/0x800/0xc00/0x1000 sign flags */
    s16 kind;   /* 0x0a */
} menu_number_entry_t;

/* Provisional: one gauge of the status record. */
typedef struct battle_unit_status_gauge {
    s16 value;  /* 0x00 */
    s16 value2; /* 0x02 */
    s16 max;    /* 0x04 */
} battle_unit_status_gauge_t;

enum {
    BATTLE_UNIT_STATUS_GAUGE_HP = 0,
    BATTLE_UNIT_STATUS_GAUGE_MP = 1,
    BATTLE_UNIT_STATUS_GAUGE_CT = 2,
};

/* Provisional 0x22-byte unit status record: the AT banner at 0x8014d038 and the
 * status billboard at 0x8014d05c. battle_menu_copy_unit_data_to_status_billboard
 * (and its DEBUGCHR twin) fills the first 0x1e bytes,
 * battle_menu_display_hovered_unit_stats draws it, and
 * battle_action_copy_at_and_cursor_to saves the banner whole as 0x22 bytes. */
typedef struct battle_unit_status_record {
    s16 level;                            /* 0x00 */
    s16 team_kind;                        /* 0x02: 0-3 from team flags, 3 = auto-battle; selects the status icon */
    s16 list_index;                       /* 0x04: AT-list position + 1, negative when the unit is not listed */
    s16 unit_count;                       /* 0x06 */
    s16 experience;                       /* 0x08 */
    s16 battle_id;                        /* 0x0a */
    battle_unit_status_gauge_t gauges[3]; /* 0x0c: BATTLE_UNIT_STATUS_GAUGE_*; CT max is always 100 */
    s16 projected_display_value;          /* 0x1e */
    u16 projected_hit_percent;            /* 0x20 */
} battle_unit_status_record_t;

typedef char battle_unit_status_record_size_must_be_0x22[(sizeof(battle_unit_status_record_t) == 0x22) ? 1 : -1];

/* Provisional 0xe-byte active-unit record at 0x8014d080, saved and restored
 * whole by battle_action_copy_at_and_cursor_to and
 * battle_action_copy_active_turn_data_from.
 * battle_menu_store_units_small_in_battle_display_data fills job_id..zodiac
 * for the unit named by battle_id. */
typedef struct battle_active_unit {
    s16 unknown_00; /* 0x00 */
    s16 job_id;     /* 0x02 */
    s16 brave;      /* 0x04 */
    s16 faith;      /* 0x06 */
    s16 zodiac;     /* 0x08 */
    s16 battle_id;  /* 0x0a: selected unit */
    s16 unknown_0c; /* 0x0c */
} battle_active_unit_t;

typedef char battle_active_unit_size_must_be_0xe[(sizeof(battle_active_unit_t) == 0xe) ? 1 : -1];

void battle_menu_display_hovered_unit_stats(
    RECT* frame_rect, s32 mode, menu_number_entry_t* entries, battle_unit_status_record_t* status);

typedef char menu_number_entry_size_must_be_12[(sizeof(menu_number_entry_t) == 12) ? 1 : -1];

/* Text origin rewritten from each menu_number_entry_t before it is drawn. */
typedef struct menu_number_position {
    u16 x; /* 0x00 */
    u16 y; /* 0x02 */
} menu_number_position_t;

typedef struct battle_ability_animation_data {
    u8 charge_animation_set_id;
    u8 attack_animation;
    u8 text_display; /* External table evidence; no reconstructed consumer yet. */
} battle_ability_animation_data_t;

typedef char battle_ability_animation_data_size_must_be_3[(sizeof(battle_ability_animation_data_t) == 3) ? 1 : -1];

enum {
    BATTLE_ABILITY_ANIMATION_COUNT = 0x1c6,
    BATTLE_CHARGE_ANIMATION_SET_COUNT = 20,
    BATTLE_CHARGE_ANIMATION_VARIANT_COUNT = 2,
    BATTLE_ATTACK_ANIMATION_USE_WEAPON = 0,
    BATTLE_ATTACK_ANIMATION_USE_ITEM = 1,
};

extern battle_ability_animation_data_t g_battle_ability_animation_data[BATTLE_ABILITY_ANIMATION_COUNT];
extern u8 g_battle_ability_charge_animation_sets[BATTLE_CHARGE_ANIMATION_SET_COUNT]
                                                [BATTLE_CHARGE_ANIMATION_VARIANT_COUNT];
extern s16 g_ability_effect_id;
extern u8 g_acting_unit_elevation;
extern u8 g_acting_unit_x;
extern u8 g_acting_unit_y;
extern s32 g_action_type;
extern s32 g_battle_acting_unit_id;
extern u8 g_battle_acting_unit_id_byte;
extern s32 g_battle_action_at_list_active;
extern s32 g_battle_action_at_list_id;
extern const battle_action_display_image_rect_t g_battle_action_display_image_rects[BATTLE_ACTION_DISPLAY_IMAGE_COUNT];
extern const u8 g_battle_action_display_numeric_selectors[16];
extern u16 g_battle_action_display_values[16];
extern const u8 g_battle_action_status_display_image_ids[BATTLE_STATUS_COUNT];
extern const u16 g_battle_action_result_palette[16];
extern const u32 g_battle_misc_status_mask_by_handler_index[BATTLE_STATUS_COUNT + 1];
extern s32 g_battle_action_phase;
extern s32 g_battle_action_post_action;
extern s32 g_battle_action_post_action_display_phase;
extern const u8 g_battle_status_display_image_ids[BATTLE_STATUS_COUNT];
extern s32 g_battle_action_post_effect_msg_counter;
extern battle_action_data_t* g_battle_action_attacker_data;
extern battle_action_state_e g_battle_action_state;
extern battle_action_data_t* g_battle_action_target_data;
extern battle_action_context_e g_battle_action_context;
extern s16 g_battle_current_reaction_ability_id;
extern s32 g_battle_distribute_target_count;
extern battle_active_unit_t g_battle_active_turn_unit;
extern u16 g_battle_camera_shake_z_offset;
extern s32 g_battle_casting_misc_id;
extern s32 g_battle_casting_unit_id;
extern s32 g_battle_cursor_x;
extern s32 g_battle_cursor_y;
extern s32 g_battle_cursor_z;
extern s32 g_battle_destination_camera_yaw;
extern battle_effect_secondary_animation_t g_battle_effect_secondary_animations[];
extern battle_effect_secondary_data_t g_battle_effect_secondary_data[];
extern battle_palette_update_entry_t g_battle_gfx_palette_update_flags[];
extern u16 g_battle_gfx_screen_color_modulation_fade_frames;
extern u8 g_battle_job_level_gained_flag;
extern u8 g_battle_map_background_color[];
extern u8 g_battle_map_background_color_backup[];
extern void* g_battle_map_data_load_buffer;
extern s32 g_battle_map_data_load_complete;
extern s32 g_battle_map_deep_dungeon_load_in_progress;
extern u8 g_battle_map_file_table[];
extern u8 g_battle_map_first_gns_resource_type;
extern u32 g_battle_map_gns_lba[];
extern s32 g_battle_map_gns_record_cursor;
extern gns_file_record_t g_battle_map_gns_records[];
extern s32 g_battle_map_id;
extern u8 g_battle_map_max_x;
extern u8 g_battle_map_max_y;
extern s32 g_battle_map_mesh_data_buffer;
extern void* g_battle_map_mesh_load_buffer;
extern s32 g_battle_map_mesh_load_state;
extern map_move_find_item_data_t g_battle_current_map_move_find_item_data;
/* Contiguous result bytes returned as battle_move_find_result_data_t. Keep
 * distinct symbols: the target reloads their absolute addresses separately. */
extern u8 g_battle_move_find_result_flags;
extern u8 g_battle_move_find_entry_index;
extern u8 g_battle_move_find_rare_item_id;
extern u8 g_battle_move_find_common_item_id;
extern u8 g_battle_move_find_trap_id;
extern s8 g_battle_move_find_occupying_unit_id;
extern map_move_find_item_data_t g_battle_map_move_find_item_data[];
extern u16 g_battle_map_palette_modified_colors[][0x100];
extern s32 g_battle_map_resource_load_state;
extern s32 g_battle_map_texture_animation_handles[];
extern map_texture_animation_state_t g_battle_map_texture_animation_states[];
extern s32 g_battle_map_textured_quad_count;
extern s32 g_battle_map_textured_quad_render_record_count;
extern s32 g_battle_map_textured_triangle_count;
extern s32 g_battle_map_textured_triangle_render_record_count;
extern s32 g_battle_map_tilt_target;
extern s32 g_battle_map_untextured_quad_count;
extern s32 g_battle_map_untextured_quad_render_record_count;
extern s32 g_battle_map_untextured_triangle_count;
extern s32 g_battle_map_untextured_triangle_render_record_count;
extern s32 g_battle_map_zoom_target;
extern u8 g_battle_math_source_skillsets[4];
extern u8 g_battle_menu_buffer_allocation_map[53];
extern s32 g_battle_menu_companion_executable_loading;
extern s32 g_battle_menu_companion_executable_request_index;
extern s32 g_battle_menu_current_id;
extern s32 g_battle_menu_help_open;
extern s32 g_battle_menu_help_opening;
extern s16 g_battle_menu_hovered_unit_display_mode;
extern RECT g_battle_menu_hovered_unit_stats_display;
extern world_menu_entry_t g_battle_menu_idle_action_entries[];
extern u16 g_battle_menu_input_disabled;
extern u8 g_battle_menu_numeric_display_frame_offsets[12][8];
extern u8 g_battle_menu_numeric_display_palette_indices[12];
extern RECT g_battle_menu_numeric_display_texture_window;
extern s32 g_battle_menu_packet_buffer_index;
extern u8 g_battle_menu_palette_colors[];
extern s32 g_battle_menu_panel_fade_intensity;
extern s32 g_battle_menu_panel_fade_mode;
extern RECT g_battle_menu_portrait_palette_rect;
extern RECT g_battle_menu_portrait_pixel_rect;
extern u8 g_battle_menu_restore_pending;
extern s16 g_battle_menu_right_unit_display_mode;
extern RECT g_battle_menu_right_unit_stats_display;
extern s32 g_battle_menu_secondary_companion_executable_request_index;
extern s32 g_battle_menu_selected_skill_target;
extern s32 g_battle_menu_slot_owner_thread_ids[3];
extern s32 g_battle_menu_slot_states[3];
extern s32 g_battle_menu_status_requested;
extern s32 g_battle_menu_status_screen_selected;
extern s32 g_battle_status_menu_open;
extern u16 g_battle_menu_transition_state;
extern battle_menu_record_t g_battle_menu_unit_selection_records[BATTLE_UNIT_SLOT_COUNT];
extern u8 g_battle_move_current_edge_height;
extern u8 g_battle_move_destination_edge_height;
extern s32 g_battle_move_jump_gravity;
extern u8 g_battle_move_step_value;
extern s16 g_battle_move_target_screen_z;
extern VECTOR g_battle_offset_screen_coords;
extern s32 g_battle_previous_camera_yaw;
extern u16 g_battle_screen_fade_intensity;
extern POLY_F4 g_battle_screen_fade_polygons[2];
extern s16 g_battle_script_attack_entry_mode;
extern s32 g_battle_script_condition_unit_battle_id;
extern s32 g_battle_script_rand16_state;
extern u32 g_battle_script_unfiltered_controller_input;
extern s32 g_battle_sound_primary_weather_sfx_id;
extern s32 g_battle_sound_secondary_weather_sfx_id;
extern s32 g_battle_sound_suppressed;
extern s32 g_battle_sound_tertiary_weather_sfx_id;
extern u8 g_battle_sound_weather_sfx_enabled;
extern s32 g_battle_state_map_init_step;
extern u8 g_battle_target_ability_targets_list[16];
extern targeting_panel_entry_t g_battle_target_panel_data[0x200];
extern u8 g_battle_target_tile_targetable_flags[];
extern s16 g_battle_unique_value_list[];
extern s32 g_battle_unique_value_list_length;
extern void* g_battle_unit_last_misc_init_byte;
extern battle_unit_misc_data_t g_battle_unit_misc_data[16];
extern battle_unit_misc_slot_flag_t g_battle_unit_misc_slot_flags[];
extern unit_status_staging_t* g_battle_unit_status_staging_data;
extern s32 g_casting_unit_misc_id;
extern s32 g_controller_input_copy_12;
/* Current ability data at 0x801938c0: the attacker/target pair, weapons, formula
 * inputs and results of the strike being resolved, with copies of the ability's
 * secondary data, the weapon's data and the status-infliction record that
 * battle_action_run_pre_formula_setup loads. */
typedef struct battle_current_ability {
    u8 attacker_id;                             /* 0x00 */
    u8 target_id;                               /* 0x01 */
    u8 strike_count;                            /* 0x02 */
    u8 strike_counter;                          /* 0x03: weapon hand, 0 = right, 1 = left */
    u8 primary_weapon_id;                       /* 0x04 */
    u8 secondary_weapon_id;                     /* 0x05 */
    u16 reaction_id;                            /* 0x06 */
    u16 elemental_flags;                        /* 0x08 */
    u8 target_x;                                /* 0x0a */
    u8 target_y;                                /* 0x0b */
    u8 target_elevation;                        /* 0x0c */
    u8 terrain;                                 /* 0x0d: surface type under the target */
    u16 xa;                                     /* 0x0e: formula steps that divide it signed read it as s16 */
    u16 ya;                                     /* 0x10: likewise */
    u8 target_faith;                            /* 0x12 */
    u8 attacker_faith;                          /* 0x13 */
    u8 skillset;                                /* 0x14 */
    u8 unknown_15;                              /* 0x15 */
    u16 ability_id;                             /* 0x16 */
    u8 weapon_id;                               /* 0x18 */
    u8 two_hands_active;                        /* 0x19 */
    u8 proc_id;                                 /* 0x1a */
    u8 used_item_id;                            /* 0x1b */
    u8 base_hit;                                /* 0x1c */
    u8 unknown_1d;                              /* 0x1d */
    u8 accessory_evade;                         /* 0x1e */
    u8 right_shield_evade;                      /* 0x1f */
    u8 left_shield_evade;                       /* 0x20 */
    u8 class_evade;                             /* 0x21 */
    u8 facing_modifier;                         /* 0x22: 0 front, 1 side, 2 back */
    u8 unknown_23;                              /* 0x23 */
    u8 charge_power;                            /* 0x24 */
    u8 formula;                                 /* 0x25 */
    u8 target_is_undead;                        /* 0x26 */
    u8 earned_experience;                       /* 0x27 */
    u8 can_earn_exp_jp;                         /* 0x28 */
    u8 defaulted_to_attack;                     /* 0x29 */
    u8 random_damage_factor;                    /* 0x2a */
    u8 mp_cost;                                 /* 0x2b: effective cost recorded before the MP checks at
                                                 * 0x8017db74; reaction eligibility reads it even when no
                                                 * MP was subtracted */
    u8 target_count;                            /* 0x2c */
    u8 knockback_flags;                         /* 0x2d */
    u8 knockback_fall_height;                   /* 0x2e */
    u8 post_action_target_id;                   /* 0x2f */
    ability_secondary_data_t range_data;        /* 0x30 */
    weapon_data_t weapon_data;                  /* 0x3e */
    status_infliction_data_t status_infliction; /* 0x46 */
    u8 level_gained_flag;                       /* 0x4c */
    u8 job_level_gained_flag;                   /* 0x4d */
    u8 post_formula_flag;                       /* 0x4e */
    u8 random_fire_flag;                        /* 0x4f */
} battle_current_ability_t;

typedef char battle_current_ability_size_must_be_0x50[(sizeof(battle_current_ability_t) == 0x50) ? 1 : -1];

extern battle_current_ability_t g_current_ability;
extern s32 g_current_ability_hamedo_flag;
extern s32 g_current_battle_event_id;
extern s32 g_current_facing_direction;
extern const RECT g_deployment_clear_rect;
extern u8 g_effect_load_state;
extern u16 g_enemy_level_sum;
extern u8 g_highest_enemy_level;
extern u8 g_map_max_x;
extern u8 g_map_max_y;
extern menu_text_state_t g_menu_text_state;
extern s32 g_previous_battle_game_state;
extern u8 g_reaction_unit_action_data_16e[];
extern s32 g_selected_ability;
extern s32 g_sound_effect_id_to_play;
extern s32 g_source_unit_misc_id;
extern battle_stats_t* g_battle_action_target;
/* Text variables $27 and $28 (rows of glyph pixels drawn, 0x10 by default),
 * bound separately: battle_menu_display_text also takes the table base, and
 * element spellings let GCC address them from it. */
extern s32 g_battle_text_substitution_value_27;
extern s32 g_battle_text_substitution_value_28;

/* Unnamed data, in address order. */
extern u16 g_main_scenario_condition_offsets[];
extern s16 g_main_scenario_condition_data[];
extern u8 g_battle_move_entry_edge_centre_offsets[];
extern u8 g_battle_move_step_centre_offsets[];
extern u8 g_battle_move_climb_step_centre_offsets[];
extern u8 g_battle_move_ascent_centre_offsets[];
extern u8 g_battle_sound_weapon_miss_sfx_ids[];
extern u8 g_battle_sound_weapon_hit_sfx_ids[];
extern u8 g_battle_sound_shield_deflect_sfx_ids[];
extern u8 g_battle_weapon_attack_animations[][3];
extern u8 g_battle_effect_weapon_type_extra_flags;
extern u8 g_battle_random_battle_music_tracks[];
extern u16 g_battle_deployed_unit_palettes[2][16];
extern u16 g_battle_gfx_wait_direction_arrow_palette[16];
extern u16 g_battle_gfx_map_selection_cursor_palette[16];
extern u16 g_battle_map_weather_particle_palette[];
extern u16 g_battle_gfx_shadow_palette[16];
extern u16 g_battle_gfx_status_bubble_palette[16];
extern s32 g_battle_deployment_skipped;
extern s32 g_battle_gfx_status_bubble_delay;
extern u8 g_battle_gfx_screen_color_modulation_target[3];
extern u16 g_battle_gfx_screen_color_modulation_value[3];
extern u16 g_battle_gfx_screen_color_modulation_step[3];
extern RECT g_battle_gfx_screen_modulation_tex_window;
extern s32 g_battle_unit_palette_animation_offset;
extern s32 g_battle_unit_palette_animation_timer;
/* Queue of post-action "effect message" popups, drained newest-first by
 * battle_action_resume_attack_phase_control. The stride is 8: the producer at
 * 0x80072d8c indexes the array directly and the consumer walks it by
 * `idx * 8`, and only these three fields are ever read or written. */
typedef struct battle_post_effect_msg {
    u8 code;   /* 0x00 message code passed to battle_menu_init_system_function */
    u8 unit;   /* 0x01 misc ID of the acting unit */
    u8 pad[2]; /* 0x02 */
    s32 value; /* 0x04 reported amount */
} battle_post_effect_msg_t;

extern battle_unit_misc_data_t* g_battle_move_tile_occupant;
extern s32 g_battle_move_displacement_direction;
extern battle_post_effect_msg_t g_battle_action_post_effect_msgs[];
extern battle_unit_misc_data_t* g_battle_misc_unit_list_head;
/* Unit graphics loader state: file kind, phase, file or descriptor index and
 * the heap buffer being filled. */
extern s32 g_battle_gfx_state_words[4];
extern u8 g_battle_gfx_cycled_deployed_palettes[2][32];
extern s32 g_battle_gfx_unit_animation_data;
extern s16 D_800b628c;
extern s16 D_800b6290;
extern u8 g_battle_gfx_item_palettes[0x200];
extern u8 g_battle_gfx_eff_palettes[0x200];
extern u8 g_battle_gfx_frog_palettes[][32];
extern u8 g_battle_gfx_crystal_palettes[][32];
extern u8 g_battle_gfx_treasure_palettes[][32];
extern s32 g_battle_gfx_shp_frame_data_buffer;
extern u16 g_battle_cursor_screen_x;
extern POLY_F4 g_battle_gfx_screen_modulation_polygons[2];
extern MATRIX g_battle_map_light_color_matrix;
extern MATRIX g_battle_map_light_matrix;
extern SVECTOR g_battle_map_light_rotation;
extern s32 g_battle_map_back_color_red;
extern s32 g_battle_map_back_color_green;
extern s32 g_battle_map_back_color_blue;
extern s16 g_battle_map_weather_effect_mode;
extern VECTOR g_battle_map_camera_zoom;
extern u8 g_battle_map_back_color_disabled;
extern s32 D_800F668C;
extern MATRIX g_battle_map_camera_matrix;
extern s32 g_battle_map_command_0x96_frame;
extern MATRIX g_battle_map_light_rotation_matrix;
/* Untextured map polygons go to OT slot (otz & mask) | fixed index: {0, 4}
 * normally, {-1, 0} while map effect data is active (command 0x88), so a
 * non-zero mask also marks that state. */
extern s32 g_battle_map_untextured_fixed_ot_index;
extern s32 g_battle_map_untextured_otz_mask;
extern s32 g_battle_map_command_0x96_duration;
extern char g_battle_text_wait_for_allocation_message[];
extern const char g_battle_script_pause_message[];
extern char g_battle_text_wait_for_file_allocation_message[];
extern battle_unit_status_record_t g_battle_menu_active_turn_banner;
/* 0x40-byte billboard block at 0x8014d0a0 saved and restored whole with the AT
 * banner: unit stats indexed as halfwords by
 * battle_menu_store_units_small_in_battle_display_data, with the editor panel
 * view g_battle_unit_editor_panel_data at +0xe. */
extern s16 g_battle_menu_billboard_data[0x20];
extern u8 g_battle_menu_billboard_skillset_name[];
extern u8 g_battle_menu_billboard_secondary_skillset_name[];
extern u8 g_battle_menu_billboard_unit_name[];
extern u8 g_battle_menu_billboard_job_name[];
extern u16 g_battle_menu_ability_display_flags[20];
extern s32 g_battle_spell_quote_last_ability_id;
extern s32 g_battle_menu_used_skillset_id;
extern s32 g_battle_menu_used_item_id;
extern s32 g_battle_menu_system_function_command;
extern s32 g_battle_menu_system_function_option;
extern u8 g_battle_menu_restore_depth;
extern u8 g_battle_menu_restore_unit_id;
extern u16 g_battle_unit_view_started_battle_id;
extern u16 g_battle_unit_view_started_comparison_id;
extern battle_menu_id_record_t g_battle_menu_id_records[31];
extern u8 g_battle_menu_glyph_image_data[];
extern u8 g_battle_menu_default_palette_colors[];
extern u8 g_battle_text_glyph_bitmaps[];
extern RECT g_battle_menu_disabled_texture_window;
extern u16 g_battle_camera_script_variable_indices[7];
extern s32 g_battle_unit_view_mode;
extern s16 g_battle_camera_position_locked;
extern s16 g_battle_script_debug_paused;
extern s16 g_battle_event_effect_target_x;
extern s16 g_battle_event_effect_target_y;
extern s32 g_battle_menu_scroll_list_depth;
extern s16 g_battle_map_destruction_wait_state;
extern s32 g_battle_camera_yaw_band;
extern u16 g_battle_map_refresh_pending;
extern map_selected_tile_t g_battle_map_selected_tile_data;
extern s16 g_battle_text_speaker_swap_unit_a;
extern s16 g_battle_text_speaker_swap_unit_b;
extern s32 g_battle_system_function_thread_busy;
extern s16* g_battle_menu_ability_display_flags_ptr;
extern u16 g_battle_menu_status_visible_rows;
extern u16 g_battle_menu_status_scroll_rows;
extern s16 D_801669E8;
/* Row actions of the auto-battle setting menu (menu entry 0x80166b4c, +0x24). */
extern s16 g_battle_menu_auto_battle_row_actions[8];
extern s16 g_battle_menu_action_slot_row_actions;
extern s16 g_battle_menu_wait_row_actions[4];
extern u16 g_battle_menu_status_list_row_actions[8];
extern u16 g_battle_menu_system_function_row_actions[];
extern s16 g_battle_menu_confirm_stored_unit_row_actions;
extern s16 g_battle_menu_set_auto_battle_row_actions;
extern u16* g_battle_menu_window_tween_easing_tables[];
extern u16 g_battle_unit_status_bounce_step;
extern u16 g_battle_menu_hovered_stats_frame_count;
extern u8 g_battle_tile_info_image_location[];
extern world_gfx_image_load_parameters_t g_battle_tile_info_image_params[2];
extern RECT g_battle_menu_sprite_page_texture_windows[5];
extern world_gfx_image_load_parameters_t g_battle_menu_sprite_page_image_params[30];
extern RECT g_battle_menu_attack_caster_stats_display;
extern RECT g_battle_menu_attack_target_stats_display;
extern u8 g_battle_menu_attack_caster_stats_setup[];
extern u8 g_battle_menu_attack_target_stats_setup[];
extern RECT g_battle_numeric_display_frame_rect;
extern RECT g_battle_numeric_display_texture_window;
extern RECT g_battle_numeric_display_text_rect_a;
extern RECT g_battle_numeric_display_text_rect_b;
extern RECT g_battle_numeric_display_text_rect_c;
extern world_gfx_image_load_parameters_t g_battle_numeric_display_image_params[18];
extern u8 g_battle_numeric_display_entries[];
extern world_gfx_image_load_parameters_t g_battle_unit_editor_sprite_image_params_a[19];
extern world_gfx_image_load_parameters_t g_battle_unit_editor_sprite_image_params_b[19];
extern world_gfx_image_load_parameters_t g_battle_unit_editor_row_image_params_a[4];
extern world_gfx_image_load_parameters_t g_battle_unit_editor_row_image_params_b[4];
extern world_gfx_image_load_parameters_t g_battle_unit_editor_item_icon_image_params[5];
extern u16 g_battle_unit_editor_sprite_clut_pairs[19][2];
extern s32 g_battle_menu_companion_executable_lbas[];
extern s32 g_battle_menu_companion_executable_sizes[];
extern s32 g_battle_menu_companion_executable_offsets[];
extern s32 g_battle_menu_secondary_companion_executable_lbas[];
extern s32 g_battle_menu_secondary_companion_executable_sizes[];
extern s32 g_battle_menu_secondary_companion_loading;
extern u8 g_battle_menu_idle_action_entry_labels[];
extern s32 g_battle_menu_building_thread_started;
extern u8 g_battle_menu_panel_fade_modes[];
extern u8 g_battle_menu_idle_action_flag_clear_ids[];
extern u8 g_battle_script_condition_param_lengths[];
extern s32 g_battle_camera_wrap_yaw_pending;
extern u16 g_battle_script_map_darkness_durations[];
extern u16 g_battle_script_unit_filter_modes[];
extern u8 g_battle_rotation_speed_frames[];
extern s32 g_battle_script_event_input;
extern s16 g_battle_menu_status_left_text_ids[20];
extern s16 g_battle_menu_status_right_text_ids[20];
extern s32 g_battle_menu_action_menu_build_result;
extern world_menu_palette_slot_t g_battle_numeric_display_palettes[];
extern world_menu_palette_slot_t g_battle_unit_summary_palettes[];
extern u8 g_battle_unit_summary_digit_images[];
extern world_menu_palette_primitives_t g_battle_unit_editor_palette;
extern s32 g_battle_saved_game_options;
extern s32 g_battle_script_saved_event_input;
extern s16 g_battle_event_map_jump_in_request;
extern s16 g_battle_script_threads_idle;
extern s32 g_battle_companion_overlay_primitives;
extern VECTOR g_battle_camera_script_vector;
extern void* g_battle_camera_published_coords_ptr;
extern battle_camera_rotation_t g_battle_camera_script_rotation;
/* Scratch map coordinates filled by battle_unit_copy_map_coords_from_misc_id;
 * read as x/z/y halfwords by battle_camera_pick_nearest_unblocked_yaw. */
extern s16 g_battle_scratch_coords[3];
extern SVECTOR* g_battle_camera_published_rotation_ptr;
extern s16 g_battle_menu_selected_option;
extern s32 g_battle_gfx_tpage7_free_rect_pixels[4];
extern s32 g_battle_gfx_tpage7_free_rect_cells[4];
extern battle_action_data_t* g_battle_menu_preview_target_action;
extern world_fade_tile_frame_t g_battle_menu_panel_fade_frames[];
extern s32 D_80173C70;
extern s32 g_battle_next_event_id;
extern u16 g_battle_script_frame_result_override;
extern VECTOR g_battle_script_light_scale;
extern s32 g_battle_event_pending_loaded_evtchr_slot;
/* BATTLE scratch at 0x801cc074, in the event-overlay area past BATTLE.BIN:
 * battle_menu_build_sprite_page lays it out as two sprite pages and the
 * scrolling ability list saves its five 16-colour CLUTs there. WORLD keeps
 * its own g_world_menu_panel_buffer at the same address. */
typedef union battle_menu_scratch_buffer {
    menu_sprite_page_t sprite_pages[2];
    u16 clut_save[5 * 16];
} battle_menu_scratch_buffer_t;

extern battle_menu_scratch_buffer_t g_battle_menu_scratch_buffer;
extern s32 g_battle_help_text_id_tables_ptr;
extern u8 g_battle_script_light_base_direction_matrix[];
extern void* g_battle_event_loaded_evtchr_buffer;
extern SVECTOR g_battle_script_light_angles;
extern u8 g_battle_script_light_base_color_matrix[];
extern u8 g_battle_action_menu_skillsets[];
extern u8* g_battle_move_terrain_costs_ptr;
extern s32 g_battle_sort_targets_nearest_first;
extern s32 g_battle_move_reachable_tiles_valid;
extern s32 g_battle_move_reachable_unit_id;
extern s32 g_battle_between_turn_resume_state;
extern u8 g_battle_rider_status_infliction_mask[5];
extern s32 g_battle_ai_propagation_resume_pass;
extern u8 g_battle_loaded_ability_flags_1;
extern u8 g_battle_move_path[];
extern battle_target_panel_t g_battle_target_panel_last;
/* g_reaction_unit_action_data_16e + 2: the saved battle_stats_t
 * last_ability_id. The container is a raw byte copy of the unit's action
 * fields, so this halfword keeps its own name. */
extern s16 g_reaction_unit_last_ability_id;
extern u8 D_8019387C;
extern u8 g_battle_player_unit_fallen;
/* Current attacker record at 0x8019389c, filled by
 * battle_action_set_current_attacker_data and read by
 * battle_action_set_mimic_ability. The AI saves and restores it whole as 30
 * bytes (battle_ai_store_considered_action_data). */
typedef struct battle_current_attacker_data {
    u8 facing;                         /* 0x00 */
    u8 data_initialized;               /* 0x01: set once the fields below are filled; no stronger meaning yet */
    u8 right_hand_weapon_id;           /* 0x02 */
    u8 left_hand_weapon_id;            /* 0x03 */
    u8 team;                           /* 0x04 */
    u8 unknown_05;                     /* 0x05 */
    s16 target_delta_x;                /* 0x06 */
    s16 target_delta_y;                /* 0x08 */
    battle_ai_command_action_t action; /* 0x0a: copy of battle_stats_t +0x16e */
} battle_current_attacker_data_t;

typedef char battle_current_attacker_data_size_must_be_30[(sizeof(battle_current_attacker_data_t) == 30) ? 1 : -1];

extern battle_current_attacker_data_t g_current_ability_attacker;
extern battle_action_data_t g_current_action_data;
extern u16 g_battle_ai_range_row_masks[];
extern s32 g_battle_ai_self_target_origin_stage;
extern s32 g_battle_effect_camera_rotation_mode;
extern s32 g_battle_effect_camera_position_mode;
extern s32 g_battle_effect_camera_zoom_mode;
extern u8 g_battle_camera_quadrant_block_directions[4];
extern u8 g_battle_effect_trap_frame_data[];
extern u8 g_battle_effect_ninja_ball_masks[];
extern s32 g_battle_effect_gravity_scale;
extern MATRIX g_battle_effect_particle_heading_matrix;
extern MATRIX g_battle_effect_emitter_matrix;
extern MATRIX g_battle_effect_projectile_matrix;
extern s32* g_battle_effect_model_header;
extern s32* g_battle_effect_model_vertices;
extern s32* D_801B8A34;
extern s32* g_battle_effect_model_commands;
extern s32 g_battle_effect_model_command_index;
extern SVECTOR g_battle_effect_camera_rotation_start;
extern SVECTOR g_battle_effect_camera_rotation_target;
extern SVECTOR g_battle_effect_camera_rotation_current;
extern SVECTOR g_battle_effect_camera_rotation_saved;
extern VECTOR g_battle_effect_camera_position_start;
extern VECTOR g_battle_effect_camera_position_target;
extern VECTOR g_battle_effect_camera_position_current;
extern VECTOR g_battle_effect_camera_position_saved;
extern s32 g_battle_effect_camera_zoom_duration;
extern s32 g_battle_effect_camera_zoom_frame;
extern VECTOR g_battle_effect_camera_zoom_start;
extern VECTOR g_battle_effect_camera_zoom_target;
extern VECTOR g_battle_effect_camera_zoom_current;
extern VECTOR g_battle_effect_camera_zoom_saved;
extern s32 D_801B8B18;
extern s32 D_801B8B1C;
extern s32 g_battle_effect_trajectory_hit_unit_id;
extern void* g_battle_effect_trap_frame_data_ptr;
extern s32 g_battle_effect_callback_slots[];
extern s32 D_801BACC4;
extern u8 g_battle_effect_palette_target_misc_ids[];
extern SVECTOR g_battle_effect_trajectory_origin;
extern SVECTOR g_battle_effect_trajectory_destination;
extern s32 g_battle_effect_trajectory_step_count;
extern u8* g_battle_effect_palette_ptr;
extern s32 D_801BC0C4;
extern s32 D_801BC0D8;

void battle_effect_set_ability_animation(u32 packed, s32 ability, battle_effect_secondary_init_t* source);
void animation_exception_handler(s32 exception_id);
void battle_map_blend_darkness_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue);

/* action */
s32 battle_action_add_poached_item_to_fur_shop_inventory(void);
s32 battle_action_calculate_at_list(battle_at_entry_t* list, s32 mode);
u32 battle_action_get_number_of_turns_to_resolve(s32 unit_index, s32 ct, battle_at_entry_t* entries);
s32 battle_action_calculate_at_list_and_get_specific_unit_id(s32 unit_idx);
s32 battle_action_calculate_chance_to_react(const battle_stats_t* unit);
void battle_action_call_attack_preparation(battle_ai_command_action_t* action);
s32 battle_action_can_unit_react(const battle_stats_t* unit);
/* Provisional: the definition falls off the end after the tail call to
 * battle_formula_can_unit_evade, so v0 carries that result; every caller reads
 * it as an s32 status. */
s32 battle_action_can_unit_react_1(battle_stats_t* unit);
void battle_state_enter_action_cast(void);
s32 battle_action_check_and_consume_mp(battle_stats_t* unit);
void battle_action_check_arrow_guard_usability(void);
void battle_action_check_between_turn_events(void);
void battle_action_check_blade_grasp_usability(void);
void battle_action_check_counter_reaction_usability(u16 reaction_id, u32 mask);
s32 battle_action_check_change_of_turn(s32 unit_id);
void battle_action_check_face_up_and_absorb_used_mp_usability(s16 reaction_id);
void battle_action_check_critical_quick_hp_restore_mp_restore_meatbone_slash_usability(s16 reaction_id);
void battle_action_check_damage_split_usability(void);
void battle_action_check_distribute_usability(void);
void battle_action_check_mp_switch_usability(void);
void battle_action_check_pa_save_ma_save_speed_save_regenerator_auto_potion_gilgame_heart_usability(s16 reaction_id);
s32 battle_action_check_reaction(battle_stats_t* unit);
void battle_action_check_reflect_reaction(void);
void battle_action_choose_facing_for_wait(void);
void battle_action_choose_wait(void);
void battle_action_clear_at_list_active(void);
void battle_action_clear_at_list_id(void);
void battle_action_open_status_menu_at_list_start(void);
void battle_action_clear_current_data(battle_action_data_t* action);
void battle_action_clear_data(void);
void battle_action_clear_knockback_flag(void);
void battle_action_clear_status_changes(battle_action_data_t* action);
void battle_action_confirm(void);
void battle_action_copy_active_turn_data_from(const u8* banner, const u8* unit, const u8* billboard);
void battle_state_enter_commence_attack_phase(void);
s32 battle_action_end_turn(s32 unit_id);
void battle_action_execute_ability(void);
void battle_noop_8018ef2c(void);
void battle_action_finalize_target_current_action(void);
void battle_state_enter_open_sp2_files(void);
void battle_action_get_next_acting_unit(s32 arg);
void battle_action_handle_move_command(void);
void battle_action_handle_post_action_xp_jp_ability(void);
void battle_action_increment_at_list_id(void);
s32 battle_action_increment_item_quantity_for_steal_break(battle_stats_t* unit, s32 item);
void battle_action_init_learn_ability_on_hit(void);
s32 battle_action_init_movement_ability_benefit(battle_stats_t* unit);
void battle_action_is_at_list_unit_charging_ability(void);
s32 battle_action_report_job_level_up(battle_unit_misc_data_t* misc);
s32 battle_action_report_level_up(battle_unit_misc_data_t* misc);
s32 battle_action_resume_attack_phase_control(void);
void battle_action_run_between_turn_events(void);
void battle_action_set_at_list_active(void);
void battle_action_set_casting_unit_id_ff(void);
void battle_action_set_current_attacker_data(battle_stats_t* unit);
void battle_action_set_damage_display_type_based_on_ability(void);
void battle_action_set_item_throw_stone_ability_display(void);
s32 battle_action_set_move_act_flags(s32 unit_id, s32 move_flag, s32 act_flag);
void battle_action_set_only_action_taken(s32 unit_id);
void battle_action_set_only_movement_taken(s32 unit_id);
void battle_action_set_target_variables(battle_stats_t* unit);
s32 battle_action_should_end_unit_turn(battle_stats_t* unit);
void battle_action_enter_move_range_exception(void);
void battle_action_show_caster_post_effect_messages(void);
void battle_action_switch_ability_to_default_attack(void);

/* bin */
void battle_bin_entrypoint(void);

/* calculate */
battle_unit_height_data_t* battle_calculate_unit_height_data(battle_unit_height_data_t* out, s32 unit_id);

/* clear */
s32 battle_classify_character_identity_slot(u32 character_identity);
s32 battle_return_zero_801842f8(void);

/* convert */
void battle_camera_convert_screen_coords_by_rotation(const u16* input, s16* output, s32 offset);

/* display */
void battle_display_menu_number_entry(s32 value, u8 digit_count, s32 image, u16* origin);
void battle_gfx_draw_status_bubble(battle_unit_misc_data_t* unit, const u16* screen_coords);

/* effect */
s32 battle_effect_init_fall_dust_secondary(battle_effect_secondary_init_t* source, VECTOR* position);
s32 battle_effect_is_item_ability(s32 ability_id);
void battle_effect_set_arrow_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id);

/* find */
battle_stats_t* battle_find_unit_data_pointer_for_entd_unit_id(s32 unit_id, s32* out_battle_id);

/* formula */
void battle_formula_3c_damage_caster_max_hp_one_fifth_heal_target_two_fifths(void);
void battle_formula_43_damage_caster_missing_hp(void);
void battle_formula_add_brave_x_stats_y(void);
void battle_formula_apply_attack_up_and_martial_arts(void);
void battle_formula_apply_attacker_berserk_frog(void);
void battle_formula_apply_catch(void);
void battle_formula_apply_charge(void);
void battle_formula_apply_damage_and_knockback(void);
void battle_formula_apply_dance_abilities(void);
void battle_formula_apply_defense_up(void);
void battle_formula_apply_elemental(void);
void battle_formula_apply_elemental_absorption(void);
s32 battle_formula_apply_elemental_absorption_and_status(void);
/* Provisional: the definition falls off the end after
 * battle_formula_roll_conditional_status_proc, so v0 carries that result and
 * every caller tests it. */
s32 battle_formula_apply_elemental_absorption_and_status_proc(void);
void battle_formula_apply_elemental_strengthen(void);
void battle_formula_apply_evasion_changes_due_to_statuses(void);
void battle_formula_apply_full_hp_mp_heal(void);
void battle_formula_apply_hp_absorption(void);
void battle_formula_apply_magic_attack_up(void);
void battle_formula_apply_magic_defense_up(void);
void battle_formula_apply_magical_support_status_compatibility(void);
void battle_formula_apply_maintenance(void);
void battle_formula_apply_mp_recovery(void);
void battle_formula_apply_physical_attack_supports(void);
void battle_formula_apply_physical_status_support_compatibility(void);
void battle_formula_apply_physical_xa_modifying_statuses_support(void);
void battle_formula_apply_quick_effect(void);
void battle_formula_apply_song_abilities(void);
void battle_formula_apply_status(void);
void battle_formula_apply_status_to_action(void);
s32 battle_formula_apply_status_and_check_undead(void);
s32 battle_formula_apply_steal_break_might_sword_hardcoding(void);
void battle_formula_apply_target_magical_status_xa_modifiers(void);
void battle_formula_apply_target_xa_affecting_statuses(void);
void battle_formula_apply_undead_absorb_attack(void);
void battle_formula_apply_undead_reversal(void);
void battle_formula_apply_weapon_element_strengthen(void);
void battle_formula_apply_weather_effects_on_bows(void);
void battle_formula_apply_weather_elemental_effects(void);
void battle_formula_apply_y_brave(void);
void battle_formula_calculate_abandon(void);
s32 battle_formula_calculate_attack_evaded(u16 numerator, u16 denominator, u8 miss_type, u8 item_lost);
void battle_formula_calculate_base_xa(void);
void battle_formula_calculate_concentrate(void);
void battle_formula_calculate_critical_hit(void);
void battle_formula_calculate_dark_confuse(void);
void battle_formula_calculate_facing_evade(void);
void battle_formula_calculate_faith(void);
void battle_formula_calculate_final_hit_percent(void);
void battle_formula_calculate_gravi2_damage(void);
void battle_formula_calculate_hp_percent_damage(void);
void battle_formula_calculate_katana_break_chance(void);
s32 battle_formula_calculate_magic_accuracy_without_faith(void);
s32 battle_formula_calculate_magical_evade(void);
void battle_formula_calculate_magical_xa_times_ya(void);
void battle_formula_calculate_mp_percent_damage(void);
void battle_formula_calculate_physical_damage(void);
s32 battle_formula_calculate_physical_evade(void);
void battle_formula_calculate_stolen_gil(void);
void battle_formula_calculate_transparent(void);
void battle_formula_calculate_truth_damage(void);
void battle_formula_calculate_truth_magical_damage(void);
s32 battle_formula_can_unit_evade(battle_stats_t* unit);
s32 battle_formula_calculate_hit(void);
/* Provisional: falls off the end after battle_formula_calculate_hit, whose
 * result stays in v0 for the caller's test. */
s32 battle_formula_calculate_physical_evade_charge(void);
void battle_formula_modify_damage_for_element(s32 element);
void battle_formula_nullify_action(void);
void battle_formula_cause_action_miss(void);
void battle_formula_check_dragon(void);
void battle_formula_clear_nullify_flags(void);
void battle_formula_convert_hp_damage_to_mp_recovery(void);
void battle_formula_determine_reduced_stat(void);
extern void battle_formula_force_attack_miss(void);
void battle_formula_force_sleeping_target_miss(void);
s32 battle_formula_get_random_0_7fff(void);
void battle_formula_heal_mp_z_times_ten(void);
void battle_formula_init_weapon_xa_ya(void);
void battle_formula_modify_elemental_damage(void);
void battle_formula_roll_conditional_status_proc_inner(void);
s32 battle_formula_roll_conditional_status_proc(void);

/* ai */
s32 battle_ai_decide_status_ct_based(s32 limit, s32 unit_id);

/* camera */
void battle_camera_convert_screen_coords_modify_by_1(const s16* screen, s16* destination);
void battle_camera_toggle_tilt(s32 unused_0, s32 sound_arg);

/* map */
s32 battle_map_calculate_location(battle_stats_t* unit);
s32 battle_map_calculate_move_find_item_flag(s32 map_id, s32 tile_index, s32 mode);
s32 battle_map_get_tile_data_value(s32 field, s32 x, s32 y, s32 layer);
void battle_map_init_ambient_light(s32 unused_0, s32 unused_1, s32 map_id);
void battle_map_init_background_gradient(s32 unused_0, s32 unused_1, s32 map_id);
s32 battle_formula_roll_random_at_least(s32 limit, s32 threshold);
void battle_formula_select_magic_gun_ability(void);
void battle_formula_set_exp_stolen(void);
void battle_formula_store_ma_and_ma_plus_y_divided_by_two(void);
void battle_formula_store_ma_and_pa_plus_y_divided_by_two(void);
void battle_formula_store_ma_and_x(void);
void battle_formula_store_ma_and_y(void);
void battle_formula_store_magical_evade_values(void);
void battle_formula_store_pa_and_pa_plus_y_divided_by_two(void);
void battle_formula_store_pa_and_weapon_power(void);
void battle_formula_store_pa_and_weapon_power_plus_y(void);
void battle_formula_store_pa_and_x(void);
void battle_formula_store_pa_and_y(void);
void battle_formula_store_reaction_proc_id_and_target(void);
void battle_formula_store_speed_and_x(void);
void battle_formula_store_xa_plus_ya_status_damage(void);
void battle_formula_store_xa_times_ya_damage(void);
void battle_formula_use_hp_damage_as_action_hit_percent(void);

/* func */
s16 battle_camera_pick_nearest_unblocked_yaw(s32 misc_id_a, s32 misc_id_b);
s32 battle_mul_div_s64(s32 a, s32 b, s32 c);
s32 battle_move_check_horizontal_jump(s32, u8, u8, u8);
u8* battle_move_calculate_pathing(s32 flags, s32 jump, s32 x, s32 y, u32 level, s32 target_x, s32 target_y,
    u32 target_level, s32 initialize, s32* suspended, s32 check_budget);
s32 battle_target_set_panels_for_action(u8* action);

/* get */
battle_stats_t* battle_unit_get_existing_pointer(u32 unit_id);
extern s32 battle_get_misc_id(s32 unit_id);

/* handle */
void battle_handle_menu_cancel_input(void* menu);

/* is */
s32 battle_is_skillset_in_spell_quote_exception_list(s32 skillset_id);

/* load */
void battle_load_entd_units(entd_encounter_t* entd_data, s32 mode);

/* map */
void battle_map_start_darkness_blend(s32 first, s32 second, s16 third, s16 fourth, s16 fifth);
s32 battle_map_check_deep_dungeon_crystal_and_init_state(void);
void battle_map_color_field(s32 first, s32 second, s16 red, s16 green, s16 blue);

/* Only SET/GET define a result (stored RGB pointer, or null for SET while
 * disabled). Other command paths leave the target return register unspecified. */
s32* battle_map_control_gte_background_color(s32 command, s32 red, s32 green, s32 blue);
void battle_map_polygon_flag_command(s32 command);
MATRIX* battle_map_light_matrix_command(s32 command, MATRIX* color, MATRIX* light, s32 duration);
void battle_map_data_load_complete_thread(void);
s32 battle_map_dispatch_gns_resource(u8 resource_type, u8* data);
void battle_map_extract_palette_colors(const u16* palette, s32 mesh_index, s32 palette_row_index);
extern s32 battle_map_get_effective_weather(void);
map_tile_t* battle_map_get_tile_data_pointer(s32 x, s32 y, u32 layer);
map_tile_t* battle_map_get_tile_data_ptr_from_misc_id(u32 misc_id);
void battle_map_init_palette_state(void);
s32 battle_map_load_data_stage_0x75(void);
s32 battle_map_load_data_stage_0x76(void);
void battle_map_load_move_find_item_data(map_move_find_item_data_t* map_data);
void battle_map_mark_data_load_complete(void);
void battle_map_modify_background_gradient(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void battle_map_reset_texture_animations(void);
void battle_map_set_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors);
void battle_map_set_command_0x96_duration(s32 id, s32 value);
void battle_map_start_ambient_light_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void battle_map_set_darkness_color(s32 frame_duration, const map_color_t* color);
s32 battle_map_step_init_sequence(s32 map_id, s32 step);
s32 battle_map_stop_texture_animation(s32 handle);
void battle_map_store_modified_palette_colors(s32 palette_group, s32 palette_index, s32 store_all);
void battle_map_store_selected_tile_coordinates(void* destination);
void battle_map_store_selected_tile_data(void);
s32 battle_map_try_start_deep_dungeon_mesh_load(void);
s32 battle_map_update_deep_dungeon_and_animation(void);
void battle_map_wait_for_refresh(void);

/* menu */
void* battle_menu_alloc_buffer(s32 bytes);
void* battle_menu_alloc_memory(s32 size);
void* battle_menu_build_and_upload_window_frame_image(s32 width, s32 height, RECT* rect, s32 mode);
void battle_menu_build_idle_action_menu(s32 menu_id);
s32 battle_menu_build_quad_pieces(s32 group, POLY_GT4* poly);
void battle_menu_build_skillset_entries(void);
void battle_menu_build_unit_status_list(void);
void battle_menu_build_window_image_row(const u16* tiles, u16* row, s32 count, s32 index);
void battle_menu_building_thread(void);
s32 battle_menu_can_accept_input(void);
void battle_menu_check_action_menu_restrictions(void);
void battle_menu_clear_idle_action_menu_entry_flags(void);
void battle_menu_close_move_help(void);
void battle_menu_dispatch_idle_action_menu(void);
void battle_menu_display_triangle_selection(void);
void battle_menu_draw_text_columns_narrow(world_menu_entry_t* entry, s32* row_offset, void* buffer);
void battle_menu_enter_status_screen_selection(void);
void battle_menu_fade_out_thread(void);
void battle_menu_free_high_overlay(void);
s32 battle_menu_get_cursor_bob_offset(s32 mode);
s32* battle_menu_get_selected_ability_address(void);
void battle_menu_icon_linked_entry_thread(void);
s32 battle_menu_init_action_menu(battle_unit_misc_data_t* unit);
void battle_menu_init_buffer_allocator(void);
s32 battle_menu_init_monster_skill_check(battle_stats_t* unit);
s32 battle_menu_has_monster_skill_supporter_at_tile(s32 team_flags, s32 x, s32 y, s32 total_height);
void battle_menu_init_sprite_array(SPRT* sprites, s32 count, s32 clut);
s32 battle_menu_get_id_based_on_mount_moveable_actable(s32 misc_id);
void battle_menu_init_subsystem_pointers(void);
void battle_menu_init_subsystems(void);
s32 battle_menu_init_system_function(s32 type, s32 b, s32 c, s32 d, s32 e);
s32 battle_menu_is_still_building(void);
s32 battle_menu_load_elemental_abilities(s32 skillset_slot, u8 unit_id);
void battle_menu_leave_status_screen_selection(void);
s32 battle_menu_lerp_fixed12(s32 start, s32 end, s32 factor);
void battle_menu_lerp_rect_to_poly_ft4(POLY_FT4* poly, RECT* from, RECT* to, s32 factor);
void battle_menu_open_active_unit_idle_action_menu(void);
void battle_menu_open_companion_executable(void);
void battle_menu_open_companion_executable_6(void);
void battle_menu_open_debugchr_unit_editor(void);
void battle_menu_open_free_cursor_help(void);
void battle_menu_open_free_cursor_mini_menu(void);
void battle_menu_open_mini_menu(void);
void battle_menu_open_move_confirm(void);
void battle_menu_open_non_active_unit_idle_action_menu(void);
void battle_menu_open_option_menu_2(void);
void battle_menu_option_menu_thread(void);
void battle_menu_refresh_hovered_unit_stats_display(void);
void battle_menu_request_companion_executable_8(void);
void battle_menu_request_open_companion_executable(s32 request_index);
void battle_menu_request_open_secondary_companion_executable(s32 request_index);
void battle_menu_reset_buffer_slots(void);
void battle_menu_reset_unit_record(s32 record_index);
void battle_menu_retry_alloc_with_message(u32 byte_length);
void battle_menu_reset_subsystems(void);
void battle_menu_run_numeric_display_panel_thread(void);
void battle_menu_run_skillset_thread(void);
void battle_menu_run_system_function_thread(void);
void battle_menu_selected_tile_info_display_thread(void);
void battle_menu_set_next_script_action_menus(void);
void battle_menu_set_option_transition_finished(void);
void battle_menu_start_building_thread(void);
void battle_menu_start_entry_thread(void);
void battle_menu_start_mini_menu_display_thread(void);
void battle_menu_start_system_function_thread(s32 system_function);
void battle_menu_store_units_small_in_battle_display_data(void);
void battle_menu_update_panel_fade(void);

/* move */
void battle_move_set_unit_step_slope_scales(battle_unit_misc_data_t* unit);
void battle_move_step_unit_to_map_tile_center(battle_unit_misc_data_t* unit, s32 allow_height_change);

/* noop */
void battle_noop_8012e190(void);
void battle_noop_80143bd0(void);

/* palette */
void battle_map_modify_palette(
    s32 command, s32 amount, s32 palette_index, s32 color_index, s32 all_colors, u16 red, u16 green, u16 blue);

/* process */
void battle_process_inflict_status_commands(void);

/* rotate */
void battle_rotate_unit(const u8* parameters);

/* script */
/* Scenario interpreter interfaces shared by battle_script_execute_event and
 * world_script_execute_event. Signatures are as the interpreters bind them;
 * unverified names stay provisional. */
void battle_script_advance_date_by_one_year(void);
void battle_script_apply_input_overrides(u32* input);
void battle_script_assign_bitset_flag(u32* bitset, s32 bit_index, s32 value);
void battle_script_blue_remove_unit(s32 unit_id);
void battle_script_blueremoveunit_all_enemy_units(void);
void battle_script_change_stats(const u8* parameters);
void battle_script_clamp_s32_to_range(s32* value, s32 minimum, s32 maximum);
void battle_script_earthquake_start(void);
s32 battle_script_face_tile();
void battle_script_face_unit(const u8* parameters, s32 second_only);
void battle_script_filter_controller_input(u32* input);
void battle_script_focus(u8* parameters);
void battle_script_focus_speed(u8* parameters, s32* position, s32* rotation);
u32* battle_script_get_controller_input_pointer(s32 use_current);
s32 battle_script_get_current_scenario_finish_operation(void);
extern s32 battle_script_get_variable(s32 variable_id);
s32 battle_script_interpolate_range_fixed12(s32 start, s32 end, s32 fraction, s32 value);
void battle_script_join_units_silently_without_needing_darkscreen(void);
void battle_script_load_event(s32 id);

/* The byte-stream reader at 0x80146078 preserves signed halfwords. Some
 * callers forward their incoming a0 without an additional argument move.
 * unit_t lookup at 0x80133158 accepts and returns full words; narrowing is
 * caller-specific, not part of its interface. */
extern s16 battle_script_load_halfword(const u8* data);
s32 battle_script_load_next_event(void);
void battle_script_map_darkness(void);
s32 battle_script_march_units();
void battle_script_print_debug_message(void);
void battle_script_reset_event_state(void);
void battle_script_run_next_event(void);
void battle_script_seed_random_from_vsync(void);
void battle_script_set_variable(s32 variable_id, s32 value);
void battle_script_show_graphic(void);
void battle_script_sprite_move(void);
void battle_script_sprite_move_beta(void);
void battle_script_start_current_event_thread(void);
void battle_script_switch_tutorial_thread_for_eventend(void);
void battle_script_unit_anim(u8* parameters);
u32 battle_script_get_event_finish_operation(void);
void battle_script_store_halfword(u8* destination, s16 value);
void battle_script_update_event_input_state(u32 state);
void battle_script_wait_for_unit_ready(s32 misc_id);
void battle_script_warp_unit(const u8* parameters);

/* sound */
void battle_sound_play_movement_sfx(battle_unit_misc_data_t* unit, s32 sound_id);
void battle_sound_play_music_tracks(s32 first_track, s32 second_track);
void battle_sound_select_weapon_hit(battle_unit_misc_data_t* unit, s32 hit);
void battle_sound_set_effect_to_cancel(void);
void battle_sound_set_effect_to_confirm(void);
void battle_sound_set_effect_to_confirm_checked(void);
void battle_sound_set_effect_to_cursor_move(void);
void battle_sound_set_effect_to_invalid(void);
void battle_sound_start_weather_sfx(void);
s32 battle_sound_stop_weather_sfx(void);
void battle_sound_wait_for_music_stub(void);

/* status */
s32 battle_status_check_unit(battle_stats_t* unit);
void battle_status_disable_acting(battle_stats_t* unit);
void battle_status_enable_special_flags(s32 status_id, s32 enabled, s32 misc_unit_id);
void battle_status_queue_current_status_graphics(s32 battle_id, s32 enabled);
s32 battle_status_inflict_defending_to_battle_id(s32 unit_id);
s32 battle_status_is_unit_absent_dead_crystal_treasure_petrified_or_ridden(const battle_stats_t* unit);
s32 battle_status_modify_inflictions(s32 removal_only);
s32 battle_status_remove_charging_ability_ct(battle_stats_t* unit, s32 do_disable);
void battle_status_remove_control(battle_stats_t* unit);
s32 battle_status_resolve_unit_changes(s32 unit_id, s32 removal_only);
void battle_status_set_inflicted_ct_and_transfer_last_used_ct(s32 unit_idx);
void battle_status_store_for_current_attack(s32 unit_id, s32 removal_only);

/* target */
s32 battle_action_build_reaction_targets(s32 actor_id, battle_strike_work_t* work, s32* target_count, u8* target_ids);
void battle_target_apply_unit_team_eligibility(s32 raw_unit_id, u8 allow_allies, u8 allow_enemies, u8 aoe_is_0xff);
void battle_target_calculate_aoe_vertical_tolerance(s32 x, s32 y, s32 lo, s32 hi);
void battle_target_calculate_arc_range(battle_stats_t* unit, u8 range, u8 flags);
void battle_target_calculate_linear_attack_tiles(s32 dir, s32 x, s32 y);
s32 battle_target_calculate_map_for_action(battle_ai_command_action_t* action, u8* flags_3);
void battle_target_calculate_strike_lunge_range(battle_stats_t* unit, u8 flags);
void battle_target_calculate_tile_coords_and_glow_from_at_list(void);
void battle_target_calculate_tile_coords_with_cursor_glow(void);
void battle_target_calculate_weapon_range(battle_stats_t* unit);
void battle_target_can_select_tile(void);
void battle_target_clear_panel_data(void);
void battle_target_clear_panel_spread_flags(void);
void battle_target_disable_green_panel_flags(void);
s32 battle_target_disable_green_panel_on_all_but_target_tile(const u8* source);
void battle_target_gather_x_y_data_for_attacks(battle_unit_misc_data_t* unit);
s32 battle_target_get_unit_id_if_tile_targetable(s32 a0, s32 a1, s32 a2);
s32 battle_unit_find_at_tile(s32 x, s32 y, u32 level, s32 filters);
void battle_target_move_cursor_to_unit(battle_unit_misc_data_t* unit);
void battle_target_remove_close_range(s32 x, s32 y, s32 range);
void battle_target_select_random_tile_for_random_fire_abilities(void);
void battle_target_select_tile(void);
void battle_target_show_move_range_in_free_cursor(void);
void battle_target_sort_list(s32 target_battle_id);
void battle_target_spread_panels(u8 passes, s32 unused);
void battle_target_store_cursor_casting_unit_name_and_data(void);
void battle_target_store_cursor_unit_name_and_data(void);
s32 battle_target_set_boxes_red(void);
void battle_target_store_cursor_unit_as_preview_target(void);
void battle_target_update_free_cursor_selection(battle_unit_misc_data_t* unit, battle_unit_misc_data_t* selected_unit);
s32 battle_target_validate_height_overlap(
    s32 attacker_id, s32 target_x, s32 target_y, s32 target_elevation, s32 target_id);

/* text */
void battle_text_run_unit_message_thread(void);

/* thread */
void battle_thread_wait_for_10_to_13(void);

/* unit */
s32 battle_unit_animate_and_set_enemy_level_data_by_misc_id(u32 misc_id);
void battle_unit_apply_poach_morbol_transformation(battle_unit_misc_data_t* unit);
void battle_unit_apply_staged_status_data(s32 unit_id, s32 misc_id);
u8 battle_unit_calculate_move_exp_jp_up_to_earn(battle_stats_t* unit);
void battle_unit_sync_all_rider_mount_positions(void);
void battle_unit_clear_distortion_animation(battle_unit_misc_data_t* unit);
void battle_unit_clear_distortion_animation_2(battle_unit_misc_data_t* unit);
extern void battle_unit_clear_facing_update_suppression(u32 misc_id);
void battle_unit_clear_status_staging_data(void);
void battle_unit_copy_misc_screen_location(u32 misc_id, VECTOR* destination);
void battle_unit_copy_rider_data_to_mount(battle_unit_misc_data_t* unit, battle_unit_misc_data_t* rider);
s32 battle_unit_count_crystal_misc_units(void);
void battle_unit_decide_facing_direction(battle_unit_misc_data_t* unit, s32 facing);
void battle_unit_dismount_rider(battle_unit_misc_data_t* unit);
void battle_unit_dispatch_distortion_animation(battle_unit_misc_data_t* unit);
void battle_unit_face_toward_cursor(battle_unit_misc_data_t* unit);
battle_unit_misc_data_t* battle_unit_find_other_chocobo_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
void battle_unit_find_war_trophies_and_bonus_money(battle_war_result_t* result);
s32 battle_unit_disable_remove(s32 misc_id);
void battle_unit_init_for_store_anim_facing_move(s32 misc_id, s32 value);
void battle_unit_project_misc_to_screen(u32 misc_id, s16* screen_coordinates);
s32 battle_unit_set_enemy_level_data_by_battle_id(u32 battle_id);
s32 battle_unit_set_target_anim_by_misc_id(u16 misc_id);
extern void battle_unit_set_facing_update_suppression(u32 misc_id);
s32 battle_unit_get_battle_index_by_entd_unit_id(s32 unit_id);
s32 battle_unit_get_battle_index_by_misc_id(u32 misc_id);
battle_stats_t* battle_unit_get_by_unit_id(s32 unit_id);
battle_unit_misc_data_t* battle_unit_get_casting_misc_data(void);
u8 battle_unit_get_effective_height(battle_stats_t* unit);
s16 battle_unit_get_facing_quadrant_by_misc_id(u32 misc_id);
battle_unit_misc_data_t* battle_unit_get_selectable_misc_data_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
battle_unit_misc_data_t* battle_unit_get_crystal_or_treasure_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
battle_unit_misc_data_t* battle_unit_get_misc_data_by_battle_id(u32 battle_id);
battle_unit_misc_data_t* battle_unit_get_misc_data_by_misc_id(u32 misc_id);
s32 battle_unit_get_misc_id_by_battle_id(u32 battle_id);
battle_unit_misc_data_t* battle_unit_get_source_misc_data(void);
battle_stats_t* battle_unit_get_stats_from_battle_id(u32 id);
s32 battle_unit_has_misc_id(u32 misc_id);
void battle_unit_increase_blue_team_brave_by_10(void);
s32 battle_unit_init_party_member_battle_data(s32 unused0, s32 unused1, s32 unit_index);
void battle_unit_learn_used_ability(battle_stats_t* unit);
void battle_unit_modify_entd_unit(battle_stats_t* unit);
void battle_unit_mount_rider_onto_unit(battle_unit_misc_data_t* rider, battle_unit_misc_data_t* mount);
s32 battle_unit_move_misc_unit_to_head(u32 misc_id);
s32 battle_unit_place_in_new_location(u32 misc_id, s16* tile, s32 facing);
s32 battle_unit_poll_and_load_entd_units(void);
s32 battle_unit_remove_misc_by_id(u32 misc_id);
void battle_unit_reset_animation_states(void);
void battle_unit_save_previous_state(battle_unit_misc_data_t* unit);
void battle_unit_set_animation_based_on_mounted_state(battle_unit_misc_data_t* unit);
void battle_unit_set_bow_hardcoding(battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target);
s32 battle_unit_set_cannot_exist(s32 unit_index);
void battle_unit_set_enemy_level_data(battle_stats_t* unit);
void battle_unit_set_animation_based_on_status(battle_unit_misc_data_t* unit);
void battle_unit_set_idle_animation_for_movement(battle_unit_misc_data_t* unit);
void battle_unit_set_level_up_animation(battle_unit_misc_data_t* unit);
void battle_unit_set_map_coords_after_death_dismount(battle_unit_misc_data_t* unit);
void battle_unit_set_real_coords_from_map_coords(battle_unit_misc_data_t* unit);
void battle_unit_set_rider_mount_x_y_facing(battle_unit_misc_data_t* unit);
extern void battle_unit_set_specific_animation_value_on_battle_init(s32 misc_id, u32 value);
void battle_unit_snap_all_facings_to_quadrants(void);
void battle_unit_start_ability_charge_animation(battle_unit_misc_data_t* unit);
void battle_unit_start_ability_charge_animation_for_movement(battle_unit_misc_data_t* unit);
void battle_state_enter_pre_attack_animation(void);
void battle_unit_store_animation_facing(u32 animation, s32 facing, battle_unit_misc_data_t* unit);
void battle_unit_store_coordinate_mount_animation_data(battle_unit_misc_data_t* unit);
s32 battle_unit_try_get_misc_data_by_unit_id(u16* unit_id, s32* state);
void battle_unit_save_facing_before_attack(battle_unit_misc_data_t* unit);
void battle_move_undo_unit_move(battle_unit_misc_data_t* unit);
void battle_unit_update_and_animate_units(void);
void battle_unit_update_anim_display_for_all_targets(battle_unit_misc_data_t* unit);
void battle_unit_update_attacker_earned_experience(s32 outcome);
void battle_unit_update_display(battle_unit_misc_data_t* unit);
void battle_unit_update_display_by_misc_id(u16 misc_id);
s32 battle_unit_update_fade_out_removal(battle_unit_misc_data_t* unit);
void battle_unit_update_float_and_jump_height(battle_unit_misc_data_t* unit);
void battle_unit_update_graphics_by_misc_id_wrapper(u32 misc_id);
s32 battle_script_set_units_movement_effect_suppression();
void battle_unit_update_mount_animation_display(battle_unit_misc_data_t* unit);
void battle_unit_update_post_command_animation_display(battle_unit_misc_data_t* unit);
void battle_unit_update_staged_status_data(u16 unit_id, u16 restore);
s32 battle_unit_set_placement_and_validate(s32 unit_index, battle_deployed_coords_t* src);
s32 battle_unit_validate_placement(s32 unit_index);
void battle_unit_view_supervisor_thread(void);

/* update */
s32 battle_update_unit_status_and_staged_status_data();

void* bzero(void* destination, s32 byte_length);
s32 battle_effect_check_direct_trajectory_to_target(s32 source_id, SVECTOR* tile_position, s32 target_id);
const u8* battle_find_text_id_location(const u8* text_section, s32 entry_id);
void main_sound_pause_tracked_sfx(void);
void battle_move_set_velocity_for_contiguous_steps_with_final_tile_height(
    battle_unit_misc_data_t* unit, const u8* path, const u8* step);
void battle_move_apply_unit_step_velocity(battle_unit_misc_data_t* unit);
void battle_move_displace_unit_at_destination_tile(battle_unit_misc_data_t* unit, s32 direction);
void battle_move_finish_unit_step_at_tile_edge(battle_unit_misc_data_t* unit);
void battle_move_update_walking_step_at_tile_edge(battle_unit_misc_data_t* unit);
void battle_camera_step_focus_toward_cursor_tile(s16 height);
s32 battle_unit_find_relocation_tile(s32 unit_index, battle_dismount_coords_t* destination);

s32 battle_move_start_next_post_movement_step(void);
void battle_gfx_run_unit_seq_script(battle_unit_misc_data_t* unit, battle_unit_anim_state_t* state, s32 advance);
void battle_gfx_run_wep_eff_seq_script(
    battle_unit_misc_data_t* unit, battle_unit_sprite_block_t* sprite, s32 index, s32 advance);
void battle_action_queue_post_effect_messages_for_unit(battle_unit_misc_data_t* unit);
void battle_effect_store_targets_and_display_types(s32 ability_id, battle_unit_misc_data_t* unit);
s32 battle_target_calculate_cursor_tile_polygon(battle_screen_coords_t* coords, u8 layer, SVECTOR* quad);
s32 battle_map_calculate_slope_height(const s16* screen_coordinates, u8 map_z);
void battle_gfx_build_cursor_tile_glow(
    s32 palette, s32 shape, SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, u32* ot);
void battle_gfx_draw_map_selection_cursor(SVECTOR* base, u32* ot);
void battle_gfx_draw_wait_direction_arrows(SVECTOR* vector, u32* ot);
s32 battle_gfx_build_next_action_result_display(battle_unit_misc_data_t* unit);
void battle_get_item_graphic_data(SPRT* sprite, s32 item_id);
void battle_gfx_determine_status_bubble_parameters(battle_unit_misc_data_t*);
s32 battle_gfx_claim_spritesheet_slot(s16 spritesheet_id);
void battle_unit_update_animation_for_status_changes(battle_unit_misc_data_t* unit);
void battle_gfx_update_and_animate_unit_wep_eff(battle_unit_misc_data_t* unit);
void battle_gfx_draw_unit_sprite_layers(battle_unit_misc_data_t* unit);
void battle_unit_move_toward_action_target(battle_unit_misc_data_t* unit, s32 allow_height_change);
void battle_move_accelerate_unit_to_destination_with_height_change(battle_unit_misc_data_t* unit);
void battle_move_accelerate_unit_to_destination_no_height_change(battle_unit_misc_data_t* unit);
void battle_menu_draw_active_window_frames(void);
void battle_text_measure_pixels(s16* width, s16* rows, const u8* text);
void battle_menu_build_zoom_draw_area_pair(void* packets, void* geometry, s32 step, s32 flag);
void battle_menu_build_ability_preview_at_list(void);
void battle_menu_run_scrolling_ability_list_thread(void);
void battle_gfx_init_scanline_frame(void);
s32 battle_poll_companion_executable_request(void);
void battle_menu_select_unit_action_slots_thread(void);
s32 battle_menu_resolve_selection(void);
s32 battle_script_run_scenario_conditions(void);
s32 battle_script_check_scenario_condition(s32 a, s32 b, s32 c, s32 d, s32 e);
void battle_script_run_sprite_move(s32 arg, s32 use_speed);
void battle_text_render_glyph_to_4bpp_image(const u8* glyph_bitmap, s32 image, u16* origin, s32 palette_offset);
s32 battle_fixed_cross_product_q12(s32 a, s32 b, s32 c, s32 d);
void battle_move_store_unit_movement_to_scratchpad(s32 unit_id);
s32 battle_move_set_reachable_tiles(s32 unit_id, s32 map_x, s32 map_y, s32 map_z);
void battle_move_set_tile_flags_for_pathfinding(s32 mode);
s32 battle_target_set_green_panels_for_action(battle_ai_command_action_t* action);
s32 battle_target_is_unit_untargetable_and_store_tile(s32 unit_id, s32* tile_index);
s32 battle_action_prepare_attack(battle_ai_command_action_t* action, battle_ai_command_action_t* target, s32 flag);
s32 battle_action_resolve_ability_strike(s32 misc_unit_id, battle_strike_work_t* work);
void battle_action_init_current_ability_strike_data(battle_stats_t* unit);
s32 battle_reaction_prepare_next(u16* reaction_id);
s32 battle_unit_apply_action_rewards(s32 unit_index, battle_action_reward_display_t* display);
s32 battle_unit_init_deployed_units_data(battle_deployed_coords_t* formation, s32 is_red_team_debug);
s32 battle_unit_set_tile_position(s32 misc_unit_id, u8 map_x, u8 map_y, u8 map_z, u8 facing);
s32 battle_menu_get_unit_action_menu_id(battle_stats_t* unit);
s32 battle_menu_load_item_abilities(s32 unit_id, u8 skillset, u8* out_item_ids);
s32 battle_menu_get_unit_skillset_ability_data(
    s32 unit_id, u8 skillset, s16* ability_ids, u8* mp_out, u8* ct_out, s32 unused, u8* flags_out, u8* turns_out);
s32 battle_menu_collect_throwable_items(s32 unit_id, u8 ability, u8* items);
s32 battle_action_run_between_turn_control(s32 mode);
void battle_formula_calculate_knockback(void);
void battle_formula_apply_zodiac_compatibility(void);
void battle_formula_store_physical_evade_values(void);
s32 battle_action_run_pre_formula_setup(const u8* source, u8 target_id);
s32 battle_action_finalize_attack_and_flag_reactions(s32 misc_id);
s32 battle_ability_find_learn_on_hit_unit(s32 attacker_id, u16* ability_id);
s32 battle_map_resolve_rotated_offset_layer(battle_stats_t*, s32, s32, s32, s32*, s32*, s32*);
s32 battle_ai_set_movement_panel_data(s32 movement_taken);
void battle_ai_set_ability_considerations(s32 action_taken);
void battle_ai_evaluate_linear_ability_behaviors(void);
void battle_effect_free_state_record(s16);
void battle_camera_execute_angle_command(s32 selector, s32 frame_high, s32 duration, SVECTOR* position);
void battle_camera_execute_position_command(s32 selector, s32 frame_high, s32 duration, VECTOR* target);
void option_entrypoint(s32 menu_type);
void* attack_deploy_get_coordinate_slots(void);
void require_reward_animate_bonus_image_burst(void);
void helpmenu_run_battle_help_menu(void);
void* game_realloc(void*, u32);
s32 battle_camera_get_input_direction(s32 mode, u32 input);
s32 battle_map_load_data(s32 map_id, s32 kind);
s32 battle_target_move_cursor_by_input(void);
void battle_gfx_prepare_post_action_display(battle_unit_misc_data_t* unit);
/* Returns the event result that the battle state handlers switch on;
 * proven by 0x80078fb4 and 0x800785ac, which both consume it. */
s32 battle_script_run_event_frame(u32* ot, u32 buttons);
void battle_move_update_path_step(battle_unit_misc_data_t* unit);
/* The last argument is an optional transform: the EFFECT overlay's particle
 * handlers pass a 0x58-byte record built on their stack, the battle script
 * opcode passes none. The emitter index arrives as a full word. */
void battle_effect_spawn_emitter_particles(
    s16 effect_index, s32 frame, s32 emitter_index, effect_work_record_t* transform);
void save_3_u16(SVECTOR* output, u16 x, u16 y, u16 z);
void battle_target_set_tile_background_color(s32 mode, s32 unused);
void battle_unit_set_target_animation_from_attack_type(
    battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target);
void battle_action_set_target_coords_and_attacker_anim(void);
void battle_gfx_set_thrown_item_graphic_palette(s32 graphic_id, battle_unit_misc_data_t* unit);
void battle_map_blend_background_gradient_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue);
void battle_map_blend_ambient_light_color(s32 mode, s32 frame_duration, s32 red, s32 green, s32 blue);
void battle_gfx_update_all_unit_rotation_and_vectors(void);
void battle_camera_update_effect_rotation_and_zoom(void);
void battle_map_update_palette_colors_inner(s32 mode, s32 palette_group, s32 palette_index, const u16* colors);
void battle_gfx_tint_unit_palette_for_weather_and_tile(battle_unit_misc_data_t* unit, s32 level, s32 final_value);
void update_unit_action_statuses(battle_stats_t* unit, s32 action_state);

struct battle_event_input_state;
struct battle_menu_window_record;
struct world_menu_icon_sprites;
struct world_menu_icon_thread_param;

extern s32 g_battle_post_battle_unit_changes_active;

void battle_action_copy_at_and_cursor_to(void* banner, void* unit, void* billboard, void* cursor_tile);
void battle_menu_configure_frame_cluts(struct battle_menu_window_record* frame);
extern CVECTOR g_battle_map_white_cvector;
/* Default vectors shared with battle_process_map_gns_record, which emits them. */
extern SVECTOR g_battle_map_zero_svector;
extern VECTOR g_battle_map_unit_scale_vector;
/* Write-only: zeroed with the map render state and never read anywhere on
 * the disc. The 150 x 14-byte shape suggests per-instruction parameter
 * storage (an event opcode takes at most 14 parameters), but no code proves
 * it, so the names stay provisional. */
extern u8 D_800F70B4[150][14];
extern u8 D_800F78E8[150];
extern u8 g_battle_map_weather_primary_speed;
extern u8 g_battle_map_rain_secondary_fall_speed;
extern u16 g_battle_menu_anything_ability_id;
extern s16 g_battle_menu_action_slot_selected_option;
extern u8 D_8018F4FC;
extern u8 g_battle_move_pathing_scratch;
extern u8 D_8018F7CC;
extern u8 D_8018F7D0;
extern s32 D_8018F7D8;
/* Per-group trap animation records. The byte at +0x00 of each 0x2e-byte record
 * is an index into g_battle_effect_trap_animation_data; the
 * battle_effect_group_t model (include/fft/battle_effect.h) starts two bytes
 * later, with spawn_start_frame at 0x801b8566, so this table is that same record array
 * addressed from its true base. */
extern u8 g_battle_effect_trap_particle_configs[];
extern s32 D_801B8B98;
extern s32 D_801BC0D4;
/* Written by battle_effect_code_script_05 but never read. */
extern u16 D_801BF000;
extern s16 g_battle_effect_root_record_index;
s32 battle_action_add_war_funds(battle_stats_t* unit, s32 total, u8 item_id);
s32 battle_action_apply_tile_trap(battle_stats_t* unit);
void battle_action_build_targets_post_action_message(void);
/* Preview action calculation entry run on the main stack. */
s32 battle_action_calculate_projected_effect(
    battle_stats_t* attacker, battle_stats_t* target, battle_ai_command_action_t* command);
s32 battle_action_call_attack_preparation_at_preview(u8* arg);
s32 battle_action_check_at_list_for_unit_battle_id(battle_stats_t* unit);
s32 battle_action_check_battle_outcome(void);
void battle_action_check_mp_switch_distribute_and_damage_split_usability(void);
void battle_action_check_reflect_blade_grasp_and_arrow_guard(void);
void battle_action_check_stat_save_and_restore_reaction_usability(void);
s32 battle_action_decrement_player_item_quantity(battle_stats_t* unit, s32 item_id, s32 always_one);
void battle_action_dispatch_target_reaction_ability(void);
s32 battle_action_get_elemental_ability_id(battle_stats_t* unit);
void battle_action_finalize_draw_out_katana_result(battle_stats_t* attacker, battle_strike_work_t* work, s32 hit_count);
void battle_action_handle_steal_exp(battle_stats_t* unit, u8 amount);
s32 battle_action_perform_reaction_ability(void);
s32 battle_action_preview_at_list(battle_stats_t* unit, s32 action, s32 at_list);
s32 battle_action_remove_broken_or_stolen_equipment(void);
void battle_action_run_main_reaction_and_flag_job_level_change(battle_stats_t* unit);
s32 battle_action_select_auto_potion_item(battle_stats_t* unit);
void battle_action_set_casting_unit_id_ff_and_init(void);
s32 battle_action_set_mimic_ability(battle_stats_t* unit);
s32 battle_action_sort_at_list(s32 unit_id, s32 mode, u16 sort_key, battle_at_entry_t* list);
void battle_action_store_ability_data(u8* src);
s32 battle_action_store_counter_ability(battle_stats_t* unit, s8 skillset_id, s16 ability_id, s32 validate_target);
void battle_clear_menu_render_buffer(void* buffer, s32 bytes);
void battle_effect_build_secondary_init_from_action(
    battle_unit_misc_data_t* unit, battle_effect_secondary_init_t* init);
void battle_effect_call_build_secondary_init(battle_unit_misc_data_t* unit);
void battle_effect_init_altima_teleport_data(
    battle_unit_misc_data_t* unit, battle_effect_secondary_init_t* teleport_data);
s32 battle_effect_init_ninja_ball_secondary(s32 elements, battle_effect_secondary_init_t* source);
void battle_effect_set_evade_type_data_item_and_throw_stone_hardcoding(battle_unit_misc_data_t* unit);
void battle_effect_set_secondary_death_smoke(battle_unit_misc_data_t* unit);
void battle_effect_set_secondary_venom_trap(battle_unit_misc_data_t* unit);
void battle_effect_set_secondary_zodiac_poof(battle_unit_misc_data_t* unit);
void battle_formula_apply_poach_and_train(void);
s32 battle_formula_calculate_dance_song_hit(void);
s32 battle_formula_calculate_elemental_xa_times_ya(void);
s32 battle_formula_calculate_friendly_magic_accuracy(void);
s32 battle_formula_calculate_golem_accuracy(void);
s32 battle_formula_calculate_magic_accuracy(void);
s32 battle_formula_calculate_magic_accuracy_no_elemental(void);
s32 battle_formula_calculate_physical_accuracy(void);
s32 battle_formula_calculate_physical_status_accuracy(void);
s32 battle_formula_calculate_weapon_damage(void);
void battle_gfx_animate_post_action_text(battle_unit_misc_data_t* unit, const u16* screen_coords);
void battle_gfx_apply_status_spritesheet_change(battle_unit_misc_data_t* unit, s32 tile_effect_level);
s32 battle_gfx_build_next_special_action_result_display(battle_unit_misc_data_t* unit);
void battle_gfx_calculate_sprite_shadow_from_tile_slope(battle_unit_misc_data_t* unit);
void battle_gfx_draw_unit_shadow(battle_unit_misc_data_t* unit, u32* ot);
void battle_gfx_draw_unit_sprite_position_copies(battle_unit_misc_data_t* unit);
u32 battle_gfx_get_unit_spritesheet_height(battle_unit_misc_data_t* unit);
void battle_gfx_init_item_get_rendering(battle_gfx_render_unit_t* unit);
battle_gfx_sprite_display_data_t* battle_gfx_init_item_sprite_display(s32 unit_index);
battle_gfx_sprite_display_data_t* battle_gfx_init_numeric_sprite_display(
    s32 unit_index, s32 display_section, battle_gfx_sprite_display_data_t* default_display);
void battle_gfx_init_status_bubble(battle_unit_misc_data_t* unit);
battle_gfx_sprite_display_data_t* battle_gfx_init_status_bubble_sprite_display(s32 unit_index);
battle_gfx_sprite_display_data_t* battle_gfx_init_weapon_sprite_display(s32 unit_index, s32 display_type);
void battle_gfx_load_trap_and_unit_frame_parts(
    battle_unit_misc_data_t* unit, battle_unit_anim_state_t* sprite, u16 frame, u16 animation);
void battle_gfx_load_unit_frame_parts(battle_unit_misc_data_t* unit, battle_gfx_sprite_display_data_t* display,
    battle_gfx_source_frame_t* src, s32 sheet_flags);
void battle_gfx_load_unit_into_evtchr_slot(battle_unit_misc_data_t* unit, s32 slot, s32 image_slot);
void battle_gfx_modify_misc_unit_palette(battle_unit_misc_data_t* misc, s32 mode, s32 section_id, s32 unit_misc_id,
    s32 preset, s16 red, s16 green, s16 blue, s32 final_value);
s32 battle_gfx_open_sp2_file(battle_unit_misc_data_t* unit, void* destination);
void battle_gfx_select_unit_shp_frame(battle_unit_misc_data_t* unit, battle_unit_anim_state_t* sprite, u16 frame,
    u16 animation, s32** out_table, s32* out_value);
void battle_gfx_set_sprite_part_flag_bit_0(battle_gfx_sprite_display_data_t* display, s32 part_index, s32 enabled);
void battle_gfx_shift_and_fill_display_svectors(battle_gfx_render_unit_t* unit);
void battle_gfx_store_item_display_data(battle_gfx_render_unit_t* unit, u32 item_id);
void battle_gfx_unpack_unit_shp_data(battle_gfx_unit_shp_frame_tables_t* dst, u8* src);
void battle_gfx_update_misc_unit_status_palette(battle_unit_misc_data_t* misc, s32 tile_effect_level, s32 final_value);
void battle_load_seq_data(u32* destination, const u8* source);
void* battle_map_determine_rare_common_item(battle_stats_t* unit);
battle_move_find_result_data_t* battle_map_set_item_trap_data(u8 x, u8 y, u8 layer);
void* battle_menu_alloc_window_buffer_pair(void);
void battle_menu_build_ability_list(s32 mode);
void battle_menu_build_and_upload_window_image(s32 width, s32 height, s32 rect, s32 mode, s32 tail_offset);
void battle_menu_build_sprite_page(s32 page_index, void* base_screen);
s32 battle_menu_build_unit_action_menus(s32 unit_id, u8* entries, u8* empty, u8* menu_types);
void battle_menu_build_unit_portrait_poly(POLY_FT4* poly, u32 battle_id);
void* battle_menu_build_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset, s32 keep);
s32 battle_menu_call_load_charge_skillset(s32 unit_id, s32 skillset_id, s16* ability_ids);
void battle_menu_check_action_slot_restrictions(s32 menu, s32 value, s32 check_unit);
void battle_menu_clear_status_menu_state(void);
s32 battle_menu_collect_calculator_abilities(s32 unit_id, u8 skillset, s16* out);
s32 battle_menu_collect_math_skill_abilities_by_flags(s32 unit_id, u8 skillset, s16* out, u8 mask);
s32 battle_menu_collect_monster_skill_abilities(s32 unit_id, u8 skillset, s16* out, s32 unused, u8* flags_out);
void battle_menu_confirm_action_silently(void* menu);
s32 battle_menu_copy_ff_terminated_bytes_to_halfwords(s16* dst, u8* src);
void battle_menu_copy_palette_colors(const void* source);
void battle_menu_copy_unit_data_to_status_billboard(battle_stats_t* unit, battle_unit_status_record_t* output);
s32 battle_menu_dispatch_system_function(
    s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled, battle_ai_command_action_t* action);
s32 battle_menu_display_item_inventory_ability(s32 unit_id, s32 skillset, u8* out_item_ids);
s32 battle_menu_load_math_skill_attributes(s32 unit_id, s32 skillset, s16* out_ability_ids);
void battle_menu_draw_numeric_display_entries(
    s32 buffer, menu_number_entry_t* entries, menu_number_position_t* position, s32 count);
void battle_menu_free_buffer(s32 buffer);
s32 battle_menu_get_dead_unit_selection(s32 battle_id);
s32 battle_menu_has_status_effect_for_status_window(s32 unit_id);
s32 battle_menu_init_attack_resources_and_threads(void);
void battle_menu_init_numeric_display_frame_primitives(RECT* bounds, world_menu_palette_primitives_t* menu);
void battle_menu_init_primitive_colors_palette_bank_0(world_menu_palette_primitives_t* menu);
void battle_menu_init_primitive_colors_palette_bank_1(world_menu_palette_primitives_t* menu);
void battle_menu_init_semitransparent_sprt(SPRT* sprite);
void battle_menu_run_icon_selection_loop(void);
void battle_menu_select_sprite_clut(SPRT* sprite);
void battle_menu_set_disabled_texture_window(u8* menu);
void battle_menu_submit_frame_primitives(struct battle_menu_window_record* frame);
void battle_menu_update_selection_from_input(battle_menu_idle_action_entry_t* ctx, s32* sel);
void battle_script_load_event_input_state(const struct battle_event_input_state* saved);
s16 battle_unit_get_camera_facing_quadrant_by_battle_id(u32 battle_id);
void battle_update_menu_cursor_primitives(
    struct world_menu_icon_thread_param* param, struct world_menu_icon_sprites* record, s32 frame, s32 cursor);
s32 battle_menu_load_charge_skillset(s32 unit_id, s32 skillset, s16* abilities, u8* ability_ct, u8* turns);
s32 battle_menu_load_draw_out_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);
s32 battle_menu_collect_draw_out_katanas(s32 unit_id, u8 skillset, u8* out_item_ids);
s32 battle_menu_load_math_skill_multiples(s32 unit_id, s32 skillset, s16* out_ability_ids);
void battle_menu_load_palette_and_text_pointers(void);
s32 battle_menu_load_skillset(s32 unit_id, u8 skillset, u8 menu);
s32 battle_menu_load_throw_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);
void battle_menu_open_help(void);
void battle_menu_open_mini_menu_help(void);
void battle_menu_open_secondary_companion_executable(void);
void battle_menu_preview_ability_turn_at_list(void);
void battle_menu_release_current_thread_slot(void);
void battle_menu_restore_selection_for_selected_unit(s32 menu_id);
void battle_menu_run_companion_overlay_9_10_thread(void);
void battle_menu_store_auto_battle_main_target_id(s32 target_id, s32 battle_id);
void battle_menu_submit_numeric_display_frame_primitives(world_menu_palette_primitives_t* display);
void battle_menu_widen_bytes_to_halfwords(s16* dst, u8* src, s32 count);
void battle_menu_zoom_cursor_frame(const void* source, SPRT* sprite, s32 step);
void battle_noop_80079298(void);
void battle_noop_8007aecc(void);
void battle_noop_8013ccec(void);
void battle_noop_8013da70(void);
void battle_noop_8014a82c(void);
s32 battle_prepare_terrain_poison(battle_stats_t* unit);
s32 battle_reaction_prepare_hamedo_for_pending_action(s32 id, u16* out_ability);
s32 battle_return_one(void);
void battle_script_clear_indexed_variable_bit0(void);
void battle_script_copy_32_bytes(void* destination, const void* source);
s32 battle_script_filter_unit_id_by_mode(u16* out_id, u16* in_id, s32* mode);
u32 battle_script_get_event_initial_repeat_counter(void);
s32 battle_script_is_deployment_running(u32* ot, u32 buttons);
void battle_script_process_pending_requests(void);
s32 battle_script_scale_music_volume(s32 x);
void battle_set_menu_sprite_clut_variant_1(SPRT* sprite);
s32 battle_status_apply_poison_and_regen(battle_stats_t* unit);
void battle_status_apply_unit_action_removal(battle_stats_t* unit);
s32 battle_status_build_upkeep_action(s32 unit_id, battle_stats_t* unit);
s32 battle_status_check_crystal_dead_jump_petrify_treasure(battle_stats_t* unit);
void battle_status_enable_disable_acting(battle_stats_t* unit);
void battle_status_inflict_by_entd_unit_id(s32 entd_id, s32 status, s32 which, s32 arg);
s32 battle_status_init_special_flag_enabling(s32 unit_id);
s32 battle_status_queue_misc_graphics_flag_change(s32 status, s32 enabled, s32 battle_id);
void battle_status_reapply_active_flags(battle_stats_t* unit);
void battle_status_remove(void);
s32 battle_status_remove_transparent(battle_stats_t* unit);
void battle_status_remove_transparent_if_jump_used(battle_stats_t* unit);
void battle_status_update_expiring(s32 unit_id);
s32 battle_target_apply_reflect(battle_stats_t* unit);
void battle_target_apply_vertical_fixed(s32 x, s32 y);
void battle_target_apply_vertical_tolerance(u8 ref_height, u8 tolerance, s32 single_layer);
s32 battle_target_calculate_ability_range_with_map_parameters(battle_stats_t* unit, u8 range);
s32 battle_target_calculate_for_menu_types(const u8* source);
s32 battle_target_count_hit_by_ability(u8* out);
s32 battle_target_list_units_on_panels(u8* list, battle_stats_t* origin);
s32 battle_target_move_cursor_to_battle_id(u32 battle_id);
void battle_target_project_cursor_tile_to_screen(VECTOR* projected, SVECTOR* position, SVECTOR* raised);
s32 battle_target_run_calculator(const battle_ai_command_action_t* source);
void battle_target_set_ability_panels(const u8* source);
s32 battle_target_set_all_panels_targeted_if_targetable(void);
void battle_target_set_coordinates_for_ability(battle_stats_t* acting, battle_strike_work_t* out);
s32 battle_target_set_weapon_attack_panels(battle_ai_command_action_t* source);
s32 battle_target_set_jump_ability_panels(const u8* source);
s32 battle_target_validate_lunging_target(s32 unit_id, s32 x, s32 y, s32 elevation, s32 target_id);
s32 battle_target_validate_weapon_target(const battle_ai_command_action_t* source);
void battle_unit_add_signed_byte_to_height(battle_unit_misc_data_t* unit, s32 delta);
s32 battle_unit_apply_level_up_down_ability(void);
s32 battle_unit_apply_stat_increment_decrement(s32 mod, u8* stat, u8 max, u8 min);
s32 battle_unit_build_deployed_units_data(s32 a0);
s32 battle_unit_build_gained_exp_jp_level_job_level(battle_stats_t* unit, battle_action_reward_display_t* rewards);
void battle_unit_call_bow_hardcoding_by_misc_id(u16 attacker_id, u16 target_id);
void battle_unit_call_set_animation_based_on_status(struct battle_unit_misc_data* unit);
s32 battle_unit_check_chocobo(battle_stats_t* unit);
s32 battle_unit_check_facing_relative_to_camera(s32 unit_id);
battle_unit_misc_data_t* battle_unit_claim_misc_slot(s16 unit_id);
void battle_unit_clear_horizontal_flip_flag(u32 misc_id);
void battle_unit_clear_movement_effect_suppression(u32 misc_id);
void battle_unit_clear_palette_update_suppression(u32 misc_id);
void battle_unit_copy_map_coords_from_misc_id(u32 misc_id, s16* coords);
void battle_unit_copy_misc_data_xyz_values(SVECTOR* destination, const battle_unit_misc_data_t* misc_data);
void battle_unit_copy_misc_screen_coords(u32 misc_id, u16* screen_coords);
void battle_unit_dismount_rider_and_update_display(battle_unit_misc_data_t* rider);
void battle_unit_face_towards_action_target(battle_unit_misc_data_t* unit, battle_unit_misc_data_t* target);
battle_stats_t* battle_unit_find_active_data_pointer(void);
s16* battle_unit_get_event_offset_ptr_by_misc_id(u32 misc_id);
battle_stats_t* battle_unit_get_attacker_data_pointer(void);
s32 battle_unit_get_facing_nibble_by_misc_id(u32 misc_id);
s32 battle_unit_get_map_coords_from_misc_id(u32 misc_id, s16* coords);
battle_unit_misc_data_t* battle_unit_find_any_misc_data_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
battle_unit_misc_data_t* battle_unit_get_overlapping_misc_data_pointer(
    battle_unit_misc_data_t* self, s32 x, s32 y, s32 z);
battle_ai_command_action_t* battle_unit_get_target_id_ptr_by_battle_id(u32 battle_id);
s32 battle_unit_get_tile_center_and_height_by_battle_id(u32 battle_id, battle_screen_coords_t* coords);
s32 battle_unit_add_event_offset_by_misc_id(u32 misc_id, const battle_screen_coords_t* amount);
void battle_unit_increment_or_decrement_height_mod(battle_unit_misc_data_t* unit);
void battle_unit_init_coordinates(struct battle_unit_misc_data* unit);
void battle_unit_init_coordinates_animation_facing(battle_unit_misc_data_t* unit);
void battle_unit_init_deployed_units_data_for_debug_red_team(s32 value);
battle_unit_misc_data_t* battle_unit_init_misc_data(s32 map_x, s32 map_y, s32 map_level, s16 facing, s32 spritesheet_id,
    s16 palette, s16 misc_id, battle_stats_t* stats, u32 flags, u8* data);
s32 battle_gfx_prepare_post_action_display_by_misc_id(u16 misc_id);
s32 battle_unit_learn_from_crystal(battle_stats_t* unit, s32 flags);
s32 battle_unit_load_job_level(battle_stats_t* unit, s32 job_id, s32* out_slot);
void battle_unit_mount_onto_pending_mount(battle_unit_misc_data_t* unit);
s32 battle_unit_prepare_rider_dismount(u32 misc_id, s32 direction, s32 jump_length);
void battle_unit_remove_misc(battle_unit_misc_data_t* unit);
s32 battle_unit_select_and_open_entd(void);
void battle_unit_select_attack_animation_for_ability(
    battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target);
void battle_unit_select_weapon_attack_animation(battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target);
void battle_unit_set_animation_for_item_abilities(battle_unit_misc_data_t* unit, battle_unit_misc_data_t* target);
s32 battle_unit_set_animation_from_facing_nibble(u32 misc_id, s32 facing_nibble);
void battle_unit_set_attacker_animation_for_shield_block(
    battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target);
void battle_unit_set_horizontal_flip_flag(u32 misc_id);
void battle_unit_set_mounted_animation_by_misc_id(u32 misc_id);
void battle_unit_set_move_and_screen_coords(battle_unit_misc_data_t* unit);
void battle_unit_set_movement_effect_suppression(u32 misc_id);
void battle_unit_set_palette_update_suppression(u32 misc_id);
void battle_unit_set_specific_animation_by_misc_id(u32 misc_id, u32 animation);
void battle_unit_set_status_animation_by_misc_id(u32 misc_id);
void battle_unit_shift_forward_or_backward(battle_unit_misc_data_t* unit, s32 delta);
void battle_unit_shift_right_left(battle_unit_misc_data_t* unit, s8 delta);
void battle_unit_store_animation_facing_movement_data(s32 animation, s32 facing, battle_unit_misc_data_t* unit);
void battle_unit_store_entd_flags_without_control_flag(battle_unit_misc_data_t* unit);
s32 battle_unit_store_jp_and_calculate_unlocked_jobs(s32 unit_index, s32 jp, s32 team, s32 slot);
void battle_unit_update_and_animate_units_three_times(void);
void battle_unit_update_attack_result_animation(battle_unit_misc_data_t* unit);
void battle_unit_update_facing_and_pending_animations(void);
void battle_write_packed_nibble(u8* data, s32 index, s32 row, s32 stride, s32 value);
void AddPrims(u32* ot, void* first, void* last); /* follows AddPrim; AddPrims-shaped call */
/* Ability-list window layout at 0x80166994. Column modes: 0 text, 1 number,
 * 2 hidden, 3 alternate number. The columns read the work buffer's u16 text
 * ids and values through the layout's s16 pointers. */
extern world_menu_text_layout_t g_battle_ability_menu_layout;
extern s16 g_battle_acting_unit_used_ability_id;
extern battle_stats_t* g_battle_action_attacker;
extern u8 g_battle_action_menu_row_types[]; /* per-row skillset kind; 13 keeps the row active */
extern s32 g_battle_action_post_action_unit_id;
extern s32 g_battle_active_menu;                  /* active menu */
extern u16 g_battle_animation_speed_forced;       /* 1 while the event speed is forced */
extern s16 g_battle_auto_battle_option_colors[5]; /* text colour per option */
extern s32 g_battle_between_turn_state;
extern s32 g_battle_cursor_previous_x;     /* cursor X before this step */
extern s32 g_battle_cursor_previous_y;     /* cursor Y before this step */
extern s32 g_battle_cursor_previous_z;     /* cursor level before this step */
extern u32 g_battle_cursor_repeat_counter; /* free-cursor repeat counter */
extern s16 g_battle_cursor_screen_y;
extern s32 g_battle_entd_selection_mode;
/* Per-byte status removal masks applied while an event stages a unit: three
 * eight-byte sets, of which the first BATTLE_STATUS_BYTE_COUNT bytes are used
 * (0x80169758). */
enum {
    BATTLE_EVENT_STATUS_MASK_BATTLE_STARTED = 0,
    BATTLE_EVENT_STATUS_MASK_PRE_BATTLE = 1,
    BATTLE_EVENT_STATUS_MASK_PRE_BATTLE_UNDEAD = 2,
    BATTLE_EVENT_STATUS_MASK_COUNT = 3,
};

extern u8 g_battle_event_status_masks[BATTLE_EVENT_STATUS_MASK_COUNT][8];
extern u8
    g_battle_event_thread_status_snapshot[]; /* per-thread running flags sampled before the yield, indexed 1..15 */
/* Biased base: encoded event-unit IDs 0x64..0x67 reach the four halfwords at
 * g_battle_event_unit_slots without subtracting 0x64 in the target. */
extern u16 g_battle_event_unit_slots_by_encoded_id[];
extern s32 g_battle_formation_display_mode;
extern void (*g_battle_formula_handlers[])(void);
extern MATRIX g_battle_gfx_numeric_display_matrix;
extern SVECTOR g_battle_gte_rtps_vxy0;
extern SVECTOR g_battle_gte_rtpt_vxy0;
extern SVECTOR g_battle_gte_rtpt_vxy1;
extern SVECTOR g_battle_gte_rtpt_vxy2;
extern s16 g_battle_item_icon_clut_base_x;
extern s16 g_battle_item_icon_clut_base_y;
extern world_image_location_t g_battle_item_icon_image_location;
extern RECT g_battle_menu_ability_list_clut_rect;
extern s16 g_battle_menu_ability_scroll_offset;
extern s16 g_battle_menu_action_type_modes[][2];
extern void* g_battle_menu_buffer_slots[6];
extern s16 g_battle_menu_height_display_suppressed;
extern s32 g_battle_menu_hide_numeric_values; /* non-zero: draw digits as blanks (0x48, 0x10) */
extern world_gfx_image_load_parameters_t g_battle_menu_icon_strip_image_params[44];
extern u16 g_battle_menu_input_block_frames;
extern RECT g_battle_menu_number_glyph_rect;   /* glyph source rectangle in the number/sign font */
extern s16 g_battle_menu_pending_selection[4]; /* 4 shorts */
extern u8 g_battle_menu_quad_group_starts[];   /* first piece index per group; [n + 1] ends group n */
extern s32 g_battle_menu_resolved_command;
extern u8 g_battle_menu_restriction_message_ids[];
extern s16 g_battle_menu_result;            /* menu result: -4 pending, -1 cancelled */
extern s16 g_battle_menu_row_message_ids[]; /* per-row message id */
/* Action command staged by the battle menu and passed whole to
 * battle_action_preview_at_list; the WORLD twin is g_world_menu_preview_action. */
extern battle_ai_command_action_t g_battle_menu_selected_action;
extern world_gfx_image_load_parameters_t g_battle_menu_single_icon_image_params[5];
extern u8 g_battle_menu_skillset_disabled_flags[]; /* per-row greyed flag */
extern battle_unit_status_record_t g_battle_menu_status_billboard;
extern u8 g_battle_menu_text_origin_x[];
extern u8 g_battle_menu_text_origin_y[];
/* Twelve crop percentages, indexed by step. */
extern u16 g_battle_menu_zoom_percentages[];
extern u8 g_battle_mimic_pending_flags[21];
extern s16 g_battle_music_set_track_request;
extern u32 g_battle_otags[2][0x180];
/* Player name buffer substituted for text command 0xE0, 0xFE-terminated;
 * 0x801720cf is its last byte. */
extern u8 g_battle_player_name[20];
extern s16 g_battle_preview_target_unit_id;
/* Count and x/y/level byte triples of the tiles already handed out as dismount
 * destinations this action. */
extern s32 g_battle_relocated_unit_count;
extern u8 g_battle_relocated_unit_coords[][3];
extern u16 g_battle_saved_animation_speed; /* animation speed saved while an event thread forces speed 2 */
extern s16 g_battle_scenario_event_active;
extern DR_MODE g_battle_screen_fade_draw_modes[];
extern u32 g_battle_script_event_current_input;
extern u32 g_battle_script_event_initial_repeat_counter;
extern u32 g_battle_script_event_previous_input;
extern u32 g_battle_script_event_repeat_counter;
extern u32 g_battle_script_event_secondary_repeat_counter;
extern s32* g_battle_script_variables;
extern u8 g_battle_spell_quote_exception_skillsets[];
extern SVECTOR g_battle_status_bubble_zoom;
extern u16 g_battle_target_color_tile_x;
extern u16 g_battle_target_color_tile_y;
extern s16 g_battle_target_tile_mark_modes[3];
extern RECT g_battle_text_large_number_glyph_rect; /* glyph source rectangle in the large number font */
extern menu_text_state_t
    g_battle_text_typewriter_state;             /* origin/stride passed to battle_text_render_glyph_to_4bpp_image */
extern RECT g_battle_text_typewriter_vram_rect; /* VRAM destination of the finished glyph */
extern s32 g_battle_turn_unit_id;
extern RECT g_battle_unit_editor_cursor_rect; /* cursor source */
extern RECT g_battle_unit_editor_panel_rect;  /* panel box */
extern RECT g_battle_unit_editor_text_rect_a; /* first text image */
extern RECT g_battle_unit_editor_text_rect_b; /* second text image */
extern s32 g_battle_unit_graphics_load_complete;
extern battle_unit_misc_data_t* g_battle_unit_last_misc_data;
extern battle_unit_misc_data_t* g_battle_unit_misc_list_head;
/* Per-unit pending animation id, applied once the unit stops moving. */
extern s16 g_battle_unit_pending_animation_ids[];
extern battle_stats_t g_battle_unit_stats[BATTLE_UNIT_SLOT_COUNT];
extern u16 g_battle_unit_status_bounce_offsets[];
extern u8 g_battle_unit_status_first_page_by_mode[];
extern CVECTOR g_battle_unit_status_gauge_colors[];
extern world_gfx_image_load_parameters_t g_battle_unit_status_icon_image_params[];
extern u16 g_battle_unit_status_quad_clut_pairs[][2];
extern world_gfx_image_load_parameters_t* g_battle_unit_status_quad_image_params_by_mode[];
extern u16 g_battle_unit_status_sprite_clut_pairs[][2];
extern world_gfx_image_load_parameters_t* g_battle_unit_status_sprite_image_params_by_mode[];
extern RECT g_battle_unit_summary_panel_rect;
extern world_gfx_image_load_parameters_t g_battle_unit_summary_portrait_image_params;
extern world_gfx_image_load_parameters_t g_battle_unit_summary_sprite_image_params[];
extern s16 g_battle_unit_was_moving_latch[];
/* Provisional: the 12-entry zodiac compatibility table indexed by the absolute
 * difference of the two signs.  1/4 weaken the formula, 2/5 strengthen it, and
 * 3 marks the opposition pair whose result depends on sex. */
extern u8 g_battle_zodiac_compatibility_modifiers[];
extern s32 g_companion_overlay_state;
extern u8 g_current_ability_canceled_statuses[];
extern s32 g_menu_text_param_22; /* 0x80166020: record +0x22 */
extern s32 g_menu_text_param_24; /* 0x80166024: record +0x24 */
extern const u8* g_text_glyph_bitmap_data;
/* Texture coordinates for the status bubble graphics, indexed by status bubble
 * id (0x800949dc/0x800949f4) and by a counter digit (0x80094a0c/0x80094a24).
 * Each table holds 24 bytes; the pairs are read together as one u/v. */
extern u8 g_battle_status_bubble_u[24];
extern u8 g_battle_status_bubble_v[24];
extern u8 g_battle_status_bubble_digit_u[24];
extern u8 g_battle_status_bubble_digit_v[24];
void battle_unit_start_post_attack_animation_display(s32 flag, u8* ctx);
s32 battle_effect_start_altima_teleport_arrival(battle_unit_misc_data_t* unit);
s32 battle_effect_start_altima_teleport_departure(battle_unit_misc_data_t* unit);
void battle_effect_start_altima_teleport_fade_out(battle_unit_misc_data_t* unit);
void battle_effect_start_altima_teleport_white_flash(battle_unit_misc_data_t* unit);
extern battle_stats_t battle_stats[BATTLE_UNIT_SLOT_COUNT];

void battle_menu_open_illegal_move_help(void);
s32 battle_process_map_gns_record(s32 phase, gns_command_record_prefix_t* record);
s32 battle_spread_targeting_panel_to_neighbors(s32 y, s32 x);
void battle_target_build_directional_attack_panels(const void* source, u8 mode);
void battle_target_check_moldball_virus_depth(s16 ability_id);
u8 battle_unit_generate_treasure(battle_stats_t* unit);

#endif
