#ifndef MAIN_SAVE_H
#define MAIN_SAVE_H

/* Resident game progress: propositions, sound-novel records, dated discoveries and saved bits. */

#include "fft/menu.h"
#include "fft/menu_selection.h"
#include "psx/types.h"

/* propositions */
/* One active proposition (g_main_active_propositions).
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
typedef char wldcore_proposition_progress_size_must_be_9[sizeof(proposition_progress_t) == 9 ? 1 : -1];

extern proposition_progress_t g_main_active_propositions[8];
extern u8 g_main_save_proposition_count; /* used entries of g_main_active_propositions */
extern u8 g_main_proposition_states[96]; /* proposition_state_e */

/* sound-novel records */
struct sound_novel_script_file;

/* A saved sound-novel reading state (g_main_sound_novel_saved_records, 0x38 bytes each).
 * sound_novel_reset_selected_reading establishes the stride and
 * sound_novel_reset_reading resets fields selectively; meanings beyond
 * those stores remain provisional. */
typedef struct sound_novel_reading {
    /* 0x00: loaded sound-novel script, including its section-offset table. */
    struct sound_novel_script_file* buffer;
    u32* script_words; /* 0x04 */
    u32 instruction;   /* 0x08 */
    /* 0x0c-0x37 mirror the live sound-novel state at g_sound_novel_state_flags. */
    u16 state_flags;          /* 0x0c; sound_novel_state_flag_e */
    s16 section;              /* 0x0e */
    s16 word_index;           /* 0x10; index of the 32-bit instruction in the section; cleared by
                               * sound_novel_restore_reading
                               */
    s16 counter_delta;        /* 0x12 */
    s16 text_id;              /* 0x14 */
    s16 brightness;           /* 0x16 */
    s16 _unknown_18;          /* 0x18 */
    s16 background_set;       /* 0x1a */
    s16 picture_render_index; /* 0x1c */
    s16 picture_id;           /* 0x1e */
    s16 text_history_0;       /* 0x20; four-entry text history, newest last */
    s16 text_history_1;       /* 0x22 */
    s16 text_history_2;       /* 0x24 */
    s16 text_history_3;       /* 0x26 */
    s16 value_display_mode;   /* 0x28 */
    s16 music_selection;      /* 0x2a */
    s16 sound_mode;           /* 0x2c; 0 plays pending sounds only, 1 also switches the music track */
    s16 picture_x;            /* 0x2e */
    s16 picture_y;            /* 0x30 */
    s16 sound_id;             /* 0x32 */
    s32 counter;              /* 0x34 */
} sound_novel_reading_t;
typedef char wldcore_saved_record_size_must_be_56[sizeof(sound_novel_reading_t) == 56 ? 1 : -1];

extern sound_novel_reading_t g_main_sound_novel_saved_records[];

/* dates and saved bits */
/* RAM bitmaps indexed by 32-bit values; the save serializer copies only their packed data
 * bytes and omits the padding at the end of each allocation. */
extern u32 g_main_proposition_last_attempt_date_bits[27]; /* written only for a successful result */
extern u32 g_main_treasure_acquisition_date_bits[14];
extern u32 g_main_land_discovery_date_bits[5];

extern u32 g_main_sound_novel_read_section_bits[40];
extern u32 g_main_sound_novel_saved_flags[2];
extern u8* g_main_brave_story_ages_source;
extern u8 g_main_brave_story_character_ages[0x40];
/* Saved and restored at save-image byte 0x1c88; nothing else uses it. */
extern u8 g_main_save_unread_byte; /* saved and restored with save image offset 0x1c88; never read */

/* world-map state kept across events */
/* A story event replayed from the Brave Story list: the event, the list cursor
 * to restore and the weather variable to put back afterwards. */
extern s32 g_main_replay_story_event_index;
extern menu_cursor_state_t g_main_saved_list_cursor_state;
extern s32 g_main_saved_weather_variable;
extern s32 g_main_saved_vsync_interval;
/* Two 32-bit masks, one bit per tutorial-list entry, set when the entry is chosen. */
extern u32 g_main_tutorial_progress_masks[];

/* initialization */
void main_save_init_data_tables(void);
void main_save_init_state_and_options(void);

#endif
