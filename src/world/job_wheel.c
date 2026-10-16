/* The job wheel, including the cursor job level, availability, the menu, sprites, job changes, open and close
 * animations, the portrait and the job name. */

#include "fft/data_job.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/job.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_option.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "fft/world_formation.h"
#include "fft/world_menu.h"
#include "fft/world_text.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static void world_formation_build_job_wheel_availability(void);
static void world_formation_build_job_wheel_job_name(void);
static void world_formation_draw_job_wheel_sprites(s32 angle, s32 radius_x, s32 radius_y);
static void world_formation_layout_job_wheel_portrait(void);
static s32 world_formation_process_job_change(s32 job);
static void world_formation_step_job_wheel_close_animation(void);
static void world_formation_step_job_wheel_open_animation(void);
static void world_formation_update_job_wheel_state(void);
static void world_formation_draw_job_wheel_mastered_star(s32 x, s32 y, s32 otag_index, s32 value);

/* Job wheel menu-script callback 0: the selected unit's level in the job under the
 * wheel cursor, from its packed pair of 4-bit levels. The row argument is ignored. */
static s32 world_formation_get_job_wheel_cursor_job_level(s32 unused_row) {
    s32 index = job_get_generic_index(g_world_job_wheel_jobs[g_world_job_wheel_cursor_index]);
    s32 level = g_world_unit_pointers[g_unit_selected_index]->job_levels[index >> 1];

    if ((index & 1) == 0) {
        level >>= 4;
    } else {
        level &= 0xf;
    }
    return level;
}

/* Menu callback: 1 when the job under the wheel cursor can be chosen; 0 for
 * an unavailable job or a wheel with fewer than two jobs. */
static s32 world_formation_is_job_wheel_selection_available(s32 unused_row) {
    if (g_world_job_wheel_job_count < 2) {
        return 0;
    }
    return (g_world_job_wheel_jobs[g_world_job_wheel_cursor_index] & WORLD_JOB_LIST_ENTRY_UNAVAILABLE) == 0;
}

/* Index of a job's entry in the job description and requirement texts: the
 * generic jobs come first (Squire is 0), and the special jobs below Squire
 * follow from index 19 + job id. */
static s32 world_job_get_text_index(s16 job_id) {
    if (job_id < JOB_ID_SQUIRE) {
        return (s16)(job_id + 19);
    }
    return (s16)(job_id - JOB_ID_SQUIRE);
}

/*
 * Per-frame handler of the formation screen's job wheel, called by
 * world_formation_process_frame for the job-wheel menu values.
 *
 * The first call binds the job descriptions, stops the two detail panels and
 * installs the wheel's menu-script callbacks. Menu value
 * WORLD_FORMATION_MENU_JOB_WHEEL_ENTER slides the unit grid out and opens
 * the wheel; WORLD_FORMATION_MENU_JOB_WHEEL_LEAVE closes it and brings the
 * grid back. On the wheel itself:
 *  - Left/Right start a spin of 36 progress units (2 a frame with fewer than
 *    four jobs, 3 otherwise) that moves the cursor one job and wraps;
 *  - Circle on an available job starts the job-change flash
 *    (world_formation_process_job_change), after which the job is changed,
 *    with replacement equipment chosen when Max Equip at Job Change is On
 *    (its option field reads 0), and the job list rebuilt; on an unavailable job other
 *    than the unit's own it shows that job's text with the error sound;
 *  - Select shows the job's text, Square toggles the unit banner (L1/R1
 *    cycle units while it is hidden) and Cross leaves.
 * With fewer than two jobs the wheel is not drawn and Circle is refused.
 */
