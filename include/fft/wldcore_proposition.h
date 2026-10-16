#ifndef WLDCORE_PROPOSITION_H
#define WLDCORE_PROPOSITION_H

/* WLDCORE's propositions: the proposition data file and its rows, the lists and dispatch, daily progress, and the
 * report with its rewards. */

#include "fft/text.h"
#include "fft/wldcore_menu_level.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* proposition data */
/* Records of the proposition data file (proposition_get_data). */
typedef enum proposition_data_record {
    PROPOSITION_DATA_ROWS = 0,                /* 23-byte proposition rows */
    PROPOSITION_DATA_FEES = 1,                /* 16-bit values behind the rows' two fee indices */
    PROPOSITION_DATA_MESSAGE_ADJUSTMENTS = 2, /* by faith and bravery band */
    PROPOSITION_DATA_CATEGORY_JOB_SCORES = 3,
    PROPOSITION_DATA_JOB_LIST_JOB_SCORES = 4,
    PROPOSITION_DATA_BRAVERY_LEVEL_SCORES = 5,
    PROPOSITION_DATA_FAITH_SCORES = 6,
    PROPOSITION_DATA_JP_AMOUNTS = 8,
    PROPOSITION_DATA_GIL_AMOUNTS = 9,
    PROPOSITION_DATA_DISCOVERY_REWARDS = 10,
    PROPOSITION_DATA_RANDOM_REWARDS = 11,
    PROPOSITION_DATA_EXTRA_DAY_JP_SCALES = 12,
    PROPOSITION_DATA_EXTRA_DAY_GIL_SCALES = 13,
    PROPOSITION_DATA_JOB_LISTS = 14, /* nine job ids per job list */
} proposition_data_record_e;

/* A row of records DISCOVERY_REWARDS and RANDOM_REWARDS: one entry of each reward-amount table. */
typedef struct proposition_reward_pair {
    u8 gil_index;
    u8 jp_index;
} proposition_reward_pair_t;

/* Which participant reports (proposition row speaker_mode). */
typedef enum proposition_speaker {
    PROPOSITION_SPEAKER_HIGHEST_BRAVERY = 1,
    PROPOSITION_SPEAKER_LOWEST_BRAVERY = 2,
    PROPOSITION_SPEAKER_HIGHEST_FAITH = 3,
    PROPOSITION_SPEAKER_LOWEST_FAITH = 4,
} proposition_speaker_e;

/* proposition_fields_t.condition_flags: what must hold for the proposition to be offered. */
typedef enum proposition_condition_flag {
    PROPOSITION_CONDITION_SHOP_AVAILABILITY = 0x01, /* EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY >= minimum */
    PROPOSITION_CONDITION_ZODIAC_SIGN = 0x02,       /* today's zodiac sign (1 = Aries) == condition_value */
    PROPOSITION_CONDITION_STATUS = 0x04,
} proposition_condition_flag_e;

/* One 23-byte proposition row expanded to named 16-bit fields by
 * proposition_unpack_row. */
typedef struct proposition_fields {
    u16 category_index;  /* 0x00; 1-based row of records CATEGORY_JOB_SCORES, DISCOVERY_REWARDS, RANDOM_REWARDS */
    u16 job_list_index;  /* 0x02 */
    u16 id;              /* 0x04 */
    u16 _unused_06;      /* 0x06 */
    u16 fee_a;           /* 0x08; remapped through record FEES */
    u16 fee_b;           /* 0x0a; remapped through record FEES */
    u16 min_days;        /* 0x0c */
    u16 max_days;        /* 0x0e */
    u16 name_text;       /* 0x10; 1-based errand name, text TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1 + n */
    u16 jp_reward;       /* 0x12 */
    u16 discovery_kind;  /* 0x14; proposition_reward_e TREASURE or UNEXPLORED_LAND, else see random_bonus */
    u16 random_bonus;    /* 0x16; 1 enables the three-tier random reward */
    u16 gil_reward;      /* 0x18 */
    u16 _unused_1a[3];   /* 0x1a */
    u16 location;        /* 0x20; 0 for any location */
    u16 condition_flags; /* 0x22; WLDCORE_PROPOSITION_CONDITION_* */
    u16 _unused_24;      /* 0x24 */
    u16 speaker_mode;    /* 0x26; proposition_speaker_e */
    u16 preferred_job;   /* 0x28 */
    u16 required_shop_availability; /* 0x2a; minimum EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY */
    u16 condition_value;            /* 0x2c; zodiac sign or proposition status index */
} proposition_fields_t;
typedef char wldcore_proposition_fields_size_must_be_46[sizeof(proposition_fields_t) == 46 ? 1 : -1];

