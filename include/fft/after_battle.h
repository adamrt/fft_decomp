#ifndef AFTER_BATTLE_H
#define AFTER_BATTLE_H

/* REQUIRE's after-battle screens: the battle conditions banners and their bursts (BONUS.BIN), the war trophies, and the
 * screens' menus, graphics, text sections and threads. */

#include "psx/libgpu.h"
#include "psx/types.h"

/* conditions */
/* The BONUS.BIN banner selected by conditions_special_thread's first parameter.
 * These labels are separate from the event instruction's display-condition operations. */
typedef enum conditions_banner {
    CONDITIONS_BANNER_READY = 0,
    CONDITIONS_BANNER_CONGRATULATIONS = 1,
    CONDITIONS_BANNER_BONUS_MONEY = 2,
    CONDITIONS_BANNER_GIL_AMOUNT = 3,
    CONDITIONS_BANNER_WAR_TROPHIES = 4,
    CONDITIONS_BANNER_WARNING = 5,
    CONDITIONS_BANNER_PARTING_SHOT = 6,
    CONDITIONS_BANNER_BATTLE_COMPLETE = 7,
    CONDITIONS_BANNER_HE_LEFT_COMPANY = 8,
    CONDITIONS_BANNER_SHE_LEFT_COMPANY = 9,
    CONDITIONS_BANNER_JOIN_UP = 10
} conditions_banner_e;

void conditions_show_ready_bugged(void);
void conditions_special_thread(void);
void victory_congratulations_thread(void);

/* REQUIRE's condition/reward dispatcher, called by the event library in BATTLE and WORLD. */
void require_conditions_entrypoint(void);

/* banner bursts */
extern RECT g_conditions_banner_palette_rect;

void conditions_load_banner_image(s32 image_id);

/* REQUIRE's banner-burst thread, entered by DarkScreen after the overlay loads. */
void require_dark_screen_entrypoint(void);

/* menus and graphics */
extern s16 g_after_battle_menu_selection_result;
extern u8 g_after_battle_gfx_fade_rgb[3];

void gfx_fade_rgb_31_frames(s32 target_0, s32 target_1, s32 target_2);

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
    AFTER_BATTLE_THREAD_SLOT_TRANSITION = 10 /* unit_panel_transition_thread */
} after_battle_thread_slot_e;

extern s32 g_after_battle_thread_suspended_id;
extern u8 g_after_battle_work[]; /* no named access */

#endif