void world_formation_run_job_wheel_menu(void) {
    u8 unused[0x28];
    s32 i;
    u8 banner;
    u32 pad;
    u32 buttons;
    u16 job;
    s16 angle;

    if (g_world_job_wheel_initialized == 0) {
        g_world_job_wheel_spin_progress = 0;
        world_text_bind_section_data(WORLD_TEXT_DATA_JOB_DESCRIPTIONS);
        g_world_job_wheel_saved_browse_enabled = g_world_formation_unit_browse_enabled;
        g_world_formation_unit_browse_enabled = 0;
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
        menu_init_scrollable_list_core(0, 0, 0);
        g_world_menu_script_callbacks[0].value = world_formation_get_job_wheel_cursor_job_level;
        g_world_menu_script_callbacks[1].value = world_formation_is_job_wheel_selection_available;
        g_world_job_wheel_job_selected = 0;
        g_world_job_change_confirmed = 0;
        g_world_job_wheel_initialized = 1;
        g_world_job_wheel_unit_index = g_unit_selected_index;
    }

    i = g_world_formation_current_menu;
    if (i == WORLD_FORMATION_MENU_JOB_WHEEL_ENTER) {
        if (world_formation_step_grid_transition(2) == 0) {
            world_formation_update_job_wheel_state();
            world_formation_build_job_wheel_job_name();
            g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_OPENING;
            world_formation_build_job_wheel_availability();
        }
        return;
    }

    /* The close animation ends by storing JOB_WHEEL_OPENING; that is its done signal here. */
    if (i == WORLD_FORMATION_MENU_JOB_WHEEL_LEAVE) {
        if (g_world_job_wheel_job_selected != 0) {
            world_formation_step_job_wheel_close_animation();
            world_formation_draw_unit_sprite(g_unit_selected_index, &g_world_job_wheel_unit_sprite_position, 0, 0x32);
            if (g_world_formation_current_menu == WORLD_FORMATION_MENU_JOB_WHEEL_OPENING) {
                g_world_job_wheel_job_selected = 0;
                g_world_formation_current_menu = i;
            }
        } else if (world_formation_step_grid_return_transition(2) == 0) {
            g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
            g_world_job_wheel_initialized = 0;
        }
        return;
    }

    /* A job change in progress: step the flash, and apply the job once it reports done. */
    if (g_world_job_change_anim_state != WORLD_JOB_CHANGE_ANIM_DONE) {
        g_world_job_change_anim_state
            = world_formation_process_job_change(g_world_job_wheel_jobs[g_world_job_wheel_cursor_index]);
        if (g_world_job_change_anim_state == WORLD_JOB_CHANGE_ANIM_DONE) {
            i = g_main_game_options.value & GAME_OPTIONS_MAX_EQUIP_AT_JOB_CHANGE_MASK;
            world_formation_change_unit_job(
                g_unit_selected_index, g_world_job_wheel_jobs[g_world_job_wheel_cursor_index], i == 0);
            world_formation_stage_selected_unit();
            world_job_build_unit_list(
                g_unit_selected_index, g_world_job_wheel_jobs, WORLD_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS);
            g_world_formation_unit_banner_enabled = g_world_formation_saved_banner_enabled;
            world_formation_layout_job_wheel_portrait();
            g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL;
        }

        /* Dissolve: each corner ramps a lead channel (two for corner 3) to 0xFF; the others dim
         * until it saturates, then brighten. Closing eases every channel back to 0x80. */
        if (g_world_job_change_anim_state == WORLD_JOB_CHANGE_ANIM_DISSOLVE) {
            g_world_job_change_flash_colors[0].r
                = g_world_job_change_flash_colors[0].r < 0xFE ? g_world_job_change_flash_colors[0].r + 1 : 0xFF;
            g_world_job_change_flash_colors[0].g = g_world_job_change_flash_colors[0].r == 0xFF
                ? g_world_job_change_flash_colors[0].g + 1
                : g_world_job_change_flash_colors[0].g - 1;
            g_world_job_change_flash_colors[0].b = g_world_job_change_flash_colors[0].r == 0xFF
                ? g_world_job_change_flash_colors[0].b + 1
                : g_world_job_change_flash_colors[0].b - 1;
            g_world_job_change_flash_colors[1].g
                = g_world_job_change_flash_colors[1].g < 0xFE ? g_world_job_change_flash_colors[1].g + 1 : 0xFF;
            g_world_job_change_flash_colors[1].r = g_world_job_change_flash_colors[1].g == 0xFF
                ? g_world_job_change_flash_colors[1].r + 1
                : g_world_job_change_flash_colors[1].r - 1;
            g_world_job_change_flash_colors[1].b = g_world_job_change_flash_colors[1].g == 0xFF
                ? g_world_job_change_flash_colors[1].b + 1
                : g_world_job_change_flash_colors[1].b - 1;
            g_world_job_change_flash_colors[2].b
                = g_world_job_change_flash_colors[2].b < 0xFE ? g_world_job_change_flash_colors[2].b + 1 : 0xFF;
            g_world_job_change_flash_colors[2].r = g_world_job_change_flash_colors[2].b == 0xFF
                ? g_world_job_change_flash_colors[2].r + 1
                : g_world_job_change_flash_colors[2].r - 1;
            g_world_job_change_flash_colors[2].g = g_world_job_change_flash_colors[2].b == 0xFF
                ? g_world_job_change_flash_colors[2].g + 1
                : g_world_job_change_flash_colors[2].g - 1;
            g_world_job_change_flash_colors[3].r
                = g_world_job_change_flash_colors[3].r < 0xFE ? g_world_job_change_flash_colors[3].r + 1 : 0xFF;
            g_world_job_change_flash_colors[3].g
                = g_world_job_change_flash_colors[3].g < 0xFE ? g_world_job_change_flash_colors[3].g + 1 : 0xFF;
            g_world_job_change_flash_colors[3].b = g_world_job_change_flash_colors[3].r == 0xFF
                ? g_world_job_change_flash_colors[3].b + 1
                : g_world_job_change_flash_colors[3].b - 1;
        } else if (g_world_job_change_anim_state == WORLD_JOB_CHANGE_ANIM_CLOSING) {
            for (i = 0; i < 4; i++) {
                g_world_job_change_flash_colors[i].r = g_world_job_change_flash_colors[i].r < 0x80
                    ? g_world_job_change_flash_colors[i].r + 1
                    : g_world_job_change_flash_colors[i].r - 1;
                g_world_job_change_flash_colors[i].g = g_world_job_change_flash_colors[i].g >= 0x80
                    ? g_world_job_change_flash_colors[i].g - 1
                    : g_world_job_change_flash_colors[i].g + 1;
                g_world_job_change_flash_colors[i].b = g_world_job_change_flash_colors[i].b >= 0x80
                    ? g_world_job_change_flash_colors[i].b - 1
                    : g_world_job_change_flash_colors[i].b + 1;
            }
        }

        gfx_enqueue_gouraud_textured_quad(&g_world_job_wheel_portrait_quad, g_world_job_change_flash_colors, 0, 0x33);
        gfx_enqueue_gouraud_textured_quad(
            &g_world_job_wheel_new_portrait_quad, g_world_job_change_flash_colors, 0, 0x33);
        world_formation_draw_job_wheel_sprites(0, ONE, ONE);
    } else {
        banner = g_world_formation_unit_banner_enabled;
        if (banner != 0) {
            g_world_formation_unit_cycle_mode = 2;
        }
        if (g_menu_input_secondary_repeat & PSX_PAD_SQUARE) {
            g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
            if (banner != 0) {
                g_world_formation_unit_banner_enabled = 0;
                g_world_formation_unit_cycle_mode = 0;
            } else {
                g_world_formation_unit_banner_enabled = 1;
                g_world_formation_unit_cycle_mode = 2;
            }
            g_world_formation_saved_banner_enabled = g_world_formation_unit_banner_enabled;
        }

        /* Rebuild the wheel when L1/R1 picked another unit on an earlier frame. */
        if (g_unit_selected_index != g_world_job_wheel_unit_index) {
            world_formation_update_job_wheel_state();
        }
        if (g_world_formation_unit_banner_enabled == 0) {
            world_formation_cycle_selected_unit(g_menu_input_secondary_repeat);
        }
        world_formation_draw_unit_sprite(g_unit_selected_index, &g_world_job_wheel_unit_sprite_position, 0, 0x32);

        if (g_world_formation_current_menu == WORLD_FORMATION_MENU_JOB_WHEEL_CLOSING) {
            world_formation_step_job_wheel_close_animation();
        } else if (g_world_formation_current_menu == WORLD_FORMATION_MENU_JOB_WHEEL_OPENING) {
            world_formation_step_job_wheel_open_animation();
        } else {
            job = g_world_job_wheel_jobs[g_world_job_wheel_cursor_index];
            if (g_world_job_wheel_spin_progress == 0) {
                pad = g_menu_input_primary_repeat;
                if (pad & PSX_PAD_CROSS) {
                    /* Rebuild the full record list and keep the selection on the same roster slot. */
                    i = g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot;
                    world_formation_build_record_list(0, g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
                    g_unit_selected_index = world_formation_find_index_by_roster_slot(i);
                    g_sound_queued_effect_id = MAIN_SFX_CANCEL;
                    g_world_job_wheel_job_selected = 1;
                    g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_LEAVE;
                    g_world_formation_unit_browse_enabled = g_world_job_wheel_saved_browse_enabled;
                } else if (g_world_job_wheel_job_count < 2) {
                    if (pad & PSX_PAD_CIRCLE) {
                        g_sound_queued_effect_id = MAIN_SFX_INVALID;
                    }
                } else {
                    /* The primary repeat's high half flags buttons held past the repeat delay. */
                    buttons = g_menu_input_secondary_repeat;
                    if ((buttons & PSX_PAD_RIGHT) || ((pad >>= 16) & PSX_PAD_RIGHT)) {
                        g_world_job_wheel_spin_progress = 1;
                        g_world_job_wheel_spin_direction = 1;
                        g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
                    } else if ((buttons & PSX_PAD_LEFT) || (pad & PSX_PAD_LEFT)) {
                        g_world_job_wheel_spin_progress = 1;
                        g_world_job_wheel_spin_direction = -1;
                        g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
                    }

                    if (g_menu_input_secondary_repeat & PSX_PAD_CIRCLE) {
                        if (!(job & WORLD_JOB_LIST_ENTRY_UNAVAILABLE)) {
                            g_world_job_change_confirmed = 1;
                            for (i = 0; i < 4; i++) {
                                g_world_job_change_flash_colors[i].r = g_world_job_change_flash_colors[i].g
                                    = g_world_job_change_flash_colors[i].b = 0x80;
                            }
                            g_world_job_change_anim_state = WORLD_JOB_CHANGE_ANIM_DISSOLVE;
                            g_world_formation_saved_banner_enabled = g_world_formation_unit_banner_enabled;
                            g_world_formation_unit_banner_enabled = 0;
                            g_world_formation_unit_cycle_mode = 0;
                            g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_CHANGE;
                        } else if ((job & WORLD_JOB_LIST_ENTRY_ID_MASK)
                            != g_world_unit_pointers[g_unit_selected_index]->identity.job_id) {
                            /* The first call bound the formation text section to the job texts. */
                            world_text_show_message_and_play_sound(
                                world_job_get_text_index(job & WORLD_JOB_LIST_ENTRY_ID_MASK)
                                    + WORLD_TEXT_ID_FORMATION_TEXT_BASE,
                                MAIN_SFX_ERROR_MESSAGE);
                            g_menu_status_display_thread_params.style = 1;
                        } else {
                            g_sound_queued_effect_id = MAIN_SFX_INVALID;
                        }
                    }

                    if (g_menu_input_secondary_repeat & PSX_PAD_SELECT) {
                        if (g_world_job_wheel_job_count >= 2) {
                            i = world_job_get_text_index(job & WORLD_JOB_LIST_ENTRY_ID_MASK);
                            if (i >= 0) {
                                world_text_show_message_and_play_sound(
                                    i + WORLD_TEXT_ID_FORMATION_TEXT_BASE, MAIN_SFX_WINDOW_OPEN);
                                g_menu_status_display_thread_params.style = 1;
                            }
                        }
                    }
                }
            } else {
                /* A spin moves one job per 36 progress units; small wheels turn more slowly. */
                pad = g_world_job_wheel_job_count;
                if ((s32)pad < 4) {
                    g_world_job_wheel_spin_progress += 2;
                }
                if ((s32)pad >= 4) {
                    g_world_job_wheel_spin_progress += 3;
                }
                if (g_world_job_wheel_spin_progress >= 0x24) {
                    g_world_job_wheel_spin_progress = 0;
                    g_world_job_wheel_cursor_index += g_world_job_wheel_spin_direction;
                    if (g_world_job_wheel_cursor_index < 0) {
                        g_world_job_wheel_cursor_index = g_world_job_wheel_job_count - 1;
                    } else if (g_world_job_wheel_job_count == g_world_job_wheel_cursor_index) {
                        g_world_job_wheel_cursor_index = 0;
                    }
                    world_formation_build_job_wheel_job_name();
                }
            }

            if (g_world_job_wheel_job_count >= 2) {
                /* The shift preserves the order of the spin-state loads. */
                angle = ((g_world_job_wheel_spin_direction << 12) * g_world_job_wheel_spin_progress
                            / g_world_job_wheel_job_count)
                    / 36 % ONE;
                world_formation_draw_job_wheel_sprites(angle, ONE, ONE);
            }
            if (g_menu_input_lock_mode == 0) {
                g_menu_status_display_thread_params.style = 0;
            }
        }
    }

    menu_gfx_enqueue_textured_quad(
        &g_world_job_wheel_name_rect, 0x30, 0x30, 0, 0, g_world_menu_window_tpage, g_world_menu_window_clut, 0x33);
    menu_run_script_with_palette_mode(&g_world_job_wheel_script, 0, g_menu_input_lock_mode);
}

/*
 * Draw the job wheel's generic-job sprites around an ellipse.
 *
 * Jobs are placed backwards from the cursor at angle steps of ONE / count on
 * radii scaled by 100 and 60 (ONE = 1.0). The ordering-table index rises in
 * the first and last angle quarters and falls in the middle two. Jobs flagged
 * 0x4000 are tinted and those with a nonzero g_world_job_wheel_mastered_jobs entry get the
 * twinkling mastered star. Needs at least two jobs.
 */
static void world_formation_draw_job_wheel_sprites(s32 angle, s32 radius_x, s32 radius_y) {
    oriented_quad_t sprite;
    u8 unused_sprite_tail[0x10]; /* Preserve the original 0x24-byte stack allocation. */
    s32 step;
    s32 ot_index;
    s32 i;
    u16 index;
    s32 scaled_x;
    s32 scaled_y;
    s32 theta;
    s16 job;
    rgb24_t* rgb;
    s16 next;

    if (g_world_job_wheel_job_count < 2) {
        return;
    }
    step = ONE / g_world_job_wheel_job_count;
    g_world_job_wheel_animation_counter++;
    ot_index = 30;
    index = g_world_job_wheel_cursor_index;
    sprite.w = sprite.source.w = 0x18;
    sprite.h = sprite.source.h = 0x28;
    g_world_job_wheel_animation_counter = g_world_job_wheel_animation_counter % 102;
    scaled_x = (radius_x * 100) >> 12;
    scaled_y = (radius_y * 60) >> 12;
    for (i = 0; i < g_world_job_wheel_job_count; i++) {
        theta = (i * step + 0x400 + angle) % ONE;
        if (theta < 0x400) {
            ot_index++;
        } else if (theta < 0x800) {
            ot_index--;
        } else if (theta < 0xC00) {
            ot_index--;
        } else {
            ot_index++;
        }
        job = index;
        world_formation_get_generic_human_graphic_entry(g_world_job_wheel_jobs[job] & WORLD_JOB_LIST_ENTRY_ID_MASK,
            g_world_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_FEMALE, &sprite.source);
        sprite.position.x = g_world_job_wheel_center_x + ((scaled_x * rcos(theta)) >> 12) - 8;
        sprite.position.y = g_world_job_wheel_center_y + ((scaled_y * rsin(theta)) >> 12) - 0x14;
        rgb = 0;
        if (g_world_job_wheel_jobs[job] & WORLD_JOB_LIST_ENTRY_UNAVAILABLE) {
            rgb = &g_world_formation_dimmed_sprite_color;
        }
        if (g_world_job_wheel_mastered_jobs[job] != 0) {
            world_formation_draw_job_wheel_mastered_star(
                sprite.position.x, sprite.position.y, ot_index, g_world_job_wheel_animation_counter);
        }
        world_formation_draw_sprite_with_shadow(&sprite, rgb, ot_index);
        if (job == 0) {
            next = g_world_job_wheel_job_count - 1;
        } else {
            next = index - 1;
        }
        index = next;
    }
}

/*
 * Run one frame of the WORLD job-change graphic dissolve; returns 1 while it
 * runs, 2 during the closing frames and 0 once finished.
 *
 * The first call changes the preview record to `job`, copies the VRAM strip at
 * g_world_job_change_old_sprite_rect and the unit's new graphic into heap buffers, and shuffles a
 * 480-entry byte-index table. Each frame clears two shuffled bytes of the old
 * strip, copies the same bytes of the new graphic into an initially blank
 * strip at g_world_job_change_new_sprite_rect, uploads both, and draws a 64-line rcos/rsin wave band
 * that slides in over frames 0-19 and out over 221-239.
 * Circle or Cross skips ahead to frame 220 (once frame 40 is reached); frame
 * 240 frees the buffers.
 *
 * `j`/`k`/`n` are each reused across the phases, and the wave phase is
 * `i * k` with `k` holding the step: splitting any of them, or keeping a
 * separate phase accumulator, changes the target's register assignment.
 */
static s32 world_formation_process_job_change(s32 job) {
    union {
        texture_source_t entry;
        point16_t points[3];
    } scratch;
    RECT rect;
    s32 i;
    s32 j;
    s32 k;
    s32 n;
    s32 swing;
    s32 amplitude;
    s32 offset;
    s32 line_y;
    s32 base;
    s32 line_x;

    if (g_world_job_change_active == 0) {
        main_sound_play_tune(MAIN_SOUND_TUNE_JOB_CHANGE);
        g_world_job_change_active = 1;
        g_world_job_change_frame = 0;
        g_world_job_change_skip_requested = 0;
        menu_input_clear_state();
        g_world_job_change_old_sprite_image = main_heap_alloc(0x1e0);
        gfx_store_image_and_wait(&g_world_job_change_old_sprite_rect, g_world_job_change_old_sprite_image);
        g_world_job_change_new_sprite_image = main_heap_alloc(0x1e0);
        memset(g_world_job_change_new_sprite_image, 0, 0x1e0);
        world_formation_change_unit_job(WORLD_FORMATION_PREVIEW_UNIT_INDEX, job, 0);
        world_formation_build_unit_graphic_entry(WORLD_FORMATION_PREVIEW_UNIT_INDEX, &scratch.entry, 0);
        g_world_job_change_new_sprite_source = main_heap_alloc(0x1e0);
        if (scratch.entry.tpage == 100) {
            rect.x = (scratch.entry.u >> 2) + 0x100;
        } else {
            rect.x = (scratch.entry.u >> 2) + 0x140;
        }
        rect.y = scratch.entry.v;
        rect.w = scratch.entry.w >> 2;
        rect.h = scratch.entry.h;
        g_world_job_wheel_new_portrait_quad.clut = scratch.entry.clut;
        gfx_store_image_and_wait(&rect, g_world_job_change_new_sprite_source);
        g_world_job_change_dissolve_order = main_heap_alloc(0x3c0);
        for (i = 479; i >= 0; i--) {
            g_world_job_change_dissolve_order[i] = i;
        }
        for (i = 0; i < 300; i++) {
            j = rand() % 480;
            k = rand() % 480;
            n = g_world_job_change_dissolve_order[j];
            g_world_job_change_dissolve_order[j] = g_world_job_change_dissolve_order[k];
            g_world_job_change_dissolve_order[k] = n;
        }
    }
    for (i = 0; i < 2; i++) {
        n = g_world_job_change_dissolve_order[g_world_job_change_frame * 2 + i];
        g_world_job_change_old_sprite_image[n] = 0;
        g_world_job_change_new_sprite_image[n] = g_world_job_change_new_sprite_source[n];
    }
    LoadImage(&g_world_job_change_old_sprite_rect, (u32*)g_world_job_change_old_sprite_image);
    LoadImage(&g_world_job_change_new_sprite_rect, (u32*)g_world_job_change_new_sprite_image);
    k = 64;
    amplitude = 20;
    offset = 0;
    swing = 10;
    if (g_world_job_change_frame < 20) {
        amplitude = g_world_job_change_frame;
        offset = (20 - amplitude) * 8;
    } else if (g_world_job_change_frame > 220) {
        amplitude = 240 - g_world_job_change_frame;
        offset = (g_world_job_change_frame - 220) * 8;
    }
    scratch.points[0].y = 0;
    for (i = 0; i < 64; i++) {
        n = (i * k + g_world_job_change_frame * 4) % ONE;
        line_x = ((amplitude * rcos(n)) >> 12) + 0x80;
        scratch.points[0].x = line_x;
        scratch.points[1].x = line_x;
        scratch.points[2].x = line_x;
        base = offset - 146;
        line_y = ((swing * rsin(n)) >> 12) - base;
        scratch.points[2].y = scratch.points[1].y = base = line_y;
        line_y -= 64;
        scratch.points[1].y = line_y - offset;
        world_menu_add_gradient_line_primitive(&scratch.points[0], g_world_job_change_wave_colors, 1, 0x38);
        scratch.points[1].y++;
        world_menu_add_gradient_line_primitive(&scratch.points[1], g_world_job_change_wave_colors + 1, 1, 0x38);
    }
    menu_gfx_enqueue_draw_mode(0, 0, GetTPage(0, 3, 0x100, 0), 0, 0x37);
    g_world_job_change_frame++;
    if (g_world_job_change_skip_requested != 0) {
        if (g_world_job_change_frame >= 40 && g_world_job_change_frame <= 220) {
            g_world_job_change_frame = 220;
            main_sound_set_tune_volume(0x46);
        }
    } else if (g_menu_input_newly_pressed & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
        g_world_job_change_skip_requested = 1;
    }
    if (g_world_job_change_frame < 220) {
        return WORLD_JOB_CHANGE_ANIM_DISSOLVE;
    }
    if (g_world_job_change_frame < 240) {
        return WORLD_JOB_CHANGE_ANIM_CLOSING;
    }
    main_heap_free(g_world_job_change_old_sprite_image);
    main_heap_free(g_world_job_change_new_sprite_image);
    main_heap_free(g_world_job_change_new_sprite_source);
    main_heap_free(g_world_job_change_dissolve_order);
    g_world_job_change_active = 0;
    return WORLD_JOB_CHANGE_ANIM_DONE;
}

/* Begin the job wheel's close animation; with fewer than two jobs there is
 * no wheel and WORLD_FORMATION_MENU_JOB_WHEEL_OPENING is set at once. */
static void world_formation_start_job_wheel_close(void) {
    if (g_world_job_wheel_job_count < 2) {
        g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_OPENING;
    } else {
        g_world_job_wheel_closing = 1;
    }
    g_world_job_wheel_close_frame = 0;
    g_world_job_wheel_unit_index = g_unit_selected_index;
}

/* One frame of the job wheel closing: over 20 frames the wheel turns a
 * quarter turn while its radius grows from 1.0 by 0x140 a frame. It ends by
 * setting the menu value WORLD_FORMATION_MENU_JOB_WHEEL_OPENING. */
static void world_formation_step_job_wheel_close_animation(void) {
    s32 pos;

    if (g_world_job_wheel_closing == 0) {
        world_formation_start_job_wheel_close();
    }
    pos = g_world_job_wheel_close_frame * 0x140 + ONE;
    world_formation_draw_job_wheel_sprites(((g_world_job_wheel_close_frame + 1) << 10) / 20, pos, pos);
    g_world_job_wheel_close_frame++;
    if (g_world_job_wheel_close_frame >= 0x14) {
        g_world_job_wheel_closing = 0;
        g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_OPENING;
    }
}

/*
 * Build the selected unit's job-wheel choices and availability flags.
 *
 * Units away on propositions have no choices. Fewer than 2 choices bypass
 * the wheel; otherwise its opening animation starts at 20 frames.
 */
static void world_formation_build_job_wheel_availability(void) {
    s32 i;
    s16 unit;
    u16 entry;
    s32 id;

    g_world_job_wheel_job_count = world_job_build_unit_list(
        g_unit_selected_index, g_world_job_wheel_jobs, WORLD_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS);
    for (i = 0; i < g_world_job_wheel_job_count; i++) {
        unit = g_unit_selected_index;
        entry = g_world_job_wheel_jobs[i];
        id = entry & WORLD_JOB_LIST_ENTRY_ID_MASK;
        if (id == g_world_unit_pointers[unit]->identity.job_id || !(entry & WORLD_JOB_LIST_ENTRY_UNAVAILABLE)) {
            g_world_job_wheel_mastered_jobs[i]
                = ability_build_list(unit, id, MENU_ABILITY_CATEGORY_ALL, 0, ABILITY_LIST_MODE_COUNT_UNLEARNED) == 0;
        } else {
            g_world_job_wheel_mastered_jobs[i] = 0;
        }
    }
    if (g_world_unit_pointers[g_unit_selected_index]->proposition_status != 0) {
        g_world_job_wheel_job_count = 0;
    }
    g_world_job_wheel_cursor_index = 0;
    world_formation_build_job_wheel_job_name();
    if (g_world_job_wheel_job_count < 2) {
        g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL;
        return;
    }
    g_world_job_wheel_open_frame = 0x14;
    g_world_job_wheel_opening = 1;
}

/* One frame of the job wheel opening: the wheel turns through the last
 * quarter turn into place while its radius shrinks by 0x140 a frame to 1.0.
 * It ends by setting the menu value WORLD_FORMATION_MENU_JOB_WHEEL. */
static void world_formation_step_job_wheel_open_animation(void) {
    s32 pos;

    if (g_world_job_wheel_opening == 0) {
        world_formation_build_job_wheel_availability();
    }
    pos = g_world_job_wheel_open_frame * 0x140 + ONE;
    world_formation_draw_job_wheel_sprites(((20 - g_world_job_wheel_open_frame) << 10) / 20 + 0xC00, pos, pos);
    g_world_job_wheel_open_frame--;
    if (g_world_job_wheel_open_frame == 0) {
        g_world_job_wheel_opening = 0;
        g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL;
    }
}

/* Restart the wheel for a newly selected unit (and on entry): an idle wheel starts its close
 * animation, which hands over to the open animation; a closing wheel switches straight to
 * opening; an opening wheel rebuilds its jobs for the new unit and adds the secondary repeat
 * period (divided by the event speed) to its remaining open frames, capped at 16. Lays the
 * portrait out again. */
static void world_formation_update_job_wheel_state(void) {
    s32 step;
    /* The target frame is 0x30 bytes; 0x18 of unreferenced locals. */
    s32 unused[6];

    if (g_world_job_wheel_closing != 0) {
        g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_OPENING;
        g_world_job_wheel_closing = 0;
        g_world_job_wheel_unit_index = g_unit_selected_index;
    } else {
        if (g_world_job_wheel_opening != 0) {
            step = g_world_job_wheel_open_frame;
            g_world_job_wheel_unit_index = g_unit_selected_index;
            world_formation_build_job_wheel_availability();
            g_world_job_wheel_open_frame = step + g_main_input_secondary_repeat_period / menu_get_event_speed();
            if (g_world_job_wheel_open_frame >= 0x11) {
                g_world_job_wheel_open_frame = 0x10;
            }
            g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_OPENING;
        } else {
            g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_CLOSING;
        }
    }
    world_formation_layout_job_wheel_portrait();
}

/* Center the two job-wheel portrait quads on (0x80, 0x7b) for the selected
 * unit's formation graphic and queue a VRAM move of that graphic to
 * (0x240, 0x130), where g_world_job_wheel_portrait_quad samples it. */
static void world_formation_layout_job_wheel_portrait(void) {
    texture_source_t entry;
    RECT rect;
    s16 x;
    s16 y;

    world_formation_build_unit_graphic_entry(g_unit_selected_index, &entry, 0);
    x = 0x80 - (entry.w >> 1);
    y = 0x7b - (entry.h >> 1);
    g_world_job_wheel_portrait_quad.y = y;
    g_world_job_wheel_new_portrait_quad.y = y;
    g_world_job_wheel_portrait_quad.x = x;
    g_world_job_wheel_new_portrait_quad.x = x;
    g_world_job_wheel_portrait_quad.clut = entry.clut;
    if (entry.tpage == 0x64) {
        rect.x = (entry.u >> 2) + 0x100;
    } else {
        rect.x = (entry.u >> 2) + 0x140;
    }
    rect.y = entry.v;
    rect.w = entry.w >> 2;
    rect.h = entry.h;
    menu_gfx_enqueue_draw_move(&rect, 0x240, 0x130, 0);
}

/* Render the job wheel's centered job name ("Can't change" when fewer than two
 * jobs are listed). */
static void world_formation_build_job_wheel_job_name(void) {
    RECT destination_rect;
    u8* font;
    s16* name;
    s8 pixel_width;

    if (g_world_job_wheel_job_count < 2) {
        font = g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN];
        g_world_job_wheel_name_text = 0x1B;
    } else {
        font = g_text_section_pointers[TEXT_SECTION_JOB_NAMES];
        g_world_job_wheel_name_text = g_world_job_wheel_jobs[g_world_job_wheel_cursor_index];
    }
    name = &g_world_job_wheel_name_text;
    destination_rect.x = 0x24C;
    destination_rect.y = 0x130;
    destination_rect.w = 0x14;
    destination_rect.h = 0x10;
    world_text_render_id_list_to_image_rows(font, name, &destination_rect, 0);
    pixel_width = world_text_measure_entry_pixel_width(menu_text_find_entry_by_index(font, *name & 0x3FF, 2));
    g_world_job_wheel_name_rect.x = ((0x50 - pixel_width) >> 1) + 0x58;
    g_world_job_wheel_name_rect.y = 0xD7;
    g_world_job_wheel_name_rect.w = pixel_width;
    g_world_job_wheel_name_rect.h = 0xE;
}

