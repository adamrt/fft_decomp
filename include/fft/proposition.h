#ifndef PROPOSITION_H
#define PROPOSITION_H

/* Propositions (the Bar's errands): the active records and their daily progress, the proposition data file, the
 * available, active and returned lists, accepting and sending units, the report and its rewards, and the discovery
 * announcement. */

#include "fft/menu_stack.h"
#include "psx/types.h"

/* active propositions */
/* Values of proposition_progress_t.flags of an active proposition. */
typedef enum proposition_progress_flag {
    PROPOSITION_PROGRESS_DISPATCHED = 0x01,
    PROPOSITION_PROGRESS_RECALLED = 0x02, /* canceled: returns on the next day */
    PROPOSITION_PROGRESS_RETURNED = 0x04, /* the days are up: waiting for the report */
} proposition_progress_flag_e;

/* One active proposition (g_proposition_active).
 * proposition_advance_day advances elapsed_days and sets the
 * returned flag; proposition_handle_dispatch_confirm_input stores the
 * current world-map location and the participants, and
 * proposition_count_returned filters returned records by location. */
typedef struct proposition_progress {
    u8 flags;          /* 0x00; proposition_progress_flag_e */
    u8 proposition_id; /* 0x01; text 0xd000 | id names it */
    u8 elapsed_days;
    u8 assigned_days;
    u8 location;
    u8 participant_count;      /* 0x05; proposition_step_participant_message trusts this count */
    u8 participant_indices[3]; /* 0x06; party indices, each checked by a party lookup before it is stored */
} proposition_progress_t;
typedef char proposition_progress_size_must_be_9[sizeof(proposition_progress_t) == 9 ? 1 : -1];

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

extern proposition_progress_t g_proposition_active[8];
extern u8 g_proposition_count;      /* used entries of g_proposition_active */
extern u8 g_proposition_states[96]; /* proposition_state_e */

/* A RAM bitmap indexed by 32-bit values; the save serializer copies only its packed data bytes and omits the padding
 * at the end of its allocation. */
extern u32 g_proposition_last_attempt_date_bits[27]; /* written only for a successful result */

void proposition_advance_day(void);
s32 proposition_count_returned(s32 location);

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
    u16 condition_flags; /* 0x22; PROPOSITION_CONDITION_* */
    u16 _unused_24;      /* 0x24 */
    u16 speaker_mode;    /* 0x26; proposition_speaker_e */
    u16 preferred_job;   /* 0x28 */
    u16 required_shop_availability; /* 0x2a; minimum EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY */
    u16 condition_value;            /* 0x2c; zodiac sign or proposition status index */
} proposition_fields_t;
typedef char proposition_fields_size_must_be_46[sizeof(proposition_fields_t) == 46 ? 1 : -1];

extern s32* g_proposition_data; /* the loaded proposition data file */
extern proposition_fields_t g_proposition_selected_row;

void proposition_unpack_row(proposition_fields_t* out, s32 index);
s32 proposition_is_available_at(s32 index, s32 location);

/* File records have different layouts; the selected record determines the table type. */
void* proposition_get_data(proposition_data_record_e record);

/* lists */
void proposition_open_available_list(void);
void proposition_handle_available_list_input(menu_stack_record_t* level);
void proposition_restore_available_list_prompt(menu_stack_record_t* level);
void proposition_block_available_list_buttons(menu_stack_record_t* unused_level);
void proposition_open_active_list(void);
void proposition_handle_active_list_input(menu_stack_record_t* level);
void proposition_restore_active_list_prompt(menu_stack_record_t* level);
void proposition_block_active_list_buttons(menu_stack_record_t* unused_level);
void proposition_open_returned_list(void);
void proposition_handle_returned_list_input(menu_stack_record_t* level);
void proposition_restore_returned_list_prompt(menu_stack_record_t* level);
void proposition_block_returned_list_buttons(menu_stack_record_t* unused_level);

/* detail and recall */
/* The proposition detail panel's outputs, at the same offsets in the PROPOSITION_RECALL and
 * BRAVE_STORY_PARTICIPANTS levels (proposition_build_detail_panel). */
typedef struct proposition_detail_panel {
    u8 _unused_00[8]; /* the owning level's own fields */
    s32 render_index; /* 0x08 */
    s32 panel_rows;   /* 0x0c */
} proposition_detail_panel_t;

/* The PROPOSITION_RECALL level, which recalls a proposition. The detail-panel builder writes
 * detail_render and detail_rows; the yes/no cursor uses choice_render. */
typedef struct proposition_recall_level {
    s32 cursor_window; /* 0x00 */
    s32 choice_render; /* 0x04 */
    s32 detail_render; /* 0x08 */
    s32 detail_rows;   /* 0x0c */
    s32 proposition;   /* 0x10 */
    s32 choice;        /* 0x14 */
    s32 confirmed;     /* 0x18 */
    u8 _unused_1c[4];
    s32 frame_window; /* 0x20 */
} proposition_recall_level_t;

extern u32 g_proposition_detail_image_buffer[];

