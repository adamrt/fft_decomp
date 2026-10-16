#ifndef INPUT_H
#define INPUT_H

/* Pad input state that no screen owns: the overlays' per-frame event input and key repeat, MAIN's repeat timing and
 * button edges, and OPEN's directional repeat. */

#include "psx/pad.h"
#include "psx/types.h"

/* event input */
extern u32* g_input_controller;
extern u32 g_input_current;
extern u32 g_input_current_buttons;
extern u32 g_input_event;
extern u32 g_input_polled_buttons;
extern u8 g_input_activation_timer;
extern u8 g_input_lock_timer;
extern u32 g_input_initial_repeat_counter;
extern u32 g_input_repeat_counter;
extern u32 g_input_secondary_repeat_counter;
extern s32 g_input_repeat_counters[16];
extern u8 g_input_page_scroll_disabled;
extern u16 g_input_page_scroll_hold_frames;

/* Defined by OPEN, WLDCORE and MAIN rather than by the menu library. */
extern u32 g_input_previous_buttons;

void input_wait_frames_or_skip(s32 frames);

/* repeat timing and button edges */
/* L1 + R1 + Select + Start restarts the game. */
enum {
    INPUT_RESET_BUTTONS = PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START,
};

extern u8 g_input_repeat_initial_delay;
extern u8 g_input_repeat_period;
extern u8 g_input_secondary_repeat_period;
extern s32 g_input_pressed_buttons;
extern u32 g_input_released_buttons;
extern u32 g_input_pressed_copy_0;
extern u32 g_input_pressed_copy_1;
extern u32 g_input_pressed_copy_2;
extern u32 g_input_pressed_copy_3;
extern u32 g_input_previous_copy_0;
extern u32 g_input_previous_copy_1;
extern u32 g_input_previous_copy_2;
extern u32 g_input_previous_copy_3;
extern u32 g_input_stable_frames;

/* OPEN directional repeat */
extern s32 g_input_directional_repeat_initial_delay;
extern s32 g_input_directional_repeat_period;
extern s32 g_input_up_repeat_counter;
extern s32 g_input_down_repeat_counter;
extern s32 g_input_left_repeat_counter;
extern s32 g_input_right_repeat_counter;

/* Volatile: title_update_sound_type_menu reloads it (lui/lw) for every
 * test; readers that test it twice from one load take a local copy. */
extern volatile u32 g_input_new_button_presses;

void input_init_directional_state(void);
u32 input_update_repeat(u32 buttons);

#endif
