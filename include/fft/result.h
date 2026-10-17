#ifndef RESULT_H
#define RESULT_H

/* Action result displays: the numbers and words that pop up over a unit, their selectors, images and digit zoom
 * curves, the projected-action display's images, and queuing and building each display. */

#include "fft/gfx.h"
#include "fft/sprite.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Pending post-action displays are queued as one little-endian 32-bit value in runtime
 * Unit Data 0x1b8..0x1bb. result_build_next_display and
 * result_build_next_special_display consume one bit at a time in ascending
 * order. */
typedef enum action_display_flag {
    ACTION_DISPLAY_FLAG_HP_DAMAGE = 0x00000001,
    ACTION_DISPLAY_FLAG_HP_HEALING = 0x00000002,
    ACTION_DISPLAY_FLAG_MP_DAMAGE = 0x00000004,
    ACTION_DISPLAY_FLAG_MP_HEALING = 0x00000008,
    ACTION_DISPLAY_FLAG_SPEED_DAMAGE = 0x00000010,
    ACTION_DISPLAY_FLAG_SPEED_GAIN = 0x00000020,
    ACTION_DISPLAY_FLAG_CT_DAMAGE = 0x00000040,
    ACTION_DISPLAY_FLAG_CT_GAIN = 0x00000080,
    ACTION_DISPLAY_FLAG_BRAVE_DAMAGE = 0x00000100,
    ACTION_DISPLAY_FLAG_BRAVE_GAIN = 0x00000200,
    ACTION_DISPLAY_FLAG_FAITH_DAMAGE = 0x00000400,
    ACTION_DISPLAY_FLAG_FAITH_GAIN = 0x00000800,
    ACTION_DISPLAY_FLAG_PA_DAMAGE = 0x00001000,
    ACTION_DISPLAY_FLAG_PA_GAIN = 0x00002000,
    ACTION_DISPLAY_FLAG_MA_DAMAGE = 0x00004000,
    ACTION_DISPLAY_FLAG_MA_GAIN = 0x00008000,
    ACTION_DISPLAY_FLAG_LEVEL_DOWN = 0x00010000,
    ACTION_DISPLAY_FLAG_LEVEL_UP = 0x00020000,
    ACTION_DISPLAY_FLAG_STOLEN = 0x00040000,
    ACTION_DISPLAY_FLAG_BROKEN = 0x00080000,
    ACTION_DISPLAY_FLAG_QUICK = 0x00100000,
    ACTION_DISPLAY_FLAG_CT_ZERO = 0x00200000,
    ACTION_DISPLAY_FLAG_MISSED = 0x00400000,
    ACTION_DISPLAY_FLAG_GUARDED = 0x00800000,
    ACTION_DISPLAY_FLAG_CAUGHT = 0x01000000,
    ACTION_DISPLAY_FLAG_GAINED_EXP = 0x02000000,
    ACTION_DISPLAY_FLAG_GAINED_JP = 0x04000000,
    ACTION_DISPLAY_FLAG_NO_TARGET = 0x08000000,
    ACTION_DISPLAY_FLAG_SILENCED = 0x10000000,
    ACTION_DISPLAY_FLAG_NO_MP = 0x20000000
} action_display_flag_e;

/* Numeric values correspond to the first sixteen action display flags. */
enum { ACTION_DISPLAY_VALUE_COUNT = 16, ACTION_DISPLAY_NUMERIC_FLAGS_MASK = 0x0000ffff };

/* Image IDs used by the projected-action display. These index the 49-entry
 * g_result_image_rects; they are not battle status IDs. */
