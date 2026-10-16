/* Unit ability slots and locks, learning, unit graphic entries, roster lookups, unit copies, zodiac by date, monster
 * breeding and packet buffer swaps. */

#include "fft/data.h"
#include "fft/data_job.h"
#include "fft/data_skillset.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/main.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_status.h"
#include "fft/script_variable.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/world_formation.h"
#include "fft/world_gfx.h"
#include "fft/world_menu.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Change an ability slot and optionally replace incompatible equipment.
 *
 * The record is saved to the party and anything the new ability set no
 * longer allows is removed. With `update_equipment`, a unit other than the
 * preview unit and not a Mime is then given its best-fit equipment: the old
 * items return to the inventory before the replacement set is taken from it,
 * and the auto-equip banner (0xD811) shows for 30 frames. Return 1 on success
 * and -2 when the unit is locked (see formation_get_unit_lock).
 */
s32 formation_ability_set_slot(s16 unit_id, s16 slot, s16 ability, s32 update_equipment) {
    s16 equipment[MENU_EQUIPMENT_SLOT_COUNT];
    s32 i;

    if (formation_get_unit_lock(unit_id) == FORMATION_UNIT_UNLOCKED) {
        formation_unit_t* unit = g_formation_unit_pointers[unit_id];
        unit->abilities.slots[slot] = ability;
        formation_save_records();
        formation_remove_invalid_loadout(unit_id);
        if (update_equipment != 0) {
            if (unit_id != FORMATION_PREVIEW_UNIT_INDEX) {
                if (g_formation_unit_pointers[unit_id]->identity.job_id != JOB_ID_MIME) {
                    formation_item_build_best_fit(unit_id, equipment);
                    for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                        item_adjust_inventory_count(g_formation_unit_pointers[unit_id]->equipment[i], 1);
                    }
                    for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                        g_formation_unit_pointers[unit_id]->equipment[i] = equipment[i];
                        item_adjust_inventory_count(equipment[i], -1);
                    }
                    formation_save_records();
                    formation_banner_countdown(0xD811, 0x1E);
                }
            }
        }
        return 1;
    }
    return -2;
}

/* Why a formation unit cannot use a menu, as a WORLD_FORMATION_UNIT_LOCK_*
 * value: away on a proposition, a monster, an egg or a Mime, tested in that
 * order; FORMATION_UNIT_UNLOCKED otherwise. */
s32 formation_get_unit_lock(s32 unit_id) {
    formation_unit_t* unit = g_formation_unit_pointers[unit_id];

    if (unit->proposition_status != 0)
        return FORMATION_UNIT_LOCK_PROPOSITION;
    if (unit->uses_monster_skillset != 0)
        return FORMATION_UNIT_LOCK_MONSTER;
    if (job_is_special_monster(unit->identity.job_id) != 0)
        return FORMATION_UNIT_LOCK_MONSTER;
    if (g_formation_unit_pointers[unit_id]->gender_flags & UNIT_FLAG_EGG)
        return FORMATION_UNIT_LOCK_EGG;
    if (g_formation_unit_pointers[unit_id]->identity.job_id == JOB_ID_MIME)
        return FORMATION_UNIT_LOCK_MIME;
    return FORMATION_UNIT_UNLOCKED;
}

/* Mark an ability learned for a job: find its position in the job's skillset list and set that
 * bit, most significant first, in the unit's learned-ability bytes for the job. */
void formation_ability_learn(s16 unit_id, s16 job_id, s16 ability_id) {
    s32 skillset = job_get_skillset(job_id);
    s16 generic_job = job_get_generic_index(job_id);
    s16* list = (s16*)ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ALL);
    s32 i = 0;
    s32 byte;
    s32 bit;
    s32 mask;
    u8* learned_abilities;

    while (list[i] != ability_id) {
        i++;
    }
    byte = i >> 3;
    bit = 7 - (i & 7);
    mask = 1;
    i = mask << bit;
    /* Keeping the job-byte stride before the field address preserves the retail load/add order. */
    learned_abilities
        = ((formation_unit_t*)((u8*)g_formation_unit_pointers[unit_id] + generic_job * 3))->learned_abilities[0];
    learned_abilities[byte] |= i;
}

/* Build the portrait source rectangle and palette for a formation unit.
 *
 * Special jobs use fixed portrait entries; generic humans derive theirs from
 * job and gender. The third argument is retained because all known callers
 * pass it even though this function does not consume it. */