void proposition_push_detail_level(s32 value);
void proposition_build_detail_panel(menu_stack_record_t* level, s32 proposition, s32 coordinate_mode);
void proposition_suspend_detail_level(menu_stack_record_t* level);
void proposition_resume_detail_level(menu_stack_record_t* level);
void proposition_handle_recall_input(menu_stack_record_t* level);

/* accepting and sending units */
/* proposition_confirm_level_t.mode as proposition_handle_accept_confirm_input steps it. */
typedef enum proposition_accept_mode {
    PROPOSITION_ACCEPT_MODE_CHOOSE = 0,
    PROPOSITION_ACCEPT_MODE_NO_UNIT_MESSAGE = 1, /* nobody can go: a press shows the second message */
    PROPOSITION_ACCEPT_MODE_CLOSE_MESSAGE = 3,   /* a refusal message is up: a press closes the panel */
} proposition_accept_mode_e;

/* The yes/no panel built by window_build_yes_no_panel and stepped by
 * proposition_handle_accept_confirm_input and proposition_handle_dispatch_confirm_input. */
typedef struct proposition_confirm_level {
    s32 cursor_window; /* 0x00 */
    s32 render_index;  /* 0x04 */
    s32 row;           /* 0x08; 0 = yes, 1 = no */
    s32 mode;          /* 0x0c; proposition_accept_mode_e */
    s32 header_window; /* 0x10 */
} proposition_confirm_level_t;

/* Values of proposition_send_unit_level_t.mode. */
typedef enum proposition_send_unit_mode {
    PROPOSITION_SEND_UNIT_MODE_SELECT = 0,
    PROPOSITION_SEND_UNIT_MODE_CANCEL = 1,       /* cross: closes back to the available propositions */
    PROPOSITION_SEND_UNIT_MODE_REPLACE = 2,      /* three are chosen: picks the slot the unit replaces */
    PROPOSITION_SEND_UNIT_MODE_STATUS_OPEN = 3,  /* square: waits for the list threads, then opens the status panels */
    PROPOSITION_SEND_UNIT_MODE_STATUS = 4,       /* unit status, browsed with L1 and R1 */
    PROPOSITION_SEND_UNIT_MODE_STATUS_CLOSE = 5, /* waits for the status threads, then reopens the list */
    PROPOSITION_SEND_UNIT_MODE_ACCEPT = 6,       /* start: closes and moves on to the day count */
} proposition_send_unit_mode_e;

/* Provisional: the PROPOSITION_SEND_UNIT level, where up to three units are chosen for a proposition.
 * proposition_step_send_unit_slots selects the slot and owns the portrait render records. */
typedef struct proposition_send_unit_level {
    window_render_pair_t list_frame; /* 0x00 */
    s32 frame_window_a;              /* 0x08 */
    s32 frame_window_b;              /* 0x0c */
    s32 preview_render;              /* 0x10 */
    s32 list_render;                 /* 0x14 */
    s32 cursor_window;               /* 0x18 */
    u8 _unused_1c[4];
    s32 mode;          /* 0x20; proposition_send_unit_mode_e */
    s32 shown_preview; /* 0x24 */
    s32 slot;          /* 0x28 */
    u8 _unused_2c[4];
    s32 unit_count;      /* 0x30; wrap bound, written by proposition_load_send_unit_candidates */
    s32 portrait_render; /* 0x34 */
    s32 portrait_frame;  /* 0x38 */
    s32 portrait_window; /* 0x3c */
    s32 left_arrow;      /* 0x40 */
    s32 right_arrow;     /* 0x44 */
    u8 _unused_48[0x10];
    s32 shown_unit; /* 0x58 */
} proposition_send_unit_level_t;
typedef char proposition_send_unit_level_size_must_be_92[sizeof(proposition_send_unit_level_t) == 92 ? 1 : -1];

extern s32 g_proposition_gil_amount;
extern s32 g_proposition_send_unit_count;
extern s16 g_proposition_candidate_levels[];
extern s32 g_proposition_dispatch_days;

void proposition_push_send_unit_level(void);
void proposition_handle_accept_input(menu_stack_record_t* level);
void proposition_handle_accept_confirm_input(menu_stack_record_t* level);
void proposition_step_send_unit_level(menu_stack_record_t* level);
void proposition_suspend_send_unit_level(menu_stack_record_t* level);
void proposition_resume_send_unit_level(menu_stack_record_t* level);
void proposition_push_days_level(void);
void proposition_step_days_level(menu_stack_record_t* level);
void proposition_push_dispatch_confirm_level(void);
void proposition_handle_dispatch_confirm_input(menu_stack_record_t* level);

/* report */
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

/* Provisional: the PROPOSITION_REPORT_CONFIRM level, which asks whether to hear a returned proposition's report.
 * proposition_handle_returned_list_input pushes it over the yes/no panel. */
typedef struct proposition_report_confirm_level {
    s32 cursor_window; /* 0x00; window record moved between the two rows */
    s32 render_index;  /* 0x04; render record holding the confirmation text */
    s32 selection;     /* 0x08; 0 = upper row, 1 = lower row */
    s32 _unused_0c;
    s32 argument; /* 0x10; argument of the level proposition_push_report_level pushes */
} proposition_report_confirm_level_t;

