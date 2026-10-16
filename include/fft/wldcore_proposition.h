#ifndef WLDCORE_PROPOSITION_H
#define WLDCORE_PROPOSITION_H

/* WLDCORE's propositions: dispatch, participants, reports and rewards. */

#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Outcome of a proposition (wldcore_proposition_report_t.result), from the
 * participants' total score. Only SUCCESS completes the proposition and earns
 * its reward. */
typedef enum wldcore_proposition_result {
    WLDCORE_PROPOSITION_RESULT_SUCCESS = 0,     /* score of 99 or more, or the preferred-job roll */
    WLDCORE_PROPOSITION_RESULT_FAILURE = 1,     /* score of 59 to 98: a tenth of the JP, a fifth of the gil */
    WLDCORE_PROPOSITION_RESULT_BAD_FAILURE = 2, /* lower score, or recalled: a twentieth of both */
} wldcore_proposition_result_e;

/* What a successful proposition brought back (wldcore_proposition_report_t.reward_type);
 * TREASURE and UNEXPLORED_LAND are also the proposition row's discovery_kind. */
typedef enum wldcore_proposition_reward {
    WLDCORE_PROPOSITION_REWARD_NONE = 0,
    WLDCORE_PROPOSITION_REWARD_TREASURE = 1,
    WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND = 2,
    WLDCORE_PROPOSITION_REWARD_RANDOM_BONUS = 3, /* the three-tier random reward */
} wldcore_proposition_reward_e;

/* Provisional: proposition-resolution work record at 0x8009f1ec. One object:
 * wldcore_proposition_find_preferred_job_unit reads excluded_party_index at
 * -0xc from its &proposition_index base, and calculate_base_jp_and_gil and
 * step_result_level reach -0x10..+0x24 from single bases. */
typedef struct wldcore_proposition_report {
    s32 excluded_party_index; /* 0x00 */
    s32 speaker_party_index;  /* 0x04; chosen and read back by wldcore_proposition_step_participant_message */
    s32 preferred_unit;       /* 0x08; fixed speaker passed to wldcore_proposition_step_participant_message */
    s32 proposition_index;    /* 0x0c; index into g_main_active_propositions */
    s32 result;               /* 0x10; wldcore_proposition_result_e */
    s32 reward_type;          /* 0x14; wldcore_proposition_reward_e */
    s32 reward_index;         /* 0x18 */
    s32 reward_value;         /* 0x1c */
    s32 recalled;             /* 0x20; reward calculation is skipped after a recall */
    u8 _unused_24[0x10];
    s32 participant_jp[3];     /* 0x34; JP awarded to each participant */
    s32 participant_scores[3]; /* 0x40; success-score contribution */
    s32 participant_gil[3];    /* 0x4c; contribution to the total gil reward */
} wldcore_proposition_report_t;
typedef char wldcore_proposition_report_size_must_be_88[sizeof(wldcore_proposition_report_t) == 88 ? 1 : -1];

/* wldcore_proposition_progress_t.flags of an active proposition. */
typedef enum wldcore_proposition_progress_flag {
    WLDCORE_PROPOSITION_PROGRESS_DISPATCHED = 0x01,
    WLDCORE_PROPOSITION_PROGRESS_RECALLED = 0x02, /* canceled: returns on the next day */
    WLDCORE_PROPOSITION_PROGRESS_RETURNED = 0x04, /* the days are up: waiting for the report */
} wldcore_proposition_progress_flag_e;

/* Value of a proposition's script variable (EVENT_SCRIPT_VAR_PROPOSITION_FIRST + id). */
typedef enum wldcore_proposition_variable_flag {
    WLDCORE_PROPOSITION_VARIABLE_IN_PROGRESS = 0x02,
    WLDCORE_PROPOSITION_VARIABLE_COMPLETED = 0x04,
    WLDCORE_PROPOSITION_VARIABLE_ATTEMPTED = 0x08, /* dispatched at least once */
} wldcore_proposition_variable_flag_e;

/* g_main_proposition_states[]: days until a failed proposition is offered
 * again, and which failure it was. */
typedef enum wldcore_proposition_state {
    WLDCORE_PROPOSITION_STATE_DAYS_MASK = 0x3f,
    WLDCORE_PROPOSITION_STATE_FAILURE = 0x40,
    WLDCORE_PROPOSITION_STATE_BAD_FAILURE = 0x80,
} wldcore_proposition_state_e;

