#ifndef PARTY_H
#define PARTY_H

/* The party roster: resident routines that build, save, level and remove its units and monster eggs. */

#include "fft/game/battle_unit.h"
#include "fft/game/party_unit.h"
#include "psx/types.h"

struct entd_encounter;

/* roster */
/* Initial roster used by OPEN before the name/birthday screens or Continue. */
typedef enum party_new_game_mode {
    PARTY_NEW_GAME_RAMZA = 0,
    PARTY_NEW_GAME_EMPTY = 1,    /* Continue replaces this roster with the loaded save. */
    PARTY_NEW_GAME_ENCOUNTER = 2 /* Load ENTD 254; no OPEN caller selects this mode. */
} party_new_game_mode_e;

extern party_unit_t g_party_units[PARTY_ROSTER_SLOT_COUNT];

void party_clear(void);
void party_init_new_game(s32 mode);
party_unit_t* party_unit_get(s32 party_index);
s32 party_find_free_slot(s32 save_formation, u8* palette);
s32 party_unit_save(const battle_unit_t* unit, s32 allow_guest);
s32 party_save_encounter_unit(const struct entd_encounter* entd, s32 unit_id, s32 allow_guest, s32 use_world_text);
void party_unit_remove(u32 index);
s32 party_find_slot_by_sprite_set(s32 sprite_set);
s32 party_create_monster_egg(s32 monster_job, s32 egg_modifier, s32 egg_color);
u32 party_unit_gain_levels(party_unit_t* party_unit, s32 level_delta);
u8 party_calculate_highest_level(void);
party_unit_t* party_get_roster_unit(s32 party_index); /* WLDCORE's wrapper for party_unit_get */
void party_join_event_units(void); /* event library: saves the units marked to join, without the formation screen */

/* Out-of-battle generation normalizes every value except male, female and
 * monster to Ramza. The raw-stat generator indexes the tables directly. */
s32 party_unit_generate(unit_generation_type_e unit_type);

#endif