void formation_build_unit_graphic(s16 formation_index, texture_source_t* destination, s32 unused) {
    formation_unit_t* unit = g_formation_unit_pointers[formation_index];
    s16 job_id;
    s32 index;
    s32 palette;
    s16 clut_x;

    job_id = unit->identity.job_id;
    if (job_id == JOB_ID_BYBLOS) {
        index = 0x4E;
    } else if (job_id == JOB_ID_STEEL_GIANT) {
        index = 0x52;
    } else if (job_id == JOB_ID_APANDA) {
        index = 0x4E;
    } else if (job_id == JOB_ID_SERPENTARIUS) {
        index = 0x4F;
    } else if (job_id == JOB_ID_ARCHAIC_DEMON) {
        index = 0x51;
    } else if (job_id == JOB_ID_ULTIMA_DEMON) {
        index = 0x51;
    } else if (job_id == JOB_ID_HOLY_DRAGON_REIS) {
        index = 0x4C;
    } else if (unit->sprite_set == CHARACTER_IDENTITY_MONSTER) {
        index = unit->monster_graphic + 0x3D;
    } else if (unit->sprite_set < CHARACTER_IDENTITY_BOY) {
        index = g_formation_generic_human_graphic_index_table[unit->sprite_set];
    } else {
        formation_get_generic_human_graphic(job_id, unit->gender_flags & UNIT_FLAG_FEMALE, destination);
        return;
    }
    bcopy(&g_formation_graphic_entries[index], destination, sizeof(*destination));
    palette = (s16)formation_get_unit_palette_index(g_formation_unit_pointers[formation_index]->identity.roster_slot);
    clut_x = (palette % 4) * 0x10 + 0x140;
    job_id = palette / 4;
    destination->clut = GetClut(clut_x, job_id + 0xE0);
}

/* Find the current formation-list index for a roster slot.
 *
 * Narrow the requested slot to s16. On a miss, return the number of entries searched.
 */
s32 formation_find_by_roster_slot(s16 roster_slot) {
    s32 index;

    for (index = 0; index < g_formation_record_count; index++) {
        if (g_formation_unit_pointers[index]->identity.roster_slot == roster_slot) {
            break;
        }
    }
    return index;
}

/* Copy the formation graphic entry of a generic human job. Jobs below 0x35
 * are looked up in a table; of the rest, Bard and Dancer have fixed entries
 * (0x3a, 0x3b), while Mime uses 0x3c and the others job * 2 - 0x7c, both
 * plus one for a female unit. */
void formation_get_generic_human_graphic(s32 job_id, s16 is_female, texture_source_t* destination) {
    s16 id = job_id;
    s16 idx;

    if (id < JOB_ID_CHEMIST_35) {
        idx = g_formation_generic_human_graphic_index_table[id];
    } else if (id == JOB_ID_DANCER) {
        idx = 0x3B;
    } else if (id == JOB_ID_BARD) {
        idx = 0x3A;
    } else {
        if (id == JOB_ID_MIME) {
            idx = 0x3C;
        } else {
            idx = (job_id * 2) - 0x7C;
        }
        if (is_female != 0) {
            idx += 1;
        }
    }
    bcopy(&g_formation_graphic_entries[idx], destination, 0xC);
}

/*
 * Copy a formation unit into the shared temporary record.
 *
 * The snapshot caller supplies the preview index as a second argument; this
 * routine ignores it and always uses g_formation_temp_unit.
 */
void formation_copy_unit_to_temp(s16 index, s32 unused_slot) {
    bcopy(g_formation_unit_pointers[index], g_formation_temp_unit, sizeof(formation_unit_t));
}

/*
 * Look up a zodiac value from month/day upper limits.
 *
 * The 13 ordered limits include both Capricorn portions of the year.
 * No date validation is performed; exhausting the 13 limits reads 31 from
 * the following month-length data, not a valid extra zodiac entry.
 */
static s32 zodiac_get_for_date(s32 month, s32 day) {
    s32 i;

    for (i = 0; i < ZODIAC_DATE_LIMIT_COUNT; i++) {
        if (month < g_zodiac_date_limits[i].date.month) {
            break;
        }
        if (month == g_zodiac_date_limits[i].date.month) {
            if (g_zodiac_date_limits[i].date.day >= day) {
                break;
            }
        }
    }
    return g_zodiac_date_limits[i].zodiac;
}

/* Advance egg hatch counters, then make the daily monster-breeding roll.
 *
 * It first reseeds rand from the play time (frames * 101 + seconds * 10 +
 * minutes + hours), so arriving at the same play time repeats the outcome.
 * A counter that reaches 0 hatches as a monster born on the current date.
 * A successful roll selects a random roster monster and one of the three
 * offspring jobs in its Chocobo-based breeding-table entry. The new egg's
 * hatch range comes from its offspring entry, and its level is based on a
 * random non-egg roster member. */
