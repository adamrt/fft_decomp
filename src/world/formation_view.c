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

/* Thread that draws the zodiac backdrop each frame, zooming it from double size down to normal
 * (1/16 per VSync of the event speed) unless parameter 1 starts it at normal size, until
 * parameter 3 asks it to stop. */
void world_formation_view_zoom_thread(void) {
    s32 frame;

    if (g_formation_screen_active != 0) {
        g_world_formation_view.offset.position.x = 0;
        g_world_formation_view.offset.position.y = 8;
    } else {
        g_world_formation_view.offset.position.x = 0x100;
        g_world_formation_view.offset.position.y = 0x80;
    }
    if (thread_get_current_parameter_1() == 0) {
        g_world_formation_view.scale_x = 0x2000;
        g_world_formation_view.scale_y = 0x2000;
    } else {
        g_world_formation_view.scale_x = ONE;
        g_world_formation_view.scale_y = ONE;
    }
    for (frame = 0;; frame++) {
        g_world_formation_view.ot = g_world_formation_ordering_table;
        if (g_world_formation_view.scale_x > ONE) {
            s32 step = g_script_event_speed << 8;
            g_world_formation_view.scale_x -= step;
            g_world_formation_view.scale_y -= step;
        }
        g_world_formation_view.brightness = g_world_menu_color_red;
        menu_build_zodiac_primitives(&g_world_formation_view, g_world_formation_view_quads[frame & 1]);
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
s32 world_formation_get_unit_palette_index(s32 party_index) {
    party_unit_t* unit = main_party_get_unit(party_index);
    job_data_t* job;
    s32 portrait;
    s32 product;
    s32 palette_base;

    if (unit->sprite_set >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        if (unit->sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
            s32 base = (unit->job_id - JOB_ID_SQUIRE) * 2;
            portrait = base + 0x60;
            if (unit->job_id == JOB_ID_MIME) {
                portrait = base + 0x5E;
            }
        } else if (unit->sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
            s32 base = (unit->job_id - JOB_ID_SQUIRE) * 2;
            portrait = base + 0x61;
            if (unit->job_id == JOB_ID_MIME) {
                portrait = base + 0x5F;
            }
        } else {
            job = main_job_get_data_pointer(unit->job_id);
            portrait = job->spritesheet_id;
        }
        if (unit->job_id == JOB_ID_BARD) {
            portrait = 0x82;
        }
        if (unit->job_id == JOB_ID_DANCER) {
            portrait = 0x83;
        }
    } else {
        portrait = unit->sprite_set;
    }
    portrait = g_world_formation_sprite_by_portrait[portrait];
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
s32 world_get_party_unit_formation_sprite(s32 party_index) {
    party_unit_t* party_unit = main_party_get_unit(party_index);
    job_data_t* job_data;
    u32 sprite_set;
    s32 portrait_id;
    s32 formation_sprite;

    sprite_set = party_unit->sprite_set;
    portrait_id = sprite_set;
    if (sprite_set >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        if (sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
            portrait_id = (party_unit->job_id - JOB_ID_SQUIRE) * 2 + 0x60;
            if (party_unit->job_id == JOB_ID_MIME) {
                portrait_id -= 2;
            }
        } else if (sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
            portrait_id = (party_unit->job_id - JOB_ID_SQUIRE) * 2 + 0x61;
            if (party_unit->job_id == JOB_ID_MIME) {
                portrait_id -= 2;
            }
        } else {
            job_data = main_job_get_data_pointer(party_unit->job_id);
            portrait_id = job_data->spritesheet_id;
        }
        if (party_unit->job_id == JOB_ID_BARD) {
            portrait_id = 0x82;
        }
        if (party_unit->job_id == JOB_ID_DANCER) {
            portrait_id = 0x83;
        }
    }

    formation_sprite = g_world_formation_sprite_by_portrait[portrait_id];
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
void world_thread_request_redraw(s32 thread_id) {
    g_threads[thread_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] = 1;
}