extern s32* g_proposition_data; /* the loaded proposition data file */
extern proposition_fields_t g_proposition_selected_row[];
/* File records have different layouts; the selected record determines the table type. */
void* proposition_get_data(proposition_data_record_e record);
void proposition_unpack_row(proposition_fields_t* out, s32 index);
s32 proposition_is_available_at(s32 index, s32 location);

/* progress */
/* proposition_progress_t.flags of an active proposition. */
typedef enum proposition_progress_flag {
    PROPOSITION_PROGRESS_DISPATCHED = 0x01,
    PROPOSITION_PROGRESS_RECALLED = 0x02, /* canceled: returns on the next day */
    PROPOSITION_PROGRESS_RETURNED = 0x04, /* the days are up: waiting for the report */
} proposition_progress_flag_e;

/* Value of a proposition's script variable (EVENT_SCRIPT_VAR_PROPOSITION_FIRST + id). */
typedef enum proposition_variable_flag {
    PROPOSITION_VARIABLE_IN_PROGRESS = 0x02,
    PROPOSITION_VARIABLE_COMPLETED = 0x04,
    PROPOSITION_VARIABLE_ATTEMPTED = 0x08, /* dispatched at least once */
} proposition_variable_flag_e;

/* g_proposition_states[]: days until a failed proposition is offered
 * again, and which failure it was. */
typedef enum proposition_state {
    PROPOSITION_STATE_DAYS_MASK = 0x3f,
    PROPOSITION_STATE_FAILURE = 0x40,
    PROPOSITION_STATE_BAD_FAILURE = 0x80,
} proposition_state_e;

/* The days a failed proposition waits before the bar offers it again; a bad failure waits less. */
enum {
    PROPOSITION_FAILURE_COOLDOWN_DAYS = 32,
    PROPOSITION_BAD_FAILURE_COOLDOWN_DAYS = 16,
};

void proposition_advance_day(void);
s32 proposition_count_returned(s32 location);

/* lists and dispatch */
/* Row kinds of the Brave Story's proposition category list (brave_story_push_propositions_level). */
typedef enum brave_story_proposition_row {
    BRAVE_STORY_PROPOSITION_ROW_ADVANCING = 0,
    BRAVE_STORY_PROPOSITION_ROW_WORK_HISTORY = 1,
} brave_story_proposition_row_e;

/* Saved proposition-list cursor row. Declared as an array: GCC's alias heuristic needs an in-struct object to keep
 * the target's load order in brave_story_handle_advancing_list_input; a scalar does not. */
extern s32 g_brave_story_advancing_saved_cursor[];
/* Saved category row, declared as an array for the same reason. */
extern s32 g_brave_story_propositions_saved_cursor[];
extern s32 g_brave_story_participant_saved_cursor;
extern s32 g_proposition_gil_amount;
extern s32 g_proposition_send_unit_count;
extern s16 g_proposition_candidate_levels[];
extern s32 g_proposition_dispatch_days;
extern u32 g_proposition_detail_image_buffer[];
void proposition_push_detail_level(s32 value);
void proposition_build_detail_panel(menu_stack_record_t* level, s32 proposition, s32 coordinate_mode);
void proposition_push_dispatch_confirm_level(void);
void proposition_push_send_unit_level(void);
void proposition_handle_recall_input(menu_stack_record_t* level);
void brave_story_handle_participant_list_input(menu_stack_record_t* level);

