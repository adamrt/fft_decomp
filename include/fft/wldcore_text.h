#ifndef WLDCORE_TEXT_H
#define WLDCORE_TEXT_H

/* WLDCORE's text: the blocks of the message file, the menu text ids the code names, and the paged text window. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* message file */
/* Blocks of the message file WLDMES.BIN (message_block_load_if_changed). */
typedef enum message_block {
    MESSAGE_BLOCK_RUMORS = 0,
    MESSAGE_BLOCK_PROPOSITIONS = 1,
    MESSAGE_BLOCK_UNEXPLORED_LANDS = 2,
    MESSAGE_BLOCK_TREASURES = 3,
    MESSAGE_BLOCK_STORY_EVENTS = 4,
    MESSAGE_BLOCK_TUTORIAL_TOPICS = 5,
    MESSAGE_BLOCK_BRAVE_STORY_PROPOSITIONS = 10,
    MESSAGE_BLOCK_FACTS = 11,
    MESSAGE_BLOCK_PROPOSITION_REPORT_FIRST = 16, /* one block per proposition id */
} message_block_e;

extern s32 g_message_block_sector_offsets[]; /* by message_block_e */
extern u8 g_message_block_buffer[];
extern s32 g_message_block_loaded; /* the block in g_message_block_buffer */
void message_block_load_if_changed(s32 block);

/* menu text */
/* The text ids of TEXT_ID_WORLD_MENU_BASE that the code names: the bar, the
 * Brave Story, the tutorial and the location menu. The first five entries are
 * the bar's rows, TEXT_ID_WORLD_MENU_BASE + bar_row_e. */
typedef enum world_map_text_id {
    TEXT_ID_OK_NO = 0xb805,                  /* "O K / N O" */
    TEXT_ID_BAR_WELCOME = 0xb806,            /* the bar owner: "Welcome. Come in and relax." */
    TEXT_ID_BAR_GOODBYE = 0xb807,            /* "Thank you! Come again." */
    TEXT_ID_RUMORS_INTRO = 0xb808,           /* "Rumors are floating around." */
    TEXT_ID_RUMOR_ABOUT = 0xb809,            /* "This rumor's about ..." */
    TEXT_ID_NO_RUMORS = 0xb80a,              /* "Haven't heard any rumors yet." */
    TEXT_ID_PROPOSITIONS_INTRO = 0xb80b,     /* "I know a great way to make a buck." */
    TEXT_ID_PROPOSITION_ABOUT = 0xb80c,      /* "This's what I know about ..." */
    TEXT_ID_NO_PROPOSITIONS = 0xb80d,        /* "Haven't heard any offers lately." */
    TEXT_ID_NO_MORE_JOBS = 0xb80e,           /* "I can't give anymore jobs." */
    TEXT_ID_ACCEPT_JOB_PROMPT = 0xb80f,      /* "You want this job?" */
    TEXT_ID_JOB_FEE_PROMPT = 0xb810,         /* "You need ... gil for info etc. Still wanna do it?" */
    TEXT_ID_NOT_ENOUGH_GIL = 0xb811,         /* "You don't got enough gil." */
    TEXT_ID_CHOOSE_UNITS = 0xb812,           /* "So, who you wanna send?" */
    TEXT_ID_CHOOSE_DAYS = 0xb813,            /* "This job's from ... to .... How many days you want?" */
    TEXT_ID_CONFIRM_CONDITIONS = 0xb814,     /* "These are the conditions, OK?" */
    TEXT_ID_JOBS_RETURNED = 0xb819,          /* "Good job. You came back from these jobs." */
    TEXT_ID_HEAR_ABOUT_JOB = 0xb81a,         /* "Wanna hear about ...?" */
    TEXT_ID_JOBS_IN_PROGRESS = 0xb81b,       /* "These jobs're in progress." */
    TEXT_ID_RETURN_TOMORROW = 0xb81c,        /* "I'll have them return tomorrow." */
    TEXT_ID_CALL_BACK_PROMPT = 0xb81d,       /* "What a waste if they quit now. Still wanna call them back?" */
    TEXT_ID_CALL_BACK_ACCEPTED = 0xb81e,     /* "Ok, I'll have them return by tomorrow." */
    TEXT_ID_JOB_CONDITION_LABELS = 0xb821,   /* "Days", "Place", "Unit" with the first two filled in */
    TEXT_ID_CHOOSE_MORE_UNITS = 0xb822,      /* "Who else are you sending?" */
    TEXT_ID_DAYS_LABEL = 0xb823,             /* "Days:" */
    TEXT_ID_DEBUG_VARIABLE_VALUE = 0xb824,   /* the debug list: a script variable and its value */
    TEXT_ID_DEBUG_VARIABLE = 0xb825,         /* the same without the value, for values of 10000 up */
    TEXT_ID_PARTY_NUMBER_LABEL = 0xb826,     /* "Party number:" */
    TEXT_ID_JOB_RESULT_LABELS = 0xb830,      /* "Bonus", "Total", "Job Point" */
    TEXT_ID_FOUR_DIGIT_VALUE = 0xb831,       /* a value, four digits wide */
    TEXT_ID_QUIT_CONTINUE = 0xb83a,          /* "Quit / Continue" */
    TEXT_ID_BRAVE_STORY_ROW_FIRST = 0xb83f,  /* plus brave_story_row_e: "Fact" to "Return" */
    TEXT_ID_INJURED_CASUALTIES = 0xb846,     /* "Injured", "Casualties" and their counts */
    TEXT_ID_BRAVE_STORY_INTRO = 0xb848,      /* Alazlam: "The secrets of the Lion War are uncovered in this book." */
    TEXT_ID_BRAVE_STORY_ENTRY_INFO = 0xb849, /* "This is the information on ..." */
    TEXT_ID_BRAVE_STORY_GOODBYE = 0xb84a,    /* "You must find truth on your own. ..." */
    TEXT_ID_SOUND_NOVEL_RESUME_CHOICES = 0xb84b, /* "This book was canceled earlier. Restart / Read from start" */
    TEXT_ID_SUBSTITUTED_TEXT = 0xb84c,           /* the text of substitution value 0 alone */
    TEXT_ID_JOB_CONTENTS_LABEL = 0xb84e,         /* "Job contents:" */
    TEXT_ID_MONTH_NAME_BASE = 0xb84f,            /* plus the month, 1 to 12: "Jan" to "Dec" */
    TEXT_ID_TOLD_BEFORE = 0xb85c,                /* "Think I told you before but..." */
    TEXT_ID_BAR = 0xb85d,                        /* the four entries of a town's location menu */
    TEXT_ID_SHOP = 0xb85e,
    TEXT_ID_SOLDIER_OFFICE = 0xb85f,
    TEXT_ID_FUR_SHOP = 0xb860,
    TEXT_ID_PROPOSITION_CATEGORY_ROW_FIRST
    = 0xb864,                                     /* plus brave_story_proposition_row_e: "Advancing", "Work history" */
    TEXT_ID_VALUE = 0xb866,                       /* a value */
    TEXT_ID_TWO_DIGIT_VALUE = 0xb867,             /* a value, two digits wide */
    TEXT_ID_RAMZA_INFO = 0xb868,                  /* BRAVE_STORY_ENTRY_INFO for person 0: a title and Ramza's name */
    TEXT_ID_RAMZA_TITLE_FIRST = 0xb869,           /* plus his profile: sixteen titles, "Knight" each */
    TEXT_ID_PROPOSITION_HINT_FIRST = 0xb879,      /* plus the proposition: the bar owner's advice on it */
    TEXT_ID_TUTORIAL_CATEGORY_ROW_FIRST = 0xb8d9, /* plus tutorial_menu_row_e: "Organization" to "Return" */
    TEXT_ID_TUTORIAL_WELCOME = 0xb8dd,            /* Daravon: "If you know, you can foresee. ..." */
    TEXT_ID_TUTORIAL_PROMPT = 0xb8de,             /* "Know the path to win the battle. Solve your questions here." */
    TEXT_ID_CHOOSE_HUMAN_UNIT = 0xb8e0,           /* "Choose a good human to send, or one with no strings!" */
    TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST = 0xb8ed,    /* plus the floor: "Nogias" to "End" */
    TEXT_ID_TREASURE_FOUND_FIRST = 0xb8f9, /* the eleven messages for a treasure found, to "Treasure Hunter Master" */
    TEXT_ID_LAND_FOUND_FIRST = 0xb904,     /* the same for an unexplored land, to "Master Adventurer" */
} world_map_text_id_e;

