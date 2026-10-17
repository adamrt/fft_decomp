#ifndef MENU_INPUT_H
#define MENU_INPUT_H

/* Menu input: who owns the input, the per-frame event input and menu controller state with their key repeat, repeat
 * timing profiles and the on-screen pressable buttons. */

#include "psx/types.h"

/* input ownership */
/* The controller or tutorial event that drives the scene menus. */
typedef enum menu_input_mode {
    MENU_INPUT_PLAYER = 0,   /* the controller */
    MENU_INPUT_BLOCKED = 1,  /* the event frame returns early and no idle action menu is built;
                                no code stores this value */
    MENU_INPUT_TUTORIAL = 2, /* a tutorial event supplies the input; set by menu_start_entry_thread */
} menu_input_mode_e;

extern u16 g_menu_input_mode; /* menu_input_mode_e */

/* Modal/message ownership suppresses menu input and dims palettes. BATTLE and WORLD each define this mode;
 * EVENT overlays use the resident BATTLE binding. */
extern s32 g_menu_input_lock_mode;

/* Nonzero while a modal selection, panel or companion overlay owns the input; the companion overlay openers
 * set it before loading and the overlay loaders clear it once the overlay is torn down. */
extern u16 g_menu_modal_open;

/* Frames left before menu input is accepted again; the frame update counts it down. */
extern u16 g_menu_input_block_frames;

/* Returns the input-lock mode; the argument is ignored. */
s32 menu_input_get_lock_mode(s32 unused_0);
void menu_set_modal_open(void);
void menu_clear_modal_open(void);

/* 1 when a menu thread may act on input: no previous thread running, no stop request, the input not locked,
 * no help window open and no block frames left. In WORLD an open Help box also blocks input for five frames. */
s32 menu_can_accept_input(void);

/* Clears the input while it is locked or a help window is open; returns 1 once this thread has a stop request
 * (parameter 3) and no other running thread does. WORLD's body also opens the entry's announcement text on
 * SELECT. */
s32 menu_should_close_thread(u32* input);

/* event input */
/* menu_input_update_event_state runs once per frame: it derives the event input from the controller, with
 * edge detection and key repeat, and yields. WORLD, BUNIT and CARD build full bodies, BUNIT's and CARD's
 * keeping their state in the g_input_* globals; ATTACK, DEBUGCHR, EQUIP and REQUIRE link an empty one. */
typedef struct menu_input_state {
    u32 new_buttons;              /* 0x00 */
    u32 current_buttons;          /* 0x04 */
    u32 previous_buttons;         /* 0x08 */
    u32 initial_repeat_counter;   /* 0x0c */
    u32 repeat_counter;           /* 0x10 */
    u32 secondary_repeat_counter; /* 0x14 */
} menu_input_state_t;
typedef char menu_input_state_size_must_be_24[sizeof(menu_input_state_t) == 24 ? 1 : -1];

extern u32 g_previous_input;

void menu_input_update_event_state(u32* otag, u32 input, s32 count_frame);

/* menu controller */
/* menu_input_update_controller publishes the newly pressed buttons and two repeat rates for the menus, with
 * one repeat counter per button. */
extern u32* g_menu_input_controller;
extern u16 g_menu_input_newly_pressed;
extern u16 g_menu_input_previous;
extern u16 g_menu_input_latched_button;
extern u8 g_menu_input_activation_timer;
extern u32 g_menu_input_primary_repeat;
extern u32 g_menu_input_secondary_repeat;
extern s32 g_menu_input_repeat_counters[16];

/* Buttons OPEN and WLDCORE keep from their menus, -1 for all. */
extern s32 g_menu_blocked_buttons;

void menu_input_update_controller(void);
void menu_input_clear_state(void);

/* EQUIP, JOBSTTS and WORLD each build a separate body. */
void menu_input_update_with_message_state(void);

/* repeat timing */
/* Six-byte row selected by the finger cursor repeat speed option (game options bits 9-11). The first three
 * bytes set the shared initial, primary and secondary input-repeat timings. */
typedef struct menu_input_timing_profile {
    u8 initial_delay;
    u8 repeat_period;
    u8 secondary_repeat_period;
    u8 scroll_accel_delay; /* copied to g_menu_scroll_accel_delay */
    u8 scroll_slow_step;   /* copied to g_menu_scroll_slow_step */
    u8 scroll_fast_step;   /* copied to g_menu_scroll_fast_step */
} menu_input_timing_profile_t;

extern u8 g_menu_scroll_accel_delay;
extern u8 g_menu_scroll_slow_step;
extern u8 g_menu_scroll_fast_step;

/* pressable buttons */
/* The index argument of WORLD's menu_draw_pressable_button and BUNIT's unit_list_draw_pressable_button: an on-screen
 * button, named after the pad button its callers test. */
typedef enum menu_pressable_button {
    MENU_PRESSABLE_BUTTON_L1 = 0,
    MENU_PRESSABLE_BUTTON_R1 = 1,
    MENU_PRESSABLE_BUTTON_ITEM_LIST_TRIANGLE = 2, /* formation item list */
    MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE = 3, /* shop buy lists */
    MENU_PRESSABLE_BUTTON_L2 = 4,
    MENU_PRESSABLE_BUTTON_R2 = 5,
} menu_pressable_button_e;

/* 12-byte piece of a pressable button (menu_draw_pressable_button, unit_list_draw_pressable_button): source
 * rectangle, then its offset from the button origin. */
typedef struct menu_button_piece {
    u16 u;        /* 0x00 */
    u16 v;        /* 0x02 */
    s16 w;        /* 0x04 */
    s16 h;        /* 0x06 */
    u16 x_offset; /* 0x08 */
    u16 y_offset; /* 0x0a */
} menu_button_piece_t;

extern u16 g_menu_button_clut;

/* Each button's first piece; the next button's start ends it. */
extern u8 g_menu_button_piece_starts[];
extern s8 g_menu_button_press_timers[];

void menu_draw_pressable_button(s16 index, s32 x, s16 y, s16 pressed, s16 disabled, u16 sound_effect_id);

#endif
