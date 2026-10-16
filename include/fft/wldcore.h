#ifndef WLDCORE_H
#define WLDCORE_H

/* WLDCORE.BIN, the world map program (the map, the bar, propositions, the Brave Story and sound novels): its entry and
 * frame loop, threads, controller input, calendar and party units. Its topic headers hold the rest. */

#include "fft/data.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/menu.h"
#include "fft/menu_panel.h"
#include "fft/unit.h"
#include "psx/types.h"

/* entry and frame loop */
/* g_main_system_flags as wldcore_process_frame_gate last saw it (main_system_flags_e). */
extern u32 g_wldcore_previous_system_flags;
s32 wldcore_entrypoint(void);
void wldcore_switch_to_stack(void*);
void wldcore_restore_previous_stack(void);
/* Empty; wldcore_init_subsystems calls it between the map dot and location marker setup. */
void wldcore_map_init_stub(void);
void wldcore_system_init_world_menus_and_date(void);

/* threads */
/* Slots that the WLDCORE screens use for threads of their own. */
typedef enum wldcore_thread_slot {
    WLDCORE_THREAD_SLOT_TRANSITION = 4,    /* the zoom thread of a screen transition */
    WLDCORE_THREAD_SLOT_WINDOW_PANEL = 12, /* a list or number panel */
    WLDCORE_THREAD_SLOT_MESSAGE = 14,      /* the message box and script text */
} wldcore_thread_slot_e;

/* Parameter blocks of WORLD's status banner, numeric editor and equipment/ability panel threads, which WLDCORE runs
 * in thread slots 8, 9 and 12. */
extern menu_status_thread_t g_wldcore_thread8_params;
extern menu_status_thread_t g_wldcore_thread9_params;
extern menu_status_thread_t g_wldcore_threadc_params;

/* controller input */
extern u32 g_wldcore_input_new_button_presses;
extern s32 g_wldcore_input_repeat_counter_up;
extern s32 g_wldcore_input_repeat_counter_down;
extern s32 g_wldcore_input_repeat_counter_left;
extern s32 g_wldcore_input_repeat_counter_right;
extern s32 g_wldcore_input_repeat_counter_r1;
extern s32 g_wldcore_input_repeat_counter_l1;
extern s32 g_wldcore_input_repeat_initial_delay;
extern s32 g_wldcore_input_repeat_period;
extern s32 g_wldcore_input_secondary_repeat_period;
void wldcore_input_init_repeat_state(void);
u32 wldcore_input_update_repeat(u32 buttons);
void wldcore_input_update_controller(void);

/* calendar */
/* Screen position of the date wldcore_gfx_draw_calendar_date draws. */
extern s32 g_wldcore_calendar_display_x;
extern s32 g_wldcore_calendar_display_y;
/* Twelve {month, day} pairs: the calendar date each zodiac sign starts on,
 * Aries (3/21) first. */
extern calendar_date_t g_wldcore_calendar_zodiac_start_dates[ZODIAC_SIGN_ORDINARY_COUNT];
void wldcore_calendar_advance_day(void);
void wldcore_calendar_advance_days(s32 days);
void wldcore_calendar_calculate_month_and_day(s32 day_of_year, s32* output_month, s32* output_day);
void wldcore_calendar_convert_date_to_zodiac(s32* month, s32* day);
/* Dates kept as nine-bit records in a bit array (day in bits 0-4, month in bits 5-8), as the save's acquisition and
 * discovery date bitmaps hold them. */
void wldcore_calendar_write_date(u32* bits, s32 record, calendar_date_t* source);
void wldcore_calendar_read_date(u32* bits, s32 record, calendar_date_t* dest);

/* bit arrays */
void wldcore_set_bit_value(u32* bits, s32 index, s32 set);
s32 wldcore_test_bit(u32* bits, s32 index);

/* files */
void wldcore_file_poll_vram_image_stream(main_file_load_descriptor_t* stream);
void wldcore_file_seek_to_battle_bin(void);

/* party units */
/* The unit wldcore_unit_build_status_panel_data loads from a party slot for the status panels. */
extern unit_stats_t g_wldcore_status_unit;
party_unit_t* wldcore_get_party_unit(s32 party_index);
void wldcore_unit_build_status_panel_data(s32 party_index);

#endif