void breeding_update(s32 unused) {
    u8 monsters[0x18];
    u8 levels[0x18];
    s32 monster_count = 0;
    s32 unit_count = 0;
    s32 i = 0;
    /* The original reused two scalar temporaries throughout; splitting them
     * into typed locals changes the callee-saved allocation (n holds the
     * egg counter, month index, rarity class, offspring job and new slot;
     * value holds the birthday, the random draw and the saved name-buffer
     * pointer). */
    s32 value;
    s32 n;
    s32 day;
    s32 month;
    u8** name_buffer;
    party_unit_t* unit;

    srand(g_save_play_time_frames * 100 + g_save_play_time_seconds * 10 + g_save_play_time_minutes
        + g_save_play_time_hours + g_save_play_time_frames);
    do {
        unit = party_get_unit(i);
        if (unit->party_id != PARTY_ID_NONE) {
            if (unit->gender_flags & UNIT_FLAG_EGG) {
                n = unit->birthday_day & 0xF;
                n -= 1;
                unit->birthday_day = (unit->birthday_day & 0xF0) | n;
                if (n == 0) {
                    unit->gender_flags = UNIT_FLAG_MONSTER;
                    day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
                    month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
                    value = 0;
                    for (n = 1; n < month; n++) {
                        value += g_breeding_month_lengths[n];
                    }
                    value += day;
                    value |= zodiac_get_for_date(month, day) << 12;
                    unit->birthday_day = value;
                    unit->zodiac = value >> 8;
                }
            } else {
                levels[unit_count++] = unit->level;
                value = job_get_skillset(unit->job_id);
                if (value >= SKILLSET_ID_MONSTER_FIRST && value < SKILLSET_ID_END) {
                    monsters[monster_count++] = i;
                }
            }
        }
        i++;
    } while (i < PARTY_ROSTER_SLOT_COUNT);

    if (monster_count == 0) {
        return;
    }
    /* The day's roll out of 256: below 6 rare, below 16 uncommon, below 32 common, else no egg. */
    value = rand() & 0xFF;
    if (value < 6) {
        n = BREEDING_RARITY_RARE;
    } else if (value < 0x10) {
        n = BREEDING_RARITY_UNCOMMON;
    } else {
        n = value < 0x20; /* COMMON or NONE */
    }
    if (n == BREEDING_RARITY_NONE) {
        return;
    }
    monster_count = rand() % monster_count;
    n = g_breeding_table[party_get_unit(monsters[monster_count])->job_id - JOB_ID_CHOCOBO]
            .offspring_job_by_rarity[n - BREEDING_RARITY_COMMON];
    i = rand() % g_breeding_table[n - JOB_ID_CHOCOBO].hatch_range + 1;
    if (i < 2) {
        i = 2;
    }
    /* The egg's hatch count goes in both nibbles; the low one counts down. Name generation
     * reads the unit-name table through the roster-name pointer, swapped for the call as
     * formation_generate_unit does. */
    name_buffer = &g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    value = (s32)*name_buffer;
    *name_buffer = g_text_section_pointers[TEXT_SECTION_UNIT_NAMES];
    n = party_create_monster_egg(n, i + i * 0x10, rand() % 4);
    *name_buffer = (u8*)value;
    if (n != -1) {
        unit = party_get_unit(n);
        party_level_unit_to_target(unit, levels[rand() % unit_count] - 1);
    }
}

/*
 * Present the completed ordering table and prepare the other packet buffer.
 *
 * Resets the primitive pool counters, waits for the GPU and the frame step's
 * VSyncs, then flips to the other packet buffer (applying its display and
 * draw environments), submits the finished ordering table from entry
 * draw_otag (-1 skips submission) and clears the new buffer's table from
 * first_otag onward.
 */
void screen_present_frame(s32 first_otag, s32 draw_otag) {
    DRAWENV env;
    DRAWENV* envp;
    menu_gfx_context_t* context;
    u32* otag = g_menu_gfx_context->otag;

    system_frame_stub();
    envp = &env;
    /* Keep the stack address available before resetting the packet counters;
     * otherwise GCC moves its calculation into GetDrawEnv's delay slot. */
    __asm__("" : : "r"(envp));
    g_menu_gfx_poly_ft4_count = 0;
    g_gfx_poly_g4_count = 0;
    g_menu_gfx_poly_gt4_count = 0;
    g_gfx_line_f2_count = 0;
    g_screen_gradient_line_count = 0;
    g_gfx_tile_count = 0;
    g_menu_gfx_draw_move_count = 0;
    g_screen_draw_area_count = 0;
    g_menu_gfx_draw_mode_count = 0;
    GetDrawEnv(envp);
    g_screen_draw_buffer_clip_y = envp->clip.y;
    while (DrawSync(1) != 0) { }
    VSync(g_menu_event_speed);
    context = g_screen_packet_buffers;
    if (g_menu_gfx_context == context) {
        context++;
    }
    g_menu_gfx_context = context;
    PutDispEnv(&context->disp_env);
    PutDrawEnv(&g_menu_gfx_context->draw_env);
    if (draw_otag != -1) {
        DrawOTag(&otag[draw_otag]);
    }
    ClearOTag(&g_menu_gfx_context->otag[first_otag], g_screen_ot_length - first_otag);
}