/* report and rewards */
/* Outcome of a proposition (proposition_report_t.result), from the
 * participants' total score. Only SUCCESS completes the proposition and earns
 * its reward. */
typedef enum proposition_result {
    PROPOSITION_RESULT_SUCCESS = 0,     /* score of 99 or more, or the preferred-job roll */
    PROPOSITION_RESULT_FAILURE = 1,     /* score of 59 to 98: a tenth of the JP, a fifth of the gil */
    PROPOSITION_RESULT_BAD_FAILURE = 2, /* lower score, or recalled: a twentieth of both */
} proposition_result_e;

/* What a successful proposition brought back (proposition_report_t.reward_type);
 * TREASURE and UNEXPLORED_LAND are also the proposition row's discovery_kind. */
typedef enum proposition_reward {
    PROPOSITION_REWARD_NONE = 0,
    PROPOSITION_REWARD_TREASURE = 1,
    PROPOSITION_REWARD_UNEXPLORED_LAND = 2,
    PROPOSITION_REWARD_RANDOM_BONUS = 3, /* the three-tier random reward */
} proposition_reward_e;

/* Reward names in the TEXT_ID_PROPOSITION_REWARD_NAME_BASE section: the random bonuses from the base,
 * then the no-reward name, the unexplored lands and the treasures, each by reward_index. */
enum {
    TEXT_ID_REWARD_NONE = TEXT_ID_PROPOSITION_REWARD_NAME_BASE + 6,
    TEXT_ID_REWARD_LAND_FIRST = TEXT_ID_PROPOSITION_REWARD_NAME_BASE + 0x1b,
    TEXT_ID_REWARD_TREASURE_FIRST = TEXT_ID_PROPOSITION_REWARD_NAME_BASE + 0x2e,
};

/* Provisional: the outcome of a returned proposition (g_proposition_report), worked out while its report
 * runs. It is one object: proposition_find_preferred_job_unit reads excluded_party_index at -0xc from its
 * &proposition_index base, and calculate_base_jp_and_gil and step_result_level reach -0x10..+0x24 from single
 * bases. */
typedef struct proposition_report {
    s32 excluded_party_index; /* 0x00 */
    s32 speaker_party_index;  /* 0x04; chosen and read back by proposition_step_participant_message */
    s32 preferred_unit;       /* 0x08; fixed speaker passed to proposition_step_participant_message */
    s32 proposition_index;    /* 0x0c; index into g_proposition_active */
    s32 result;               /* 0x10; proposition_result_e */
    s32 reward_type;          /* 0x14; proposition_reward_e */
    s32 reward_index;         /* 0x18 */
    s32 reward_value;         /* 0x1c */
    s32 recalled;             /* 0x20; reward calculation is skipped after a recall */
    u8 _unused_24[0x10];
    s32 participant_jp[3];     /* 0x34; JP awarded to each participant */
    s32 participant_scores[3]; /* 0x40; success-score contribution */
    s32 participant_gil[3];    /* 0x4c; contribution to the total gil reward */
} proposition_report_t;
typedef char wldcore_proposition_report_size_must_be_88[sizeof(proposition_report_t) == 88 ? 1 : -1];

extern proposition_report_t g_proposition_report;
void proposition_handle_report_confirm_input(proposition_report_confirm_level_t* level);
void proposition_determine_success(void);
s32 proposition_find_preferred_job_unit(void);
s32 proposition_step_participant_message(proposition_report_level_t* state, s32 message, s32 speaker);
s32 proposition_adjust_message(s32 base, s32 party_index);
void proposition_calculate_jp_and_gil(void);
void proposition_apply_extra_days(void);
void proposition_roll_bonus_reward(void);
s32 proposition_get_milestone_rank(void);

#endif
