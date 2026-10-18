#ifndef PARTY_H
#define PARTY_H

/* The party roster: MAIN's routines that build, save, level and remove its units and monster eggs, and REQUIRE's
 * after-battle party changes (departures, dismissals, equipment returns and bio variables). */

#include "fft/menu.h"
#include "fft/unit.h"
#include "fft/unit_entd.h"
#include "fft/unit_party.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* roster */
extern party_unit_t g_party_units[PARTY_ROSTER_SLOT_COUNT];

void party_clear(void);
void party_init_new_game(s32 mode);
party_unit_t* party_get_unit(s32 party_index);
s32 party_find_free_slot(s32 save_formation, u8* palette);
s32 party_save_unit(unit_stats_t* unit, s32 allow_guest);
s32 party_save_encounter_unit(entd_encounter_t* entd, s32 unit_id, s32 allow_guest, s32 use_world_text);
void party_remove_unit(u32 index);
s32 party_create_monster_egg(s32 monster_job, s32 egg_modifier, s32 egg_color);
u32 party_level_unit_to_target(party_unit_t* party_unit, s32 level_delta);
u8 party_calculate_highest_level(void);
party_unit_t* party_get_roster_unit(s32 party_index); /* WLDCORE's wrapper for party_get_unit */
void party_join_event_units(void); /* event library: saves the units marked to join, without the formation screen */

/* Out-of-battle generation normalizes every value except male, female and
 * monster to Ramza. The raw-stat generator indexes the tables directly. */
s32 party_generate_unit(unit_generation_type_e unit_type);

/* after-battle changes */
/* What party_set_class_variable stores in a character's bio variables. */
typedef enum party_bio_state {
    PARTY_BIO_STATE_DISMISSED = 12, /* dismissed or turned away */
    PARTY_BIO_STATE_LEFT_LOW_BRAVE = 13,
    PARTY_BIO_STATE_LEFT_HIGH_FAITH = 14,
    PARTY_BIO_STATE_LOST_IN_BATTLE = 15
} party_bio_state_e;

extern s32 g_party_affected_unit_count;
extern s32 g_party_affected_battle_unit_indices[];

s32 party_update_after_battle(void);
void party_brave_faith_report_thread(void);
void party_departures_thread(void);
void party_set_class_variable(s32 unit_class, s32 value);
s32 party_store_or_sell_equipment(s32 unit_id);
s32 party_sell_equipment_over_limit(s32 unit_id);

#endif
