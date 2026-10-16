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
/* g_system_flags as world_map_update_backdrop last saw it (system_flags_e). */
extern u32 g_world_map_previous_system_flags;
s32 world_map_entrypoint(void);
void world_map_switch_to_stack(void*);
void world_map_restore_previous_stack(void);
/* Empty; world_map_init_subsystems calls it between the map dot and location marker setup. */
void world_map_init_stub(void);
void world_map_init_menus_and_date(void);

/* threads */
/* Slots that the WLDCORE screens use for threads of their own. */
typedef enum world_map_thread_slot {
    WORLD_MAP_THREAD_SLOT_TRANSITION = 4,    /* the zoom thread of a screen transition */
    WORLD_MAP_THREAD_SLOT_WINDOW_PANEL = 12, /* a list or number panel */
    WORLD_MAP_THREAD_SLOT_MESSAGE = 14,      /* the message box and script text */
} world_map_thread_slot_e;

/* Parameter blocks of WORLD's status banner, numeric panel and equipment/ability panel threads, which WLDCORE runs
 * in thread slots 8, 9 and 12. */
extern menu_status_thread_t g_unit_panel_banner_params;
extern menu_status_thread_t g_unit_panel_numbers_params;
extern menu_status_thread_t g_unit_panel_panel_params;

/* controller input */
extern u32 g_world_map_input_new_button_presses;
void world_map_init_input_repeat(void);
u32 world_map_update_input_repeat(u32 buttons);
void world_map_read_pad(void);

/* calendar */
/* Screen position of the date calendar_draw_date draws. */
extern s32 g_calendar_display_x;
extern s32 g_calendar_display_y;
/* Twelve {month, day} pairs: the calendar date each zodiac sign starts on,
 * Aries (3/21) first. */
extern calendar_date_t g_calendar_zodiac_start_dates[ZODIAC_SIGN_ORDINARY_COUNT];
void calendar_advance_day(void);
void calendar_advance_days(s32 days);
void calendar_calculate_month_and_day(s32 day_of_year, s32* output_month, s32* output_day);
void calendar_convert_date_to_zodiac(s32* month, s32* day);
/* Dates kept as nine-bit records in a bit array (day in bits 0-4, month in bits 5-8), as the save's acquisition and
 * discovery date bitmaps hold them. */
void calendar_write_date(u32* bits, s32 record, calendar_date_t* source);
void calendar_read_date(u32* bits, s32 record, calendar_date_t* dest);

/* bit arrays */
void bits_set_value(u32* bits, s32 index, s32 set);
s32 bits_test(u32* bits, s32 index);

/* files */
void world_map_poll_texture_stream(file_load_descriptor_t* stream);
void event_transition_seek_battle_bin(void);

/* party units */
/* The unit unit_panel_build_unit_data loads from a party slot for the status panels. */
extern unit_stats_t g_unit_panel_unit;
party_unit_t* party_get_roster_unit(s32 party_index);
void unit_panel_build_unit_data(s32 party_index);

#endif
