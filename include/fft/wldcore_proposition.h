#ifndef WLDCORE_PROPOSITION_H
#define WLDCORE_PROPOSITION_H

/* WLDCORE's propositions: dispatch, participants, reports and rewards. */

#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* proposition */
extern wldcore_proposition_report_t g_wldcore_proposition_report;
extern s32* g_wldcore_proposition_data_base;
extern s32 g_wldcore_proposition_selected_entry;
extern s32 g_wldcore_proposition_send_unit_count;
extern s32 g_wldcore_proposition_gil_amount;
extern s32 g_wldcore_proposition_send_units[];
extern s16 g_wldcore_proposition_candidate_levels[];
extern s32 g_wldcore_proposition_dispatch_days;
/* Saved brave-story row, declared as an array for the same reason as
 * g_wldcore_active_propositions_saved_cursor. */
extern s32 g_wldcore_proposition_category_saved_cursor[];
extern u32 g_wldcore_proposition_detail_image_buffer[];
void wldcore_proposition_handle_report_confirm_input(wldcore_proposition_report_confirm_level_t* level);
void wldcore_proposition_determine_success(void);
void wldcore_proposition_calculate_base_jp_and_gil(void);
void wldcore_proposition_roll_bonus_reward(void);
void wldcore_proposition_apply_extra_days_multiplier(void);
s32 wldcore_proposition_adjust_message_index(s32 base, s32 party_index);
void wldcore_proposition_advance_daily_counters(void);
s32 wldcore_proposition_count_completed(s32 location);
/* File records have different layouts; the selected record determines the table type. */
void* wldcore_proposition_get_data_pointer(wldcore_proposition_data_record_e record);
s32 wldcore_proposition_load_picture(s32 index);
void wldcore_proposition_load_picture_data_set(s32 value);
void wldcore_proposition_submit_marker_request(GsOT* ot);
void wldcore_proposition_push_detail_level(s32 value);
s32 wldcore_proposition_is_available_at_location(s32 index, s32 location);
void wldcore_proposition_push_dispatch_confirm_level(void);
void wldcore_proposition_push_send_unit_level(void);
s32 wldcore_proposition_step_participant_message(wldcore_proposition_report_level_t* state, s32 message, s32 speaker);
s32 wldcore_proposition_find_preferred_job_unit(void);

/* menu */
void wldcore_proposition_handle_cancel_confirm_input(wldcore_menu_stack_record_t* level);

#endif
