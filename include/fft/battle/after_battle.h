#ifndef AFTER_BATTLE_H
#define AFTER_BATTLE_H

/* REQUIRE's party changes after a battle (Brave and Faith departures, dismissals, equipment over the limit) and the
 * after-battle screens' menus, text sections and threads. The battle conditions banners are in conditions.h and the
 * war trophies in war_trophy.h. */

#include "psx/types.h"

/* party changes */
/* What after_battle_set_biography_state stores in a character's bio variables. */
typedef enum party_bio_state {
    PARTY_BIO_STATE_DISMISSED = 12, /* dismissed or turned away */
    PARTY_BIO_STATE_LEFT_LOW_BRAVE = 13,
    PARTY_BIO_STATE_LEFT_HIGH_FAITH = 14,
    PARTY_BIO_STATE_LOST_IN_BATTLE = 15
} party_bio_state_e;

extern s32 g_party_affected_unit_count;
extern s32 g_party_affected_battle_unit_indices[];

s32 after_battle_update_party(void);
void after_battle_apply_unit_changes(void);
void after_battle_brave_faith_report_thread(void);
void after_battle_departures_thread(void);
void after_battle_set_biography_state(s32 character_identity, s32 bio_state);
s32 after_battle_store_or_sell_equipment(s32 unit_id);
s32 after_battle_sell_equipment_over_limit(s32 unit_id);

/* menus */
extern s16 g_after_battle_menu_selection_result;

void after_battle_wait_menu_selection(void);

/* text */
/* Character identities grouped by their warning, departure and dismissal text. */
enum { PARTY_MESSAGE_IDENTITY_GROUP_COUNT = 73, PARTY_MESSAGE_IDENTITIES_PER_GROUP = 3 };

/* REQUIRE's text-file directory, with offsets relative to g_text_data. The first
 * three named sections retain these indices at runtime; job names bind to
 * TEXT_SECTION_JOB_NAMES instead. */
typedef enum after_battle_text_section {
    AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_WARNINGS = 1,   /* a unit's Brave is low or its Faith high */
    AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES = 2, /* the unit leaves the party for it */
    AFTER_BATTLE_TEXT_SECTION_DISMISS_UNIT_REPLIES = 3,   /* what a unit turned away or dismissed says */
    AFTER_BATTLE_TEXT_SECTION_JOB_NAMES = 4
} after_battle_text_section_e;

extern u8 g_party_message_identity_groups[PARTY_MESSAGE_IDENTITY_GROUP_COUNT][PARTY_MESSAGE_IDENTITIES_PER_GROUP];

/* threads */
/* Slots of the after-battle screens' own threads; thread_screen_slot_e
 * has the shared ones. */
typedef enum after_battle_thread_slot {
    AFTER_BATTLE_THREAD_SLOT_MENU = 5,       /* selection menus, prompts and the special-case display */
    AFTER_BATTLE_THREAD_SLOT_MESSAGE = 7,    /* the icon strip, message text or menu shown with them */
    AFTER_BATTLE_THREAD_SLOT_TRANSITION = 10 /* unit_panel_backdrop_thread */
} after_battle_thread_slot_e;

extern s32 g_after_battle_thread_suspended_id;

#endif