void window_measure_text_image(s32 text_id, size32_t* output);

/* scrollable text window */
/* text_window_t.scroll_state. */
typedef enum text_window_scroll {
    TEXT_WINDOW_SCROLL_PAGING = 0,
    TEXT_WINDOW_SCROLL_MANUAL = 1, /* while square is held: up and down scroll by rows */
    TEXT_WINDOW_SCROLL_RETURN = 2, /* scrolls back to the page start */
} text_window_scroll_e;

/* A paged text window and its four decorative window records. */
typedef struct text_window {
    s32 image_x; /* 0x00; text image position.x for window_build_render_record_image; cleared by the setups */
    s32 image_y; /* 0x04; text image position.y; 0x70 or 0x60 from the setups */
    point32_t base;
    s32 text_width;
    s32 rows_per_page;
    s32 measured_columns;
    s32 measured_rows;
    /* 0x20..0x2e: copy of the optional extra render record's base and bounds, taken as two block copies;
     * text_window_render_page scrolls base.y by row * 16 into the record's base and clips the
     * record's y and height from the bounds. */
    point32_t extra_base; /* 0x20 */
    RECT extra_bounds;    /* 0x28 */
    s32 scroll_state;     /* 0x30; text_window_scroll_e */
    s32 text_id;
    s32 left_window_index;
    s32 middle_window_index;
    s32 right_window_index;
    s32 render_record_index;
    s32 lower_window_index;
    s32 priority; /* 0x4c; OT priority of the text window's records (window_init_animated_record) */
    s32 last_page_row;
    s32 page_start_row;
    s32 scroll_row;            /* 0x58; row shown while scrolling (MANUAL and RETURN) */
    s32 image_coordinate_mode; /* 0x5c; coordinate mode of the text render record image */
    s32 scroll_timer;          /* 0x60; frame counter of the auto-scroll state */
    s32 extra_render_index;    /* 0x64; optional render record scrolled with the text, -1 if none */
    s32 text_substitutions[2]; /* 0x68; copied to the text substitution values unless -1 */
} text_window_t;
typedef char wldcore_text_scrollable_window_size_must_be_112[sizeof(text_window_t) == 112 ? 1 : -1];

extern text_window_t g_text_window;
void text_window_open(text_window_t* state);
s32 text_window_step(text_window_t* window);

#endif