/* Values of proposition_report_level_t.load_phase. */
typedef enum proposition_report_load {
    PROPOSITION_REPORT_LOAD_DONE = 0,
    PROPOSITION_REPORT_LOAD_MESSAGES = 1, /* waits for the audio queue, then loads the message block */
    PROPOSITION_REPORT_LOAD_PICTURE = 2,  /* waits for it, then loads the reward's picture set */
} proposition_report_load_e;

/* Provisional: the PROPOSITION_REPORT level pushed by proposition_push_report_level, which starts the
 * message thread (WORLD_MAP_THREAD_SLOT_MESSAGE), and stepped by proposition_step_report_level. */
typedef struct proposition_report_level {
    s32 state; /* 0x00; step index, -1 .. 0x12; pushed as -1, or 0x63 when the proposition was recalled */
    s32 phase; /* 0x04; proposition_step_message phase; cleared at push */
    u8 _unused_08[4];
    s32 load_phase;   /* 0x0c; proposition_report_load_e; MESSAGES at push */
    s32 render_index; /* 0x10; picture render record; -1 at push */
    u8 _unused_14[0x18];
    s32 proposition; /* 0x2c; argument of the push */
} proposition_report_level_t;

/* Values of proposition_result_level_t.phase. */
typedef enum proposition_result_phase {
    PROPOSITION_RESULT_PHASE_INPUT = 0,
    PROPOSITION_RESULT_PHASE_CLOSING = 1, /* waits for the audio queue and the message thread */
    PROPOSITION_RESULT_PHASE_DONE = 2,    /* waits for the audio queue, then pops */
} proposition_result_phase_e;

/* Provisional: the PROPOSITION_RESULT level pushed by proposition_push_result_level and stepped by
 * proposition_step_result_level. */
typedef struct proposition_result_level {
    s32 render_index; /* 0x00 */
    u8 _unused_04[0xc];
    s32 phase; /* 0x10; proposition_result_phase_e */
    u8 _unused_14[0x14];
    s32 delay;       /* 0x28; frames before the success jingle restarts */
    s32 proposition; /* 0x2c; index into g_proposition_active */
    u8 _unused_30[0x2c];
} proposition_result_level_t;

/* Provisional: the outcome of a returned proposition (g_proposition_report), worked out while its report
 * runs. It is one object: proposition_find_preferred_job_unit reads reporter_party_index at -0xc from its
 * &proposition_index base, and proposition_calculate_jp_and_gil and proposition_step_result_level reach -0x10..+0x24
 * from single bases. */
typedef struct proposition_report {
    s32 reporter_party_index; /* 0x00 */
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
typedef char proposition_report_size_must_be_88[sizeof(proposition_report_t) == 88 ? 1 : -1];

extern proposition_report_t g_proposition_report;

void proposition_handle_report_confirm_input(menu_stack_record_t* level);
void proposition_step_report_level(menu_stack_record_t* level);
void proposition_step_result_level(menu_stack_record_t* level);
void proposition_determine_success(void);
s32 proposition_find_preferred_job_unit(void);
s32 proposition_step_participant_message(proposition_report_level_t* state, s32 message, s32 speaker);
s32 proposition_adjust_message(s32 base, s32 party_index);
void proposition_calculate_jp_and_gil(void);
void proposition_apply_extra_days(void);
void proposition_roll_bonus_reward(void);

/* discovery */
/* Values of proposition_discovery_level_t.phase. */
typedef enum proposition_discovery_phase {
    PROPOSITION_DISCOVERY_PHASE_MESSAGES = 0,     /* circle shows the next message */
    PROPOSITION_DISCOVERY_PHASE_LAST_MESSAGE = 1, /* waits for the message box, then starts MUSIC_MILESTONE */
    PROPOSITION_DISCOVERY_PHASE_JINGLE = 2,       /* 180 frames, then back to MUSIC_BAR */
    PROPOSITION_DISCOVERY_PHASE_CLOSE = 3,        /* waits for the audio queue, then pops */
} proposition_discovery_phase_e;

/* The PROPOSITION_DISCOVERY level pushed by proposition_push_discovery_level after a report that found treasure or
 * unexplored land: a run of "found" messages ending in the milestone jingle. proposition_show_next_milestone_message
 * shows each message and sets LAST_MESSAGE when the count runs out; proposition_step_discovery_level then advances
 * the later phases. */
typedef struct proposition_discovery_level {
    s32 current;
    s32 remaining;
    s32 phase; /* 0x08; proposition_discovery_phase_e */
    s32 timer; /* 0x0c; frames spent in phase 2 */
} proposition_discovery_level_t;

/* The collection counts that earn a milestone message (proposition_get_milestone_rank): eight each,
 * 1/4/8/12/16/20/24/31 treasures and 1/3/6/8/10/12/14/16 unexplored lands. */
extern u8 g_proposition_treasure_milestones[];
extern u8 g_proposition_unexplored_land_milestones[];

s32 proposition_get_milestone_rank(void);
void proposition_push_discovery_level(void);
void proposition_step_discovery_level(menu_stack_record_t* level);

#endif