typedef enum action_display_image_id {
    ACTION_DISPLAY_IMAGE_DEAD = 0,
    ACTION_DISPLAY_IMAGE_UNDEAD = 1,
    ACTION_DISPLAY_IMAGE_PETRIFY = 2,
    ACTION_DISPLAY_IMAGE_INVITE = 3,
    ACTION_DISPLAY_IMAGE_DARKNESS = 4,
    ACTION_DISPLAY_IMAGE_CONFUSION = 5,
    ACTION_DISPLAY_IMAGE_SILENCE = 6,
    ACTION_DISPLAY_IMAGE_BLOOD_SUCK = 7,
    ACTION_DISPLAY_IMAGE_OIL = 8,
    ACTION_DISPLAY_IMAGE_FLOAT = 9,
    ACTION_DISPLAY_IMAGE_RERAISE = 10,
    ACTION_DISPLAY_IMAGE_TRANSPARENT = 11,
    ACTION_DISPLAY_IMAGE_BERSERK = 12,
    ACTION_DISPLAY_IMAGE_POISON = 13,
    ACTION_DISPLAY_IMAGE_REGEN = 14,
    ACTION_DISPLAY_IMAGE_PROTECT = 15,
    ACTION_DISPLAY_IMAGE_SHELL = 16,
    ACTION_DISPLAY_IMAGE_HASTE = 17,
    ACTION_DISPLAY_IMAGE_SLOW = 18,
    ACTION_DISPLAY_IMAGE_STOP = 19,
    ACTION_DISPLAY_IMAGE_FAITH = 20,
    ACTION_DISPLAY_IMAGE_INNOCENT = 21,
    ACTION_DISPLAY_IMAGE_CHARM = 22,
    ACTION_DISPLAY_IMAGE_SLEEP = 23,
    ACTION_DISPLAY_IMAGE_DONT_MOVE = 24,
    ACTION_DISPLAY_IMAGE_DONT_ACT = 25,
    ACTION_DISPLAY_IMAGE_REFLECT = 26,
    ACTION_DISPLAY_IMAGE_DEATH_SENTENCE = 27,
    ACTION_DISPLAY_IMAGE_STOLEN = 28,
    ACTION_DISPLAY_IMAGE_BROKEN = 29,
    ACTION_DISPLAY_IMAGE_QUICK = 36,
    ACTION_DISPLAY_IMAGE_HP = 37,
    ACTION_DISPLAY_IMAGE_MP = 38,
    ACTION_DISPLAY_IMAGE_CT = 39,
    ACTION_DISPLAY_IMAGE_SPEED = 40,
    ACTION_DISPLAY_IMAGE_BRAVE = 41,
    ACTION_DISPLAY_IMAGE_FAITH_STAT = 42,
    ACTION_DISPLAY_IMAGE_PHYSICAL_ATTACK = 43,
    ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK = 44,
    ACTION_DISPLAY_IMAGE_LEVEL = 45,
    ACTION_DISPLAY_IMAGE_GIL = 46,
    ACTION_DISPLAY_IMAGE_EXP = 47,
    ACTION_DISPLAY_IMAGE_FROG = 48,
    ACTION_DISPLAY_IMAGE_NONE = 255
} action_display_image_id_e;

enum { ACTION_DISPLAY_IMAGE_COUNT = ACTION_DISPLAY_IMAGE_FROG + 1 };

typedef struct action_display_image_rect {
    u8 u;
    u8 v;
    u8 width;
    u8 height;
} action_display_image_rect_t;
typedef char action_display_image_rect_size_must_be_4[(sizeof(action_display_image_rect_t) == 4) ? 1 : -1];

/* Packed image IDs in the action-result status list. */
enum { ACTION_STATUS_DISPLAY_IMAGE_MASK = 0x7f, ACTION_STATUS_DISPLAY_REMOVAL = 0x80 };

/* Entries of unit_t.status_display_image_list. */
enum { ACTION_STATUS_DISPLAY_CAPACITY = 27 };

/* Block view of the numeric display fields at runtime unit data 0x2bc..0x2cf,
 * taken as one pointer by the action result display builders
 * (result_build_next_display, result_build_next_special_display
 * and result_animate_text). */
typedef struct action_result_display {
    u8 active;                     /* 0x00 */
    u8 _padding_01;                /* 0x01; aligns selector */
    u16 selector;                  /* 0x02; result_display_selector_e */
    u16 value;                     /* 0x04 */
    u16 progress;                  /* 0x06 */
    sprite_display_t* displays[3]; /* 0x08 */
} action_result_display_t;
typedef char action_result_display_size_must_be_20[(sizeof(action_result_display_t) == 20) ? 1 : -1];

