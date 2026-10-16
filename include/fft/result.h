#ifndef RESULT_H
#define RESULT_H

/* The numbers and words that pop up over a unit after an action: the graphic selectors, their images and digit zoom
 * curves, and queuing and building each display. */

#include "fft/action.h"
#include "fft/gfx.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* The low 9 bits select a result graphic; bit 9 alters its motion for a
 * critical hit. Values below 0x10 are also indices into
 * g_result_display_values. */
typedef enum result_numeric_display_selector {
    RESULT_NUMERIC_DISPLAY_HP_DAMAGE = 0x000,
    RESULT_NUMERIC_DISPLAY_HP_HEALING = 0x001,
    RESULT_NUMERIC_DISPLAY_MP_DAMAGE = 0x002,
    RESULT_NUMERIC_DISPLAY_MP_HEALING = 0x003,
    RESULT_NUMERIC_DISPLAY_SPEED_DAMAGE = 0x004,
    RESULT_NUMERIC_DISPLAY_SPEED_GAIN = 0x005,
    RESULT_NUMERIC_DISPLAY_CT_DAMAGE = 0x006,
    RESULT_NUMERIC_DISPLAY_CT_GAIN = 0x007,
    RESULT_NUMERIC_DISPLAY_BRAVE_DAMAGE = 0x008,
    RESULT_NUMERIC_DISPLAY_BRAVE_GAIN = 0x009,
    RESULT_NUMERIC_DISPLAY_FAITH_DAMAGE = 0x00a,
    RESULT_NUMERIC_DISPLAY_FAITH_GAIN = 0x00b,
    RESULT_NUMERIC_DISPLAY_PA_DAMAGE = 0x00c,
    RESULT_NUMERIC_DISPLAY_PA_GAIN = 0x00d,
    RESULT_NUMERIC_DISPLAY_MA_DAMAGE = 0x00e,
    RESULT_NUMERIC_DISPLAY_MA_GAIN = 0x00f,
    RESULT_NUMERIC_DISPLAY_MISSED = 0x010,
    RESULT_NUMERIC_DISPLAY_GUARDED = 0x020,
    RESULT_NUMERIC_DISPLAY_CAUGHT = 0x030,
    RESULT_NUMERIC_DISPLAY_GAINED_EXP = 0x040,
    RESULT_NUMERIC_DISPLAY_GAINED_JP = 0x050,
    RESULT_NUMERIC_DISPLAY_STATUS_INFLICTED = 0x060,
    RESULT_NUMERIC_DISPLAY_STATUS_REMOVED = 0x070,
    RESULT_NUMERIC_DISPLAY_LEVEL_DOWN = 0x080,
    RESULT_NUMERIC_DISPLAY_LEVEL_UP = 0x090,
    RESULT_NUMERIC_DISPLAY_STOLEN = 0x0a0,
    RESULT_NUMERIC_DISPLAY_BROKEN = 0x0b0,
    RESULT_NUMERIC_DISPLAY_QUICK = 0x0c0,
    RESULT_NUMERIC_DISPLAY_CT_ZERO = 0x0d0,
    RESULT_NUMERIC_DISPLAY_NO_TARGET = 0x0e0,
    RESULT_NUMERIC_DISPLAY_SILENCED = 0x0f0,
    RESULT_NUMERIC_DISPLAY_NO_MP = 0x100,
} result_numeric_display_selector_e;

/* Encoding in action_result_display_t.selector. */
enum {
    RESULT_NUMERIC_DISPLAY_SELECTOR_MASK = 0x1ff,
    RESULT_NUMERIC_DISPLAY_MODIFIER_CRITICAL = 0x200,
};

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