/*
 * Draws the mastered-job star over a job-wheel sprite at (x, y).
 *
 * `value` is the wheel's animation counter. The star twinkles twice per
 * cycle: it runs through frames 1..4 over counts 0x3C..0x4F, and again from
 * 0x5E (frame 4 from 0x64 on); otherwise it shows frame 0. Frames are 10
 * texels apart from u 0x24.
 */
static void world_formation_draw_job_wheel_mastered_star(s32 x, s32 y, s32 otag_index, s32 value) {
    s32 level;
    oriented_quad_t* sprite;

    if (value < 0x50) {
        level = 0;
        if (value >= 0x3C) {
            level = 1;
            if (value >= 0x42) {
                level = 2;
                if (value >= 0x48) {
                    level = 4;
                    if (value < 0x4C) {
                        level = 3;
                    }
                }
            }
        }
    } else {
        level = 0;
        if (value >= 0x5E) {
            level = 1;
            if (value >= 0x60) {
                level = 2;
                if (value >= 0x62) {
                    level = 4;
                    if (value < 0x64) {
                        level = 3;
                    }
                }
            }
        }
    }
    sprite = &g_world_job_wheel_mastered_icon_sprite;
    sprite->position.x = x + 7;
    g_world_job_wheel_mastered_icon_sprite.position.y = y - 0xC;
    g_world_job_wheel_mastered_icon_sprite.source.u = level * 10 + 0x24;
    menu_gfx_enqueue_oriented_textured_quad(sprite, 0, TEXTURE_UV_FLIP_NONE, 0, otag_index);
}