/* The low 9 bits select a result graphic; bit 9 alters its motion for a
 * critical hit. Values below 0x10 are also indices into
 * g_result_display_values. */
typedef enum result_display_selector {
    RESULT_DISPLAY_HP_DAMAGE = 0x000,
    RESULT_DISPLAY_HP_HEALING = 0x001,
    RESULT_DISPLAY_MP_DAMAGE = 0x002,
    RESULT_DISPLAY_MP_HEALING = 0x003,
    RESULT_DISPLAY_SPEED_DAMAGE = 0x004,
    RESULT_DISPLAY_SPEED_GAIN = 0x005,
    RESULT_DISPLAY_CT_DAMAGE = 0x006,
    RESULT_DISPLAY_CT_GAIN = 0x007,
    RESULT_DISPLAY_BRAVE_DAMAGE = 0x008,
    RESULT_DISPLAY_BRAVE_GAIN = 0x009,
    RESULT_DISPLAY_FAITH_DAMAGE = 0x00a,
    RESULT_DISPLAY_FAITH_GAIN = 0x00b,
    RESULT_DISPLAY_PA_DAMAGE = 0x00c,
    RESULT_DISPLAY_PA_GAIN = 0x00d,
    RESULT_DISPLAY_MA_DAMAGE = 0x00e,
    RESULT_DISPLAY_MA_GAIN = 0x00f,
    RESULT_DISPLAY_MISSED = 0x010,
    RESULT_DISPLAY_GUARDED = 0x020,
    RESULT_DISPLAY_CAUGHT = 0x030,
    RESULT_DISPLAY_GAINED_EXP = 0x040,
    RESULT_DISPLAY_GAINED_JP = 0x050,
    RESULT_DISPLAY_STATUS_INFLICTED = 0x060,
    RESULT_DISPLAY_STATUS_REMOVED = 0x070,
    RESULT_DISPLAY_LEVEL_DOWN = 0x080,
    RESULT_DISPLAY_LEVEL_UP = 0x090,
    RESULT_DISPLAY_STOLEN = 0x0a0,
    RESULT_DISPLAY_BROKEN = 0x0b0,
    RESULT_DISPLAY_QUICK = 0x0c0,
    RESULT_DISPLAY_CT_ZERO = 0x0d0,
    RESULT_DISPLAY_NO_TARGET = 0x0e0,
    RESULT_DISPLAY_SILENCED = 0x0f0,
    RESULT_DISPLAY_NO_MP = 0x100
} result_display_selector_e;

/* Encoding in action_result_display_t.selector. */
enum { RESULT_DISPLAY_SELECTOR_MASK = 0x1ff, RESULT_DISPLAY_MODIFIER_CRITICAL = 0x200 };

extern const u8 g_result_numeric_selectors[ACTION_DISPLAY_VALUE_COUNT];
extern u16 g_result_display_values[ACTION_DISPLAY_VALUE_COUNT];
extern action_display_image_rect_t g_result_image_rects[ACTION_DISPLAY_IMAGE_COUNT];
extern const palette_t g_result_palette;
extern s16 g_result_digit_0_zoom_curve[21];
extern s16 g_result_digit_0_critical_zoom_curve[21];
extern s16 g_result_digit_1_zoom_curve[21];
extern s16 g_result_digit_1_critical_zoom_curve[21];
extern s16 g_result_digit_2_zoom_curve[21];
extern s16 g_result_digit_2_critical_zoom_curve[21];

/* Eight per status_changes mask byte, indexed from the byte's high bit. */
extern u8 g_result_status_image_ids[STATUS_BYTE_COUNT][8];

void result_prepare_display(unit_t* unit);
s32 result_prepare_display_by_runtime_id(s32 runtime_id);
s32 result_build_next_display(unit_t* unit);
s32 result_build_next_special_display(unit_t* unit);
void result_init_earned_exp_jp_display(struct unit* unit);
void result_queue_charge_failure_message(unit_t* unit, s32 mode);
void result_animate_text(unit_t* unit, const SVECTOR* screen_coords);

#endif