/* Records of the proposition data file (wldcore_proposition_get_data_pointer). */
typedef enum wldcore_proposition_data_record {
    WLDCORE_PROPOSITION_DATA_ROWS = 0,                /* 23-byte proposition rows */
    WLDCORE_PROPOSITION_DATA_FEES = 1,                /* 16-bit values behind the rows' two fee indices */
    WLDCORE_PROPOSITION_DATA_MESSAGE_ADJUSTMENTS = 2, /* by faith and bravery band */
    WLDCORE_PROPOSITION_DATA_CATEGORY_JOB_SCORES = 3,
    WLDCORE_PROPOSITION_DATA_JOB_LIST_JOB_SCORES = 4,
    WLDCORE_PROPOSITION_DATA_BRAVERY_LEVEL_SCORES = 5,
    WLDCORE_PROPOSITION_DATA_FAITH_SCORES = 6,
    WLDCORE_PROPOSITION_DATA_JP_AMOUNTS = 8,
    WLDCORE_PROPOSITION_DATA_GIL_AMOUNTS = 9,
    WLDCORE_PROPOSITION_DATA_DISCOVERY_REWARDS = 10,
    WLDCORE_PROPOSITION_DATA_RANDOM_REWARDS = 11,
    WLDCORE_PROPOSITION_DATA_EXTRA_DAY_JP_SCALES = 12,
    WLDCORE_PROPOSITION_DATA_EXTRA_DAY_GIL_SCALES = 13,
    WLDCORE_PROPOSITION_DATA_JOB_LISTS = 14, /* nine job ids per job list */
} wldcore_proposition_data_record_e;

/* Records 10 and 11 select one entry from each reward-amount table. */
typedef struct wldcore_proposition_reward_pair {
    u8 gil_index;
    u8 jp_index;
} wldcore_proposition_reward_pair_t;

/* wldcore_proposition_report_level_t.load_phase. */
typedef enum wldcore_proposition_report_load {
    WLDCORE_PROPOSITION_REPORT_LOAD_DONE = 0,
    WLDCORE_PROPOSITION_REPORT_LOAD_MESSAGES = 1, /* waits for the audio queue, then loads the message block */
    WLDCORE_PROPOSITION_REPORT_LOAD_PICTURE = 2,  /* waits for it, then loads the reward's picture set */
} wldcore_proposition_report_load_e;

/* wldcore_proposition_result_level_t.phase. */
typedef enum wldcore_proposition_result_phase {
    WLDCORE_PROPOSITION_RESULT_PHASE_INPUT = 0,
    WLDCORE_PROPOSITION_RESULT_PHASE_CLOSING = 1, /* waits for the audio queue and the message thread */
    WLDCORE_PROPOSITION_RESULT_PHASE_DONE = 2,    /* waits for the audio queue, then pops */
} wldcore_proposition_result_phase_e;

/* Which participant reports (proposition row speaker_mode). */
typedef enum wldcore_proposition_speaker {
    WLDCORE_PROPOSITION_SPEAKER_HIGHEST_BRAVERY = 1,
    WLDCORE_PROPOSITION_SPEAKER_LOWEST_BRAVERY = 2,
    WLDCORE_PROPOSITION_SPEAKER_HIGHEST_FAITH = 3,
    WLDCORE_PROPOSITION_SPEAKER_LOWEST_FAITH = 4,
} wldcore_proposition_speaker_e;

typedef enum wldcore_proposition_condition_flag {
    WLDCORE_PROPOSITION_CONDITION_SCRIPT_VAR_6F = 0x01, /* EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY >= minimum */
    WLDCORE_PROPOSITION_CONDITION_MONTH = 0x02,         /* today's zodiac sign (1 = Aries) == condition_value */
    WLDCORE_PROPOSITION_CONDITION_STATUS = 0x04,
} wldcore_proposition_condition_flag_e;

/* Row kinds of the brave-story proposition category list
 * (wldcore_menu_push_proposition_category_level). */
typedef enum wldcore_proposition_category_row {
    WLDCORE_PROPOSITION_CATEGORY_ROW_ACTIVE = 0,
    WLDCORE_PROPOSITION_CATEGORY_ROW_BRAVE_STORY = 1,
} wldcore_proposition_category_row_e;

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