/* Initializes every primitive pool of a packet buffer with its libgpu
 * constructor; quads additionally get texture shading enabled. */
static void screen_init_packet_pools(menu_gfx_context_t* buffer) {
    s32 i;

    for (i = 0; i < g_screen_textured_quad_capacity; i++) {
        SetPolyFT4(&buffer->textured_quads[i]);
        SetShadeTex(&buffer->textured_quads[i], 0);
    }
    for (i = 0; i < g_screen_gradient_quad_capacity; i++) {
        SetPolyG4(&buffer->gouraud_quads[i]);
        SetShadeTex(&buffer->gouraud_quads[i], 0);
    }
    for (i = 0; i < g_screen_textured_gradient_quad_capacity; i++) {
        SetPolyGT4(&buffer->gouraud_textured_quads[i]);
        SetShadeTex(&buffer->gouraud_textured_quads[i], 0);
    }
    for (i = 0; i < g_screen_flat_line_capacity; i++) {
        SetLineF2(&buffer->flat_lines[i]);
    }
    for (i = 0; i < g_screen_gradient_line_capacity; i++) {
        SetLineG2(&buffer->gouraud_lines[i]);
    }
    for (i = 0; i < g_gfx_tile_capacity; i++) {
        SetTile(&buffer->tiles[i]);
    }
    for (i = 0; i < g_screen_draw_move_capacity; i++) {
        SetDrawMove(&buffer->draw_moves[i]);
    }
}

/* Binds the primitive pools for both WORLD packet buffers, initializes their
 * draw/display environments, and presents twice to establish the active pair.
 */
void screen_init_packet_buffers(menu_gfx_context_t* buffers, u32* otags, void* unused_2, void* unused_3, void* unused_4,
    POLY_FT4* textured_quads, void* unused_6, void* unused_7, POLY_G4* gradient_quads,
    POLY_GT4* textured_gradient_quads, LINE_F2* flat_lines, void* unused_11, void* unused_12, LINE_G2* gradient_lines,
    void* unused_14, void* unused_15, TILE* tiles, void* unused_17, void* unused_18, void* unused_19, void* unused_20,
    void* unused_21, void* unused_22, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes) {
    s32 i;

    g_screen_packet_buffers = buffers;
    g_menu_gfx_context = buffers;
    for (i = 0; i < 2; i++) {
        g_screen_packet_buffers[i].otag = otags + g_screen_ot_length * i;
        g_screen_packet_buffers[i].textured_quads = textured_quads + g_screen_textured_quad_capacity * i;
        g_screen_packet_buffers[i].gouraud_quads = gradient_quads + g_screen_gradient_quad_capacity * i;
        g_screen_packet_buffers[i].gouraud_textured_quads
            = textured_gradient_quads + g_screen_textured_gradient_quad_capacity * i;
        g_screen_packet_buffers[i].flat_lines = flat_lines + g_screen_flat_line_capacity * i;
        g_screen_packet_buffers[i].gouraud_lines = gradient_lines + g_screen_gradient_line_capacity * i;
        g_screen_packet_buffers[i].tiles = tiles + g_gfx_tile_capacity * i;
        g_screen_packet_buffers[i].draw_moves = draw_moves + g_screen_draw_move_capacity * i;
        g_screen_packet_buffers[i].draw_areas = draw_areas + g_screen_draw_area_capacity * i;
        g_screen_packet_buffers[i].draw_modes = draw_modes + g_gfx_draw_mode_capacity * i;
        screen_init_packet_pools(&g_screen_packet_buffers[i]);
        SetDefDrawEnv(&g_screen_packet_buffers[i].draw_env, 0, i * 240, 255, 240);
        SetDefDispEnv(&g_screen_packet_buffers[i].disp_env, 0, i == 0 ? 240 : 0, 255, 240);
        g_screen_packet_buffers[i].draw_env.isbg = 0;
        g_screen_packet_buffers[i].draw_env.dfe = 1;
        setRGB0(&g_screen_packet_buffers[i].draw_env, 0, 0, 0);
        g_screen_packet_buffers[i].draw_env.ofs[0] = -128;
    }
    screen_present_frame(0, -1);
    screen_present_frame(0, -1);
}
