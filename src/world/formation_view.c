/* The view zoom thread, the unit palette index, formation sprites and redraw requests. */

#include "fft/data_job.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/menu_list.h"
#include "fft/menu_panel.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "fft/world_menu.h"
#include "psx/libgte.h"
#include "psx/types.h"

static zodiac_draw_context_t g_formation_view
    = { 0, 128, 1, 8192, 8192, { 0, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 1, 128, 0, 0, 1, 0, 1 }, { 0, 0 } } };
static u16 g_formation_sprite_by_portrait[212] = { 0, 0, 1, 2, 3, 120, 121, 4, 122, 123, 124, 125, 5, 6, 126, 7, 127,
    13, 9, 10, 11, 12, 19, 13, 129, 14, 15, 130, 16, 131, 23, 17, 133, 18, 19, 134, 135, 136, 137, 138, 18, 14, 20, 139,
    21, 141, 142, 20, 21, 140, 22, 0, 23, 0, 0, 0, 0, 0, 141, 142, 143, 0, 144, 0, 145, 0, 0, 146, 0, 147, 0, 0, 24, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65,
    66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 0, 81, 82, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 113, 114, 115,
    116, 117, 118, 119, 0, 0 };

/* Thread that draws the zodiac backdrop each frame, zooming it from double size down to normal
 * (1/16 per VSync of the event speed) unless parameter 1 starts it at normal size, until
 * parameter 3 asks it to stop. */
void formation_zodiac_backdrop_thread(void) {
    s32 frame;

    if (g_formation_screen_active != 0) {
        g_formation_view.offset.position.x = 0;
        g_formation_view.offset.position.y = 8;
    } else {
        g_formation_view.offset.position.x = 0x100;
        g_formation_view.offset.position.y = 0x80;
    }
    if (thread_get_current_parameter_1() == 0) {
        g_formation_view.scale_x = 0x2000;
        g_formation_view.scale_y = 0x2000;
    } else {
        g_formation_view.scale_x = ONE;
        g_formation_view.scale_y = ONE;
    }
    for (frame = 0;; frame++) {
        g_formation_view.ot = g_formation_ordering_table;
        if (g_formation_view.scale_x > ONE) {
            s32 step = g_script_event_speed << 8;
            g_formation_view.scale_x -= step;
            g_formation_view.scale_y -= step;
        }
        g_formation_view.brightness = g_screen_color_red;
        menu_build_zodiac_primitives(&g_formation_view, g_formation_view_quads[frame & 1]);
        thread_yield();
        if (thread_get_current_parameter_3() != 0) {
            thread_exit_current();
        }
    }
}

/*
 * Compute the formation sprite palette index of a party unit; the caller
 * turns it into the CLUT at (0x140 + (index % 4) * 16, 0xe0 + index / 4).
 *
 * Generic male/female identities derive their sheet from the job (the same
 * sheet numbering menu_build_portrait_poly uses); other selectors take the
 * job's spritesheet. Monster sprites from 0x3e on have three palettes each,
 * and the job's portrait palette picks one. `job` is only loaded for
 * selectors other than the generic humans, so this step assumes no other
 * unit's sprite reaches 0x3e.
 */
s32 formation_get_unit_palette_index(s32 party_index) {
    party_unit_t* unit = main_party_get_unit(party_index);
    job_data_t* job;
    s32 portrait;
    s32 product;
    s32 palette_base;

    if (unit->sprite_set >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        if (unit->sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
            s32 base = (unit->job_id - JOB_ID_SQUIRE) * 2;
            portrait = base + JOB_SHEET_GENERIC_MALE_BASE;
            if (unit->job_id == JOB_ID_MIME) {
                portrait = base + JOB_SHEET_GENERIC_MALE_MIME_BASE;
            }
        } else if (unit->sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
            s32 base = (unit->job_id - JOB_ID_SQUIRE) * 2;
            portrait = base + JOB_SHEET_GENERIC_FEMALE_BASE;
            if (unit->job_id == JOB_ID_MIME) {
                portrait = base + JOB_SHEET_GENERIC_FEMALE_MIME_BASE;
            }
        } else {
            job = main_job_get_data_pointer(unit->job_id);
            portrait = job->spritesheet_id;
        }
        if (unit->job_id == JOB_ID_BARD) {
            portrait = JOB_SHEET_BARD;
        }
        if (unit->job_id == JOB_ID_DANCER) {
            portrait = JOB_SHEET_DANCER;
        }
    } else {
        portrait = unit->sprite_set;
    }
    portrait = g_formation_sprite_by_portrait[portrait];
    if (unit->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
        portrait = 0;
    }
    if (unit->sprite_set == CHARACTER_IDENTITY_MONSTER) {
        portrait |= job->job_portrait_palette << 10;
    }
    portrait &= 0xFF;
    if (portrait == 0x18) {
        return 0x7A;
    }
    if (portrait >= MENU_WLDFACE_FIRST_MONSTER_PORTRAIT) {
        portrait = g_monster_formation_sprite_table[portrait - MENU_WLDFACE_FIRST_MONSTER_PORTRAIT];
    } else if (unit->sprite_set & CHARACTER_IDENTITY_HIGH_BIT_MASK) {
        portrait -= 1;
    }
    if (portrait >= 0x3E) {
        portrait -= 0x3E;
        product = portrait * 3;
        palette_base = job->job_portrait_palette + 0x3E;
        portrait = product + palette_base;
    }
    return portrait;
}

/* Resolve a party member's 32-bit formation sprite value: bits 0-9 select the
 * sprite, and for monsters bits 10-11 carry the job's portrait palette. */
s32 formation_get_party_unit_sprite(s32 party_index) {
    party_unit_t* party_unit = main_party_get_unit(party_index);
    job_data_t* job_data;
    u32 sprite_set;
    s32 portrait_id;
    s32 formation_sprite;

    sprite_set = party_unit->sprite_set;
    portrait_id = sprite_set;
    if (sprite_set >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        if (sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
            portrait_id = (party_unit->job_id - JOB_ID_SQUIRE) * 2 + JOB_SHEET_GENERIC_MALE_BASE;
            if (party_unit->job_id == JOB_ID_MIME) {
                portrait_id -= 2;
            }
        } else if (sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
            portrait_id = (party_unit->job_id - JOB_ID_SQUIRE) * 2 + JOB_SHEET_GENERIC_FEMALE_BASE;
            if (party_unit->job_id == JOB_ID_MIME) {
                portrait_id -= 2;
            }
        } else {
            job_data = main_job_get_data_pointer(party_unit->job_id);
            portrait_id = job_data->spritesheet_id;
        }
        if (party_unit->job_id == JOB_ID_BARD) {
            portrait_id = JOB_SHEET_BARD;
        }
        if (party_unit->job_id == JOB_ID_DANCER) {
            portrait_id = JOB_SHEET_DANCER;
        }
    }

    formation_sprite = g_formation_sprite_by_portrait[portrait_id];
    sprite_set = party_unit->sprite_set;
    if (sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
        formation_sprite = 0;
    }
    if (sprite_set == CHARACTER_IDENTITY_MONSTER) {
        formation_sprite |= job_data->job_portrait_palette << 10;
    }
    return formation_sprite;
}

/* Ask the scrolling text list thread in thread_id to redraw its page. */
void screen_thread_request_redraw(s32 thread_id) {
    g_threads[thread_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] = 1;
}
