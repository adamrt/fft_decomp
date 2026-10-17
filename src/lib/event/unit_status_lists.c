/* Unit status lists, billboard data, text section pointer relocation, the dismiss and join script instructions, height
 * display, sprite pages, hovered and preview unit stats, panel threads, input handling, window sprites and CLUT
 * selection. */

#include "fft/ability.h"
#include "fft/ability_list.h"
#include "fft/action.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/calendar.h"
#include "fft/command.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_window.h"
#include "fft/party.h"
#include "fft/portrait.h"
#include "fft/result.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/sound.h"
#include "fft/sprite_sheet.h"
#include "fft/status.h"
#include "fft/status_screen.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_event.h"
#include "fft/unit_identity.h"
#include "fft/unit_panel.h"
#include "fft/unit_party.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* A stats panel's two draw offsets (unit_view_stats_panel_thread). */
typedef struct menu_unit_status_offsets {
    menu_status_panel_draw_offset_t a; /* 0x00 */
    menu_status_panel_draw_offset_t b; /* 0x10 */
} menu_unit_status_offsets_t;

/* The mode argument of unit_view_stats_panel_thread, the one thread body of the four unit stats panels: which panel
 * the thread draws; each value has one caller. The panel slides in, redraws each frame and slides out on the stop
 * request. */
typedef enum unit_stats_panel {
    UNIT_STATS_PANEL_HOVERED_UNIT = 0,  /* unit_view_refresh_hovered_stats */
    UNIT_STATS_PANEL_ATTACK_CASTER = 1, /* unit_view_preview_caster_stats */
    UNIT_STATS_PANEL_ATTACK_TARGET = 2, /* unit_view_preview_target_stats; shows the reaction ability name */
    UNIT_STATS_PANEL_RIGHT_UNIT = 3,    /* unit_view_refresh_right_stats */
} unit_stats_panel_e;

/* Double-buffered 0x2ec-byte page of a unit stats panel (g_unit_view_stats_panel_pages; each mode's first page
 * comes from g_unit_view_stats_first_page_by_mode). Provisional. */
typedef struct unit_view_stats_page {
    POLY_G4 bars[3];                   /* 0x000: HP/MP/CT gauges */
    DR_MODE draw_mode_1;               /* 0x06c */
    DR_MODE draw_mode_0;               /* 0x078 */
    menu_palette_primitives_t palette; /* 0x084 */
    POLY_FT4 quads[5];                 /* 0x170 */
    SPRT sprites[7];                   /* 0x238 */
    POLY_FT4 portrait;                 /* 0x2c4 */
} unit_view_stats_page_t;

/* Double-buffered 0xf0-byte frame of the unit summary panel. Provisional. */
typedef struct unit_view_summary_frame {
    SPRT sprites[7];                    /* 0x00: [0] name/job, [1]-[3] numbers, [6] zodiac */
    DR_MODE draw_mode_0;                /* 0x8c */
    DR_MODE draw_mode_1;                /* 0x98 */
    menu_palette_primitives_t* palette; /* 0xa4 */
    DR_OFFSET draw_offset_a;            /* 0xa8 */
    DR_OFFSET draw_offset_b;            /* 0xb4 */
    point16_t offset_a;                 /* 0xc0 */
    point16_t offset_b;                 /* 0xc4 */
    POLY_FT4 portrait;                  /* 0xc8 */
} unit_view_summary_frame_t;

/* 12-byte textured-quad piece (g_unit_view_quad_pieces) that unit_view_build_height_pieces turns into a POLY_GT4:
 * texture origin, size and screen position relative to g_unit_view_quad_origin. Provisional. */
typedef struct menu_quad_piece {
    u8 u; /* 0x00 */
    u8 _unused_01;
    s8 v;           /* 0x02: texture v + 0x78 */
    u8 _padding_03; /* aligns w */
    s16 w;          /* 0x04 */
    s16 h;          /* 0x06 */
    s16 x;          /* 0x08 */
    s16 y;          /* 0x0a */
} menu_quad_piece_t;

/* The four vertex colors unit_view_build_height_pieces applies (g_unit_view_quad_colors); the leading 32-bit value is
 * untyped. Provisional. */
typedef struct menu_quad_colors {
    u32 _unused_00;
    CVECTOR vertex[4]; /* 0x04; one color per POLY_GT4 corner */
} menu_quad_colors_t;

/* One of the two 0x144-byte packet pages of the display (g_unit_view_height_display_pages): five Gouraud textured
 * quads, two sprites and the two draw-mode packets that select their texture windows. Provisional. */
typedef struct menu_quad_page {
    POLY_GT4 quads[5]; /* 0x000 */
    SPRT sprites[2];   /* 0x104 */
    DR_MODE modes[2];  /* 0x12c */
} menu_quad_page_t;
typedef char menu_quad_page_size_must_be_324[sizeof(menu_quad_page_t) == 324 ? 1 : -1];

extern unit_view_stats_page_t g_unit_view_stats_panel_pages[];
extern menu_quad_colors_t g_unit_view_quad_colors;
extern menu_quad_piece_t g_unit_view_quad_pieces[];
extern menu_quad_page_t g_unit_view_height_display_pages[2];

static u8 g_status_screen_line_colors[8 * MENU_STATUS_SCREEN_LINE_SIZE] = { 16, 16, 16, 16, 1, 16, 90, 32, 32, 32, 17,
    1, 17, 90, 32, 32, 32, 34, 1, 34, 90, 16, 16, 16, 35, 1, 35, 90, 16, 16, 16, 122, 1, 122, 90, 32, 32, 32, 123, 1,
    123, 90, 32, 32, 32, 140, 1, 140, 90, 16, 16, 16, 141, 1, 141, 90 };

static u16 g_status_screen_sprite_clut_pairs[19][2]
    = { 0x7cbc, 0x7c7c, 0x7cbc, 0x7c7c, 0x7d7c, 0x7dfc, 0x7d7c, 0x7dfc, 0x7d7c, 0x7dfc, 0x7d7c, 0x7dfc, 0x7d7c, 0x7dfc,
          0x7d7c, 0x7dfc, 0x7d7c, 0x7dfc, 0x7d7c, 0x7dfc, 0x7d7c, 0x7dfc, 0x7d7c, 0x7dfc, 0x7c3c, 0x7d3c, 0x7c3c,
          0x7d3c, 0x7c3c, 0x7d3c, 0x7c3c, 0x7d3c, 0x7c3c, 0x7d3c, 0x7c3c, 0x7d3c, 0x7c3c, 0x7d3c };

static s32 unit_view_display_action_preview(POLY_FT4* unused_primitive, const point16_t* unused_base_load,
    const RECT* unused_base_screen, image_layout_t* unused_parameters);

static s32 unit_view_build_height_pieces(s32 group, POLY_GT4* poly);
static void menu_copy_sprite_position_uv_and_size(const menu_entry_t* entry, SPRT* sprite, s32 unused_frame);

/* Return 1 when the unit has any current status (innate ones included), else 0; one of
 * the conditions under which Circle opens the status list. */
s32 unit_has_any_current_status(s32 battle_id) {
    unit_stats_t* unit = unit_get_stats(battle_id);
    s32 index;

    for (index = 0; index < STATUS_BYTE_COUNT; index++) {
        if (unit->status_sets.current[index] != 0) {
            return 1;
        }
    }
    return 0;
}

/* Builds the two-column status list for the stored unit: every set bit of
 * the current status bytes becomes text id 0x8800 + byte * 8 + bit, then
 * the visible/overflow row counts are derived from the entry count.
 *
 * The flag reads go through local pointers so they are reloaded each
 * iteration as the target does; the columns are s16 so `index + 0x8800`
 * stays a 32-bit addition with the constant hoisted into a register. */
void status_screen_build_status_list(void) {
    unit_stats_t* unit;
    u16 i;
    u16 bit;
    u16 count;
    u16 half;
    u16 odd;
    u32 mask;
    u16 index;
    s16* flag_a;
    s16* flag_b;

    /* A generic monster's or team kind 1 unit's rows get the inert MENU_ROW_ACTION_NONE (Circle
     * only records the row); other units' rows open JOBSTTS silently. */
    for (i = 0; i < 8; i++) {
        flag_a = &g_unit_panel_selected_unit_fields.state.generic_monster;
        flag_b = &g_unit_panel_selected_unit_status.team_kind;
        if (*flag_a != 0 || *flag_b == 1) {
            g_status_screen_status_list_row_actions[i] = MENU_ROW_ACTION_NONE;
        } else {
            g_status_screen_status_list_row_actions[i] = MENU_ROW_ACTION_SILENT | MENU_COMMAND_ID_JOBSTTS;
        }
    }
    unit = unit_get_stats(g_unit_panel_selected_unit.battle_id);
    for (i = 0; i < 20; i++) {
        g_unit_panel_status_left_text_ids[i] = TEXT_ID_ABILITY_NAME_BASE;
        g_unit_panel_status_right_text_ids[i] = TEXT_ID_ABILITY_NAME_BASE;
        g_ability_list_ability_display_flags[i] = 0;
    }
    count = 0;
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        mask = 0x80;
        for (bit = 0; bit < 8; bit++) {
            if (mask & unit->status_sets.current[i]) {
                index = i * 8 + bit;
                if (count & 1) {
                    g_unit_panel_status_right_text_ids[count / 2] = index + 0x8800;
                } else {
                    g_unit_panel_status_left_text_ids[count / 2] = index + 0x8800;
                }
                count++;
            }
            mask >>= 1;
        }
    }
    sound_queue_sfx_confirm();
    if (count == 0) {
        status_screen_jobs_thread();
    }
    odd = count & 1;
    half = count / 2;
    count = half + odd;
    if (count >= 8) {
        g_status_screen_status_list_layout.columns.row_count = 7;
        g_status_screen_status_list_layout.columns.hidden_rows = count - 7;
    } else {
        g_status_screen_status_list_layout.columns.row_count = count;
        g_status_screen_status_list_layout.columns.hidden_rows = 0;
    }
    ability_list_scrolling_thread();
}

/* Copy the selected panel and tile records into four caller-owned buffers. */
void unit_panel_copy_billboard_out(unit_panel_summary_t* banner, unit_panel_unit_t* unit,
    unit_panel_record_t* billboard, battle_map_selected_tile_t* cursor_tile) {
    mem_copy_bytes_nonempty(banner, &g_unit_panel_selected_unit_status, sizeof(g_unit_panel_selected_unit_status));
    mem_copy_bytes_nonempty(unit, &g_unit_panel_selected_unit, sizeof(g_unit_panel_selected_unit));
    mem_copy_bytes_nonempty(billboard, &g_unit_panel_selected_unit_fields, sizeof(g_unit_panel_selected_unit_fields));
    mem_copy_bytes_nonempty(cursor_tile, &g_battle_map_selected_tile_data, sizeof(g_battle_map_selected_tile_data));
}

/* Load the active-turn banner, the active-turn unit record and the billboard data
 * from the caller's copies: the inverse of unit_panel_copy_billboard_out,
 * without the cursor tile record. REQUIRE.OUT publishes its own records this way. */
void unit_panel_copy_billboard_in(
    const unit_panel_summary_t* banner, const unit_panel_unit_t* unit, const unit_panel_record_t* billboard) {
    mem_copy_bytes_nonempty(&g_unit_panel_selected_unit_status, banner, sizeof(g_unit_panel_selected_unit_status));
    mem_copy_bytes_nonempty(&g_unit_panel_selected_unit, unit, sizeof(g_unit_panel_selected_unit));
    mem_copy_bytes_nonempty(&g_unit_panel_selected_unit_fields, billboard, sizeof(g_unit_panel_selected_unit_fields));
}

/* Point g_text_section_pointers at the sections of a text file that starts with a table of
 * TEXT_SECTION_COUNT offsets, each measured from the end of that table. */
void text_relocate_pointer_table(const u32* offsets) {
    s32 index = 0;
    const u8* text_data = (const u8*)offsets + sizeof(u32) * TEXT_SECTION_COUNT;

    do {
        g_text_section_pointers[index] = (u8*)(text_data + offsets[index]);
        index++;
    } while (index < TEXT_SECTION_COUNT);
}

/* Back up the text section pointer table once. */
void text_save_section_pointers(void) {
    if (g_text_section_backup_active == 0) {
        mem_copy_bytes_nonempty(
            g_text_saved_section_pointers, g_text_section_pointers, sizeof(g_text_saved_section_pointers));
        g_text_section_backup_active++;
    }
}

/* Restore the text section pointer table and clear the backup flag. */
void text_restore_section_pointers(void) {
    mem_copy_bytes_nonempty(g_text_section_pointers, g_text_saved_section_pointers, sizeof(g_text_section_pointers));
    g_text_section_backup_active = 0;
}

/* DismissUnit: remove the first roster member after slot 0 with this sprite set from
 * the party, returning its equipment to the inventory. Nothing happens if none matches. */
void script_opcode_7a_dismiss_unit(s32 sprite_set) {
    party_unit_t* party;
    s32 party_index = 1;
    s32 empty_party_id = PARTY_ID_NONE;
    s32 empty_item;
    s32 slot;
    u8 item;

    for (; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party = party_get_unit(party_index);
        if (party->party_id != empty_party_id && party->sprite_set == sprite_set) {
            break;
        }
    }

    if (party_index != PARTY_ROSTER_SLOT_COUNT) {
        slot = 0;
        empty_item = ITEM_ID_NONE;
        for (; slot < UNIT_EQUIPMENT_SLOT_COUNT; slot++) {
            item = party->equipment[slot];
            if (item != ITEM_ID_NOTHING && item != empty_item) {
                g_item_quantities[item]++;
            }
        }
        party_remove_unit(party_index);
    }
}

/* Persist event units marked to join without opening the formation screen. */
void party_join_event_units(void) {
    s32 out_idx;
    s32 i;
    unit_stats_t* unit;
    u8 first;
    u8 flags;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = unit_entd_find_stats(unit_get_stats(i)->unit_id, &out_idx);
        if (out_idx >= 0 && i == out_idx) {
            if (unit->formation_index == UNIT_FORMATION_INDEX_NONE
                || ((first = unit->character_identity) != CHARACTER_IDENTITY_ENTD_NONE
                    && first <= CHARACTER_IDENTITY_RAMZA_CHAPTER_4)) {
                flags = unit->unit_flags;
                if (flags & UNIT_FLAG_JOIN_AFTER_EVENT) {
                    party_save_unit(unit, flags & UNIT_FLAG_SAVE_FORMATION);
                }
            }
        }
    }
}

/* CallFunction 2 and 7: stage a 365-day advance in the date-advance variable. The
 * calendar itself moves when that variable is consumed. */
void calendar_advance_one_year(void) {
    script_set_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE, 365);
}

/* CallFunction 3: raise the Brave of every blue-team unit by 10, capped at UNIT_BRAVE_FAITH_MAX. */
void unit_increase_blue_team_brave_by_10(void) {
    unit_stats_t* unit;
    s32 unit_id;

    for (unit_id = 0; unit_id < UNIT_SLOT_COUNT; unit_id++) {
        unit = unit_get_stats(unit_id);
        if ((unit->team_flags & UNIT_TEAM_MASK) == UNIT_TEAM_BLUE) {
            unit->brave += 10;
            if (unit->brave >= UNIT_BRAVE_FAITH_MAX) {
                unit->brave = UNIT_BRAVE_FAITH_MAX;
            }
        }
    }
}

/* CallFunction 0x0f and 0x10: place the runtime unit of battle id 3 on the tile and
 * facing of battle id 1, or that of battle id 4 on battle id 2, and cancel any
 * rotation in progress. Nothing happens when the warped battle id has no runtime
 * unit.
 *
 * The parameter arrives as the CallFunction selector and is reused for the runtime
 * id. */
void script_warp_unit_display_to_paired_unit(s32 runtime_id) {
    unit_stats_t* unit;
    unit_animation_state_t* state;
    s32 facing;
    s32 battle_id;

    if (runtime_id == EVENT_CALL_FUNCTION_WARP_UNIT_4_DISPLAY_TO_UNIT_2_POSITION_AND_FACING) {
        unit = unit_get_stats(1);
        battle_id = 3;
    } else {
        unit = unit_get_stats(2);
        battle_id = 4;
    }
    runtime_id = unit_get_runtime_id_by_battle_id(battle_id);
    if (runtime_id != -1) {
        g_script_warp_coords.x = unit->x;
        g_script_warp_coords.layer = unit->position.higher_elevation;
        g_script_warp_coords.y = unit->position.y;
        facing = unit->position.facing << 2;
        state = &g_unit_animation_states[runtime_id];
        state->delay = 0;
        state->rotating = 0;
        state->target_facing = facing;
        unit_place_in_new_location(runtime_id, &g_script_warp_coords, unit->position.facing);
        unit_set_facing(runtime_id, unit->position.facing << 2);
    }
}

/* Hide the depth number of the selected-tile panel; BATTLE's effect playback sets it for the
 * length of an ability effect. */
void unit_view_hide_tile_depth(void) {
    g_unit_view_depth_display_suppressed = 1;
}

/* Show the selected-tile panel's depth number again (effect playback end). */
void unit_view_show_tile_depth(void) {
    g_unit_view_depth_display_suppressed = 0;
}

/* Allocate text image VRAM for the sprite or textured quad and give it the normal menu CLUT, GetClut(0x3c0, 0x1f0),
 * plus clut_offset sixteens of the 16-bit CLUT value (4 is one VRAM line down). Callers pass 0 or 2; 2 carries out
 * of the x field: 16-bit CLUT value 0x7c5c is GetClut(0x1c0, 0x1f1). */
void text_configure_sprite_vram(RECT* rect, s32 width, s32 height, void* packet, s32 clut_offset) {
    texture_primitive_t* primitive = packet;

    rect->w = width;
    rect->h = height;
    gfx_alloc_texture_rect(rect, primitive, (u32*)-1);
    primitive->sprt.clut = (clut_offset << 4) + MENU_CLUT_NORMAL;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * text_configure_sprite_vram and unit_view_tile_info_thread.
 * Nothing on the disc calls it or points at it; the name is a neutral
 * placeholder until a reference turns up. */
void menu_noop_1(void) {
}

/* Draw the selected tile height and depth while its panel thread is active. */
void unit_view_tile_info_thread(void) {
    RECT rect;
    u8 buffer[0x100];
    s32 i;
    s32 j;
    s32 frame;
    menu_quad_page_t* page;
    POLY_GT4* quad;

    g_unit_panel_hide_numeric_values = 0;
    page = &g_unit_view_height_display_pages[0];
    gfx_set_draw_mode_for_texture_page(&page->modes[0], MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    gfx_set_draw_mode_for_texture_page(&page->modes[1], MENU_TEXTURE_PAGE_FRAME);
    for (i = 0; i < 5; i++) {
        quad = &page->quads[i];
        SetPolyGT4(quad);
        SetShadeTex(quad, 0);
        SetSemiTrans(quad, 1);
        quad->tpage = GetTPage(0, 0, 0x380, 0x120);
        quad->clut = 0x7CFC;
    }
    menu_init_sprite_array(page->sprites, 2, 0x7CBC);
    page->sprites[1].clut = 0x7D7C;
    for (i = 0; i < 2; i++) {
        gfx_set_image_primitive_layout(&page->sprites[i], &g_menu_texture_origin, &g_unit_view_tile_info_image_location,
            &g_unit_view_tile_info_image_params[i]);
    }
    text_configure_sprite_vram(&rect, 0x18, 0x10, &page->sprites[0], 0);
    mem_copy_bytes_nonempty(
        &g_unit_view_height_display_pages[1], &g_unit_view_height_display_pages[0], sizeof(menu_quad_page_t));

    for (frame = 0;; frame++) {
        page = &g_unit_view_height_display_pages[frame & 1];
        /* The target loads the display height signed (lh). */
        i = unit_view_build_height_pieces(g_battle_map_selected_tile_data.display_height, page->quads) - 1;
        thread_yield();
        if (thread_find_running_by_task(THREAD_TASK_DARK_SCREEN) != 0) {
            continue;
        }
        if (thread_find_running_by_task(THREAD_TASK_DARK_SCREEN_HOLD) != 0) {
            continue;
        }
        g_unit_panel_hide_numeric_values = 0;
        if (thread_get_current_parameter_3() != 0) {
            break;
        }
        for (j = i; j >= 0; j--) {
            gfx_draw_or_append_gpu_primitive(&page->quads[j]);
        }
        if (g_battle_map_selected_tile_data.depth != 0) {
            menu_clear_render_buffer(buffer, 0x100);
            g_menu_window_text_pen.stride = 0x18;
            menu_set_text_origin(0x10, 0);
            menu_draw_large_number_glyphs(g_battle_map_selected_tile_data.depth, 1, buffer, &g_menu_window_text_pen);
            if (g_unit_view_depth_display_suppressed == 0) {
                LoadImage(&rect, buffer);
                page->sprites[0].clut = 0x7CBC;
                gfx_draw_or_append_gpu_primitive(&page->sprites[0]);
                gfx_draw_or_append_gpu_primitive(&page->modes[0]);
                gfx_draw_or_append_gpu_primitive(&page->sprites[1]);
                gfx_draw_or_append_gpu_primitive(&page->modes[1]);
            }
        }
    }
    thread_yield();
    gfx_free_texture_rect(&rect);
    thread_exit_current();
}

/* Lay out the textured pieces of piece group `group` (the tile height readout for one
 * display_height value) as gradient quads at g_unit_view_quad_origin. Returns the number of quads
 * written. */
static s32 unit_view_build_height_pieces(s32 group, POLY_GT4* poly) {
    menu_quad_piece_t* piece;
    s32 count;
    s32 i;
    menu_quad_colors_t* colors = &g_unit_view_quad_colors;

    piece = &g_unit_view_quad_pieces[g_unit_view_quad_group_starts[group]];
    count = g_unit_view_quad_group_starts[group + 1] - g_unit_view_quad_group_starts[group];
    for (i = 0; i < count; i++) {
        setXY4(poly, g_unit_view_quad_origin.x + piece->x, g_unit_view_quad_origin.y + piece->y,
            g_unit_view_quad_origin.x + piece->x + piece->w, g_unit_view_quad_origin.y + piece->y,
            g_unit_view_quad_origin.x + piece->x, g_unit_view_quad_origin.y + piece->y + (piece->h - 1),
            g_unit_view_quad_origin.x + piece->x + piece->w, g_unit_view_quad_origin.y + piece->y + (piece->h - 1));
        setUV4(poly, piece->u, piece->v - 0x78, piece->u + piece->w, piece->v - 0x78, piece->u,
            piece->v - 0x78 + (piece->h - 1), piece->u + piece->w, piece->v - 0x78 + (piece->h - 1));
        setRGB0(poly, colors->vertex[0].r, colors->vertex[0].g, colors->vertex[0].b);
        setRGB1(poly, colors->vertex[1].r, colors->vertex[1].g, colors->vertex[1].b);
        setRGB2(poly, colors->vertex[2].r, colors->vertex[2].g, colors->vertex[2].b);
        setRGB3(poly, colors->vertex[3].r, colors->vertex[3].g, colors->vertex[3].b);
        poly++;
        piece++;
    }
    return count;
}

/*
 * Build one of the two double-buffered menu sprite pages: five draw-mode
 * packets (texture window per sprite) followed by five sprites placed from
 * the parameter table, then submit them all in reverse order.
 */
static void menu_build_sprite_page(s32 page_index, RECT* base_screen) {
    s32 i;
    menu_sprite_page_t* page = &g_menu_scratch_buffer.sprite_pages[page_index & 1];
    SPRT* sprite;

    for (i = 0; i < 5; i++) {
        sprite = &page->sprites[i];
        menu_init_sprt(sprite);
        SetDrawMode(&page->modes[i], 1, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_menu_sprite_page_texture_windows[i]);
        gfx_set_image_primitive_layout(
            sprite, &g_menu_texture_origin, base_screen, &g_menu_sprite_page_image_params[i]);
    }
    for (i = 4; i >= 0; i--) {
        gfx_draw_or_append_gpu_primitive(&page->sprites[i]);
        gfx_draw_or_append_gpu_primitive(&page->modes[i]);
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between menu_build_sprite_page
 * and unit_view_display_action_preview. Nothing on the disc calls it or
 * points at it; the name is a neutral placeholder until a reference turns up. */
void menu_noop_2(void) {
}

/*
 * Loads the action-preview result image (HP/MP/gil/EXP/level/status/stat
 * change) and sets up the value panel for the current preview action.
 *
 * Returns the display image id, or the highest-order status image when a
 * status change accompanies a damage/stat change. The two early exits that
 * hide the panel return 2: the target jumps past the epilogue's `move v0,s1`
 * with the panel state's 2 still in $v0. An explicit `return 2;` adds a second
 * constant load, so they use a bare `return;`. The caller discards the result.
 * WORLD has no action previews: its copy is empty and leaves $v0 unset.
 */
static s32 unit_view_display_action_preview(
    POLY_FT4* prim, const point16_t* base_load, const RECT* base_screen, image_layout_t* params) {
#ifdef FFT_BATTLE
    menu_number_entry_t* panel;
    u16* label_x;
    const action_display_image_rect_t* rect;
    s32 value;
    s32 image;
    s32 extra;
    s32 show_empty;
    s32 width;
    s32 x;
    u32 exp;
    u16* digits_x;
    u16* extra_label_x;
    action_data_t* action;
    u8 target_id;

    /* Panel number formats (menu_draw_large_number_glyphs) are a digit count plus an optional sign
     * flag; kind 2 hides a panel entry. */
    extra = 0;
    show_empty = 0;
    panel = g_menu_projected_value_panel;
    panel[0].x = 0xf;
    g_menu_projected_value_panel[0].kind = 1;
    g_menu_projected_value_panel[0].format = MENU_DECIMAL_SIGN_MINUS | 3;
    g_menu_projected_value_panel[0].y = 0;
    digits_x = &panel[1].x;
    *digits_x = 0x32;
    label_x = &g_menu_attack_caster_quad_image_params[0][2].x_screen_offset;
    image = 0;
    g_menu_projected_value_panel[1].kind = 0;
    *label_x = g_menu_attack_caster_quad_image_params[1][2].x_screen_offset;
    extra_label_x = &g_menu_attack_caster_quad_image_params[0][3].x_screen_offset;
    *extra_label_x = g_menu_attack_caster_quad_image_params[1][3].x_screen_offset;
    /* A skipped preview leaves the action null, and its accuracy is read
     * (from kernel RAM) before the null test. */
    action = g_unit_view_preview_target_action;
    g_unit_panel_selected_unit_status.projected_hit_percent = action->attack_accuracy;
    target_id = g_unit_panel_comparison_unit_data.battle_id;
    if (target_id == 0xff || action == 0) {
        g_menu_projected_value_panel[0].kind = 2;
        g_menu_projected_value_panel[1].kind = 2;
        return;
    }
    if (action->attack_type == 0) {
        g_menu_projected_value_panel[0].kind = 2;
    }

    if (action->attack_type & ACTION_TYPE_HP_DAMAGE) {
        value = action->hp_damage;
        image = ACTION_DISPLAY_IMAGE_HP;
    } else if (action->attack_type & ACTION_TYPE_HP_HEALING) {
        value = action->hp_healing;
        image = ACTION_DISPLAY_IMAGE_HP;
        g_menu_projected_value_panel[0].format = MENU_DECIMAL_SIGN_PLUS | 3;
    } else if (action->attack_type & ACTION_TYPE_MP_DAMAGE) {
        value = action->mp_damage;
        image = ACTION_DISPLAY_IMAGE_MP;
    } else if (action->attack_type & ACTION_TYPE_MP_HEALING) {
        image = ACTION_DISPLAY_IMAGE_MP;
        value = action->mp_healing;
        g_menu_projected_value_panel[0].format = MENU_DECIMAL_SIGN_PLUS | 3;
    } else if (action->gil_change != 0 && (action->attack_type & ACTION_TYPE_PSEUDO_STATUS)) {
        value = action->gil_change;
        image = ACTION_DISPLAY_IMAGE_GIL;
        if (value >= 0) {
            g_menu_projected_value_panel[0].format = MENU_DECIMAL_SIGN_PLUS | 4;
        } else {
            value = -value;
            g_menu_projected_value_panel[0].format = MENU_DECIMAL_SIGN_MINUS | 4;
        }
        panel[0].kind = 0;
        *label_x -= 4;
        panel[0].x -= 10;
        panel[0].y += 4;
        *digits_x += 2;
    } else if ((exp = g_unit_view_preview_target_action->exp_change) != 0
        && (g_unit_view_preview_target_action->attack_type & ACTION_TYPE_PSEUDO_STATUS)) {
        s32 number_x;

        image = ACTION_DISPLAY_IMAGE_EXP;
        value = exp;
        /* Retail reverses the signs of gil's preview: an EXP loss (Steal Exp.'s target) shows the
         * plus sign and a gain the minus sign. */
        if (exp & ACTION_EXP_CHANGE_LOSS) {
            value &= ACTION_EXP_CHANGE_VALUE_MASK;
            panel[0].format = MENU_DECIMAL_SIGN_PLUS | 3;
        } else {
            panel[0].format = MENU_DECIMAL_SIGN_MINUS | 3;
        }
        number_x = panel[0].x;
        number_x -= 8;
        panel[0].x = number_x;
        *label_x -= 8;
    } else if ((g_unit_view_preview_target_action->special_effect
                   & (ACTION_SPECIAL_EFFECT_LEVEL_UP | ACTION_SPECIAL_EFFECT_LEVEL_DOWN))
        && (g_unit_view_preview_target_action->attack_type & ACTION_TYPE_PSEUDO_STATUS)) {
        image = ACTION_DISPLAY_IMAGE_LEVEL;
        value = 1;
        if (g_unit_view_preview_target_action->special_effect & ACTION_SPECIAL_EFFECT_LEVEL_UP) {
            panel[0].format = MENU_DECIMAL_SIGN_PLUS | 1;
        } else {
            panel[0].format = MENU_DECIMAL_SIGN_MINUS | 1;
        }
        panel[0].x += 12;
    } else if ((g_unit_view_preview_target_action->attack_type & ACTION_TYPE_PSEUDO_STATUS)
        && g_unit_view_preview_target_action->ct_change == ACTION_CT_CHANGE_QUICK) {
        /* The three shifted-label tails are duplicated in the original and
         * merged by cross-jumping; a shared goto tail allocates differently. */
        image = ACTION_DISPLAY_IMAGE_QUICK;
        value = 0;
        panel[0].kind = 2;
        *label_x = *label_x - g_result_image_rects[ACTION_DISPLAY_IMAGE_QUICK].width + 8;
    } else if ((g_unit_view_preview_target_action->attack_type & ACTION_TYPE_PSEUDO_STATUS)
        && (g_unit_view_preview_target_action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM)) {
        image = ACTION_DISPLAY_IMAGE_STOLEN;
        value = 0;
        panel[0].kind = 2;
        *label_x = *label_x - g_result_image_rects[ACTION_DISPLAY_IMAGE_STOLEN].width + 8;
    } else if ((g_unit_view_preview_target_action->attack_type & ACTION_TYPE_PSEUDO_STATUS)
        && (g_unit_view_preview_target_action->special_effect & ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT)) {
        image = ACTION_DISPLAY_IMAGE_BROKEN;
        value = 0;
        panel[0].kind = 2;
        *label_x = *label_x - g_result_image_rects[ACTION_DISPLAY_IMAGE_BROKEN].width + 8;
    } else if (g_unit_view_preview_target_action->attack_type & ACTION_TYPE_STATUS_CHANGE) {
        g_thread_call_target = (void (*)(void))status_find_action_highest_order_effect;
        image = thread_call_on_main_stack(g_unit_view_preview_target_action);
        if (image > 0) {
            if (image & STATUS_ACTION_RESULT_REMOVAL) {
                extra = 0x1f;
            } else if ((image & (STATUS_ACTION_RESULT_REMOVAL | STATUS_ACTION_RESULT_SCRIPT_FILTER_EXCLUDED))
                == (STATUS_ACTION_RESULT_REMOVAL | STATUS_ACTION_RESULT_SCRIPT_FILTER_EXCLUDED)) {
                /* Unreachable: the preceding removal test also catches these flags. */
                extra = 0x1e;
            }
            image &= STATUS_ACTION_RESULT_INDEX_MASK;
            image = g_status_display_image_ids[image - 1];
            /* Duplicated store: it keeps the state write in its own block
             * ahead of the label arithmetic, as in the target. */
            if (image == 0) {
                show_empty = 1;
                panel[0].kind = 2;
            } else {
                panel[0].kind = 2;
            }
            width = g_result_image_rects[image].width;
            x = *label_x - width;
            value = 0;
            *label_x = x + 8;
            if (extra != 0) {
                *extra_label_x = x - 0x12;
            }
        } else {
            panel[0].kind = 2;
        }
    } else if (g_unit_view_preview_target_action->attack_type & ACTION_TYPE_PSEUDO_STATUS) {
        image = ACTION_DISPLAY_IMAGE_NONE;
        if (g_unit_view_preview_target_action->sp_change != 0) {
            image = ACTION_DISPLAY_IMAGE_SPEED;
            value = g_unit_view_preview_target_action->sp_change;
            *label_x -= 0x26;
        } else if (g_unit_view_preview_target_action->ct_change != 0) {
            image = ACTION_DISPLAY_IMAGE_CT;
            value = g_unit_view_preview_target_action->ct_change;
            *label_x -= 0x1e;
        } else if (g_unit_view_preview_target_action->pa_change != 0) {
            image = ACTION_DISPLAY_IMAGE_PHYSICAL_ATTACK;
            value = g_unit_view_preview_target_action->pa_change;
            *label_x -= 0x14;
        } else if (g_unit_view_preview_target_action->ma_change != 0) {
            image = ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK;
            value = g_unit_view_preview_target_action->ma_change;
            *label_x -= 0x14;
        } else if (g_unit_view_preview_target_action->brave_change != 0) {
            image = ACTION_DISPLAY_IMAGE_BRAVE;
            value = g_unit_view_preview_target_action->brave_change;
            *label_x -= 0x24;
        } else if (g_unit_view_preview_target_action->faith_change != 0) {
            image = ACTION_DISPLAY_IMAGE_FAITH_STAT;
            value = g_unit_view_preview_target_action->faith_change;
            *label_x -= 0x24;
        }
        if (image >= ACTION_DISPLAY_IMAGE_CT && image <= ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK) {
            panel[0].x += 0x10;
            if (value & ACTION_STAT_CHANGE_INCREASE) {
                panel[0].format = MENU_DECIMAL_SIGN_PLUS | 2;
                if (image == ACTION_DISPLAY_IMAGE_CT && value == 0xff) {
                    value = 0x64;
                    panel[0].format = 3;
                    *label_x = *label_x; /* reloads and rewrites x, as the target does */
                    panel[0].x -= 8;
                }
            } else {
                panel[0].format = MENU_DECIMAL_SIGN_MINUS | 2;
                if (image == ACTION_DISPLAY_IMAGE_CT && value == 0x7f) {
                    value = 0;
                    panel[0].format = 2;
                    *label_x += 8;
                }
            }
            value &= ACTION_STAT_CHANGE_VALUE_MASK;
        } else {
            panel[0].kind = 2;
            return;
        }
    }

    g_unit_panel_selected_unit_status.projected_display_value = value;
    if (image != ACTION_DISPLAY_IMAGE_NONE && (image != 0 || show_empty != 0)) {
        rect = &g_result_image_rects[image];
        params->source.x = rect->u;
        params->source.y = rect->v;
        params->source.w = rect->width;
        params->source.h = rect->height;
        gfx_set_image_primitive_layout(prim, base_load, base_screen, params);
    }
    prim->clut = 0x7d7c;
    if (image >= ACTION_DISPLAY_IMAGE_HP && image <= ACTION_DISPLAY_IMAGE_EXP) {
        prim->clut = 0x7cbc;
    }
    if (image == ACTION_DISPLAY_IMAGE_PHYSICAL_ATTACK || image == ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK) {
        prim->clut = 0x7d7c;
    }

    if (extra != 0) {
        prim++;
        params++;
        prim->clut = 0x7d7c;
        rect = &g_result_image_rects[extra];
        params->source.x = rect->u;
        params->source.y = rect->v;
        params->source.w = rect->width;
        params->source.h = rect->height;
        gfx_set_image_primitive_layout(prim, base_load, base_screen, params);
        return image;
    }

    if ((g_unit_view_preview_target_action->attack_type & ACTION_TYPE_STATUS_CHANGE)
        && g_unit_view_preview_target_action->attack_type != ACTION_TYPE_STATUS_CHANGE) {
        prim++;
        params++;
        g_thread_call_target = (void (*)(void))status_find_action_highest_order_effect;
        image = thread_call_on_main_stack(g_unit_view_preview_target_action);
        if (image > 0) {
            if (image & STATUS_ACTION_RESULT_REMOVAL) {
                setRGB0(prim, 0x30, 0x8c, 0xdc);
            } else {
                setRGB0(prim, 0xbe, 0x28, 0xa);
            }
            image &= STATUS_ACTION_RESULT_INDEX_MASK;
            image = g_status_display_image_ids[image - 1];
            if (image != ACTION_DISPLAY_IMAGE_NONE && image != ACTION_DISPLAY_IMAGE_INVITE) {
                rect = &g_result_image_rects[image];
                params->source.x = rect->u;
                params->source.y = rect->v;
                params->source.w = rect->width;
                params->source.h = rect->height;
                gfx_set_image_primitive_layout(prim, base_load, base_screen, params);
                value = prim->x1 - prim->x0;
                if (value >= 0x18) {
                    prim->x0 += 0x18 - value;
                    prim->x1 += 0x18 - value;
                    prim->x2 += 0x18 - value;
                    prim->x3 += 0x18 - value;
                    prim->y0 -= 9;
                    prim->y1 -= 9;
                    prim->y2 -= 9;
                    prim->y3 -= 9;
                }
                prim->clut = 0x7d7c;
            }
        }
    }
    return image;
#endif
}

/* Thread body of a unit stats panel: `mode` selects the hovered unit, the right-hand unit, or the
 * caster or target of an attack preview. The panel slides in, redraws each frame and slides out on
 * the stop request; CT shows at most 100, an egg's numbers are hidden and the target panel also
 * names its reaction ability. */
static void unit_view_stats_panel_thread(
    RECT* frame_rect, s32 mode, struct menu_number_entry* entries, unit_panel_summary_t* status) {
    RECT rects[4];
    menu_unit_status_offsets_t offsets[2];
    u16 extent_w;
    u16 extent_h;
    u8* buffer;
    u8* text;
    s32 text_loaded;
    s32 running;
    s32 unit_id;
    s32 slide;
    s32 speed;
    s32 text_id;
    unit_stats_t* unit;
    unit_view_stats_page_t* page;
    image_layout_t* params;
    menu_unit_status_offsets_t* offs;
    POLY_G4* poly;
    menu_unit_status_gauge_t* gauge;
    CVECTOR* color;
    glyph_blit_t* origin;
    s32* stride;
    s32 frame;
    s32 i;
    s32 hide_values;
    s32 base_y;
    s32 row_y;
    u16 width;

    unit_id = status->unit_index;
    speed = 0x16;
    unit = unit_get_stats(unit_id);
    slide = 0x84;
    g_unit_panel_hide_numeric_values = unit->unit_flags & UNIT_FLAG_EGG;
    if (thread_get_current_task_id() == THREAD_TASK_HOVERED_UNIT_STATS) {
        g_unit_view_hovered_stats_frame_count++;
    }
    if (mode == UNIT_STATS_PANEL_ATTACK_TARGET) {
        text_id = unit->abilities.reaction;
        if (text_id >= ABILITY_ID_COUNT) {
            text_id = 0;
        }
        if (text_id != 0) {
            text = text_find_entry(text_id + TEXT_ID_ABILITY_NAME_BASE);
            g_thread_call_target = (void (*)(void))text_measure_pixels;
            thread_call_on_main_stack(&extent_w, &extent_h, text);
            if (extent_w & 3) {
                extent_w = (extent_w & 0xFFFC) + 4;
            }
            extent_w = 0x4C - extent_w;
            if (extent_w > 0x100) {
                extent_w = 0;
            }
            g_unit_view_target_quad_image_params[1].x_screen_offset
                = g_unit_view_target_quad_default_image_params[1].x_screen_offset + extent_w;
            g_unit_view_target_quad_image_params[2].x_screen_offset
                = g_unit_view_target_quad_default_image_params[2].x_screen_offset + extent_w;
            for (frame = 0; frame < 3; frame++) {
                g_menu_sprite_page_image_params[frame].x_screen_offset
                    = g_menu_sprite_page_image_param_bases[frame].x_screen_offset + extent_w;
                g_menu_sprite_page_image_params[frame].source.w
                    = g_menu_sprite_page_image_param_bases[frame].source.w - extent_w;
            }
            g_menu_sprite_page_image_params[3].x_screen_offset
                = g_menu_sprite_page_image_param_bases[3].x_screen_offset + extent_w;
        }
    }
    page = &g_unit_view_stats_panel_pages[g_unit_view_stats_first_page_by_mode[mode]];
    status_screen_init_numbers_frame(frame_rect, &page->palette);
    gfx_set_draw_mode_for_texture_page(&page->draw_mode_1, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    gfx_set_draw_mode_for_texture_page(&page->draw_mode_0, MENU_TEXTURE_PAGE_FRAME);
    menu_init_sprite_array(page->sprites, 7, 0x7CBC);
    for (frame = 0; frame < 5; frame++) {
        menu_init_default_poly_ft4(&page->quads[frame]);
    }
    menu_init_default_poly_ft4(&page->portrait);
    text_configure_sprite_vram(&rects[0], 0x28, 0x28, &page->quads[0], 2);
    text_configure_sprite_vram(&rects[1], 0x60, 0x10, &page->quads[1], 2);
    params = g_unit_view_stats_quad_image_params_by_mode[mode];
    if (mode == UNIT_STATS_PANEL_ATTACK_CASTER) {
        gfx_set_image_primitive_layout(&page->quads[4], &g_menu_texture_origin, frame_rect, &params[4]);
        for (frame = 0; frame < 2; frame++, params++) {
            gfx_set_image_primitive_layout(&page->quads[frame], &g_menu_texture_origin, frame_rect, params);
            page->quads[frame].clut = 0x7CBC;
        }
        unit_view_display_action_preview(&page->quads[frame], &g_menu_texture_origin, frame_rect, params);
    } else if (mode == UNIT_STATS_PANEL_ATTACK_TARGET) {
        for (frame = 0; frame < 2; frame++, params++) {
            gfx_set_image_primitive_layout(&page->quads[frame], &g_menu_texture_origin, frame_rect, params);
        }
        if (text_id != 0) {
            gfx_set_image_primitive_layout(&page->quads[frame], &g_menu_texture_origin, frame_rect, params);
        }
    } else {
        for (frame = 0; frame < 4; frame++, params++) {
            gfx_set_image_primitive_layout(&page->quads[frame], &g_menu_texture_origin, frame_rect, params);
        }
    }
    mem_copy_bytes_nonempty(&page[1], page, sizeof(unit_view_stats_page_t));
    text_loaded = 0;
    /* `slide` is the panel's x shift from its resting place: it starts 0x84 out and eases
     * in, `speed` dropping by 2 per step; a stop request slides it back out. */
    for (frame = 0;; frame++) {
        if (status->gauges[MENU_UNIT_STATUS_GAUGE_CT].value > UNIT_TURN_CT_THRESHOLD) {
            status->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;
        }
        if (text_loaded != 0) {
            menu_free_memory(buffer);
            text_loaded = 0;
        }
        for (i = 0; i < g_script_event_speed; i++) {
            if (thread_get_current_parameter_3() != 0) {
                if (mode == UNIT_STATS_PANEL_HOVERED_UNIT
                    && g_thread_contexts[THREAD_SLOT_RIGHT_UNIT_STATS].task_id == THREAD_TASK_ATTACK_CASTER_STATS) {
                    /* Both exits leave the step loop and the frame loop. */
                    goto exit;
                }
                slide += 0x1E;
                if (slide > 0x80) {
                    goto exit;
                }
            } else {
                if (slide != 0) {
                    slide -= speed;
                    if (slide < 0) {
                        slide = 0;
                    }
                }
                if (speed >= 3) {
                    speed -= 2;
                }
            }
        }
        running = thread_find_running_by_task(THREAD_TASK_STATUS_SCREEN);
        if (running != 0 && thread_is_running_asm(running - 1) == 0) {
            running = 0;
        }
        page = &g_unit_view_stats_panel_pages[g_unit_view_stats_first_page_by_mode[mode] + (frame & 1)];
        portrait_build_unit_poly(&page->portrait, unit_id);
        if (running != 0) {
            setRGB0(&page->portrait, 0x40, 0x50, 0x70);
        } else {
            setRGB0(&page->portrait, 0x80, 0x80, 0x80);
        }
        status->team_kind = 0;
        if (unit->initial_team_flags & UNIT_TEAM_MASK) {
            status->team_kind = 1;
        }
        if (!(unit->initial_team_flags & UNIT_TEAM_OR_PLAYER_CONTROL_MASK)) {
            status->team_kind = 2;
        }
        if (unit->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
            status->team_kind = 3;
        }
        params = g_unit_view_stats_sprite_image_params_by_mode[mode];
        if (g_unit_view_stats_bounce_step == 0) {
            setXY4(&page->portrait, frame_rect->x + 2, frame_rect->y + 2, frame_rect->x + 0x21, frame_rect->y + 2,
                frame_rect->x + 2, frame_rect->y + 0x32, frame_rect->x + 0x21, frame_rect->y + 0x32);
            mem_copy_bytes_nonempty(
                &params[6], &g_unit_view_stats_icon_image_params[status->team_kind], sizeof(*params));
            for (i = 0; i < 7; i++, params++) {
                gfx_set_image_primitive_layout(&page->sprites[i], &g_menu_texture_origin, frame_rect, params);
            }
        }
        if (mode != UNIT_STATS_PANEL_ATTACK_CASTER) {
            for (i = 0; i < 4; i++) {
                page->quads[i].clut = g_unit_view_stats_quad_clut_pairs[i][running & 1];
            }
        }
        poly = page->bars;
        if (mode == UNIT_STATS_PANEL_ATTACK_TARGET) {
            page->quads[1].clut = MENU_CLUT_NORMAL;
            page->quads[2].clut = 0x7D7C;
        }
        color = g_unit_view_stats_gauge_colors;
        gauge = status->gauges;
        for (i = 0; i < 3; i++) {
            SetPolyG4(poly);
            extent_h = 0xC2;
            extent_w = frame_rect->x + 0x2F;
            if (mode == UNIT_STATS_PANEL_ATTACK_TARGET || mode == UNIT_STATS_PANEL_RIGHT_UNIT) {
                extent_w -= 0x6E;
                extent_h -= 0x10;
            }
            width = (gauge->value << 5) / gauge->max;
            if (width == 0) {
                extent_w = 0;
            }
            hide_values = g_unit_panel_hide_numeric_values;
            if (hide_values != 0) {
                width = 0x20;
            }
            row_y = extent_h + i * 0xB;
            setXY4(poly, extent_w, row_y, extent_w + width, row_y, extent_w, row_y + 3, extent_w + width, row_y + 3);
            width = (gauge->bonus << 5) / gauge->max;
            if (width == 0) {
                extent_w = 0;
            }
            poly->x2 -= 3;
            poly->x3 -= 3;
            if (hide_values != 0) {
                setRGB0(poly, 0x40, 0x40, 0x40);
                setRGB1(poly, 0x80, 0x80, 0x80);
                setRGB2(poly, 0x40, 0x40, 0x40);
                setRGB3(poly, 0xA0, 0xC4, 0xC4);
            } else if (running != 0) {
                setRGB0(poly, color->r >> 1, color->g >> 1, color->b);
                color++;
                setRGB1(poly, color->r >> 1, color->g >> 1, color->b);
                color++;
                setRGB2(poly, color->r >> 1, color->g >> 1, color->b);
                color++;
                setRGB3(poly, color->r >> 1, color->g >> 1, color->b);
                color++;
            } else {
                setRGB0(poly, color->r, color->g, color->b);
                color++;
                setRGB1(poly, color->r, color->g, color->b);
                color++;
                setRGB2(poly, color->r, color->g, color->b);
                color++;
                setRGB3(poly, color->r, color->g, color->b);
                color++;
            }
            poly++;
            gauge++;
        }
        if (frame == 0) {
            buffer = menu_alloc_buffer(0x700);
            menu_clear_render_buffer(buffer, 0x700);
            stride = &g_menu_window_text_pen.stride;
            origin = &g_menu_window_text_pen;
            *stride = 0x28;
            menu_draw_numeric_display_entries(buffer, entries, origin, 6);
            LoadImage(&rects[0], (u32*)buffer);
            i = g_unit_panel_hide_numeric_values;
            *stride = 0x60;
            g_unit_panel_hide_numeric_values = 0;
            if (mode == UNIT_STATS_PANEL_HOVERED_UNIT || mode == UNIT_STATS_PANEL_RIGHT_UNIT) {
                menu_draw_numeric_display_entries(buffer + 0x400, entries + 6, origin, 2);
                g_unit_panel_hide_numeric_values = i;
                menu_draw_numeric_display_entries(buffer + 0x400, entries + 8, origin, 2);
            } else if (mode == UNIT_STATS_PANEL_ATTACK_CASTER) {
                menu_draw_numeric_display_entries(buffer + 0x400, entries + 6, origin, 2);
                g_unit_panel_hide_numeric_values = i;
            } else if (text_id != 0) {
                menu_set_text_origin(2, 0);
                menu_display_text_string(buffer + 0x400, origin, text);
            }
            LoadImage(&rects[1], (u32*)(buffer + 0x400));
            text_loaded = 1;
        }
        offs = &offsets[frame & 1];
        base_y = (g_gfx_screen_polarity & 1) == 0 ? 0xF0 : 0;
        if (mode == UNIT_STATS_PANEL_ATTACK_TARGET || mode == UNIT_STATS_PANEL_RIGHT_UNIT) {
            offs->a.position.x = slide - 0x80;
        } else {
            offs->a.position.x = -0x80 - slide;
        }
        if (g_unit_view_stats_bounce_step != 0) {
            offs->a.position.y = base_y + g_unit_view_stats_bounce_offsets[g_unit_view_stats_bounce_step];
        } else {
            offs->a.position.y = base_y;
        }
        SetDrawOffset(&offs->a.packet, (u16*)&offs->a.position);
        offs->b.position.x = -0x80;
        offs->b.position.y = base_y;
        SetDrawOffset(&offs->b.packet, (u16*)&offs->b.position);
        gfx_draw_or_append_gpu_primitive(&offs->b);
        gfx_draw_or_append_gpu_primitive(&page->quads[0]);
        gfx_draw_or_append_gpu_primitive(&page->quads[1]);
        gfx_draw_or_append_gpu_primitive(&page->draw_mode_1);
        poly--;
        for (i = 0; i < 3; i++) {
            gfx_draw_or_append_gpu_primitive(poly);
            poly--;
        }
        gfx_draw_or_append_gpu_primitive(&page->quads[2]);
        if (mode == UNIT_STATS_PANEL_ATTACK_CASTER) {
            page->quads[4].clut = 0x7D7C;
            gfx_draw_or_append_gpu_primitive(&page->quads[4]);
        }
        if (mode != UNIT_STATS_PANEL_ATTACK_TARGET) {
            gfx_draw_or_append_gpu_primitive(&page->quads[3]);
        } else if (text_id != 0) {
            menu_build_sprite_page(frame, frame_rect);
        }
        for (i = 0; i < 7; i++) {
            page->sprites[i].clut = g_unit_view_stats_sprite_clut_pairs[i][running & 1];
            gfx_draw_or_append_gpu_primitive(&page->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&page->draw_mode_0);
        gfx_draw_or_append_gpu_primitive(&page->portrait);
        if (running != 0) {
            menu_init_primitive_colors_dimmed(&page->palette);
        } else {
            menu_init_primitive_colors_normal(&page->palette);
        }
        status_screen_submit_numbers_frame(&page->palette);
        gfx_draw_or_append_gpu_primitive(&offs->a);
        thread_yield();
        g_unit_panel_hide_numeric_values = unit->unit_flags & UNIT_FLAG_EGG;
        if (thread_get_current_task_id() == THREAD_TASK_HOVERED_UNIT_STATS) {
            g_unit_view_hovered_stats_frame_count++;
        }
    }
exit:
    thread_yield();
    if (thread_get_current_task_id() == THREAD_TASK_HOVERED_UNIT_STATS) {
        g_unit_view_hovered_stats_frame_count = 0;
    }
    gfx_free_texture_rect(&rects[0]);
    gfx_free_texture_rect(&rects[1]);
    thread_exit_current();
}

/* Reload the unit and redraw its status panel; a negative list index draws dashes. */
void unit_view_refresh_hovered_stats(void) {
    unit_panel_load_selected();
    g_unit_view_hovered_stats_setup[6].format = 2;
    if (g_unit_panel_selected_unit_status.list_index < 0) {
        g_unit_view_hovered_stats_setup[6].format = 0x0c00;
    }
    unit_view_stats_panel_thread(&g_unit_view_hovered_stats_rect, UNIT_STATS_PANEL_HOVERED_UNIT,
        g_unit_view_hovered_stats_setup, &g_unit_panel_selected_unit_status);
}

/* Reload the unit and redraw its status panel; a negative list index draws dashes. */
void unit_view_refresh_right_stats(void) {
    unit_panel_load_selected();
    g_unit_view_right_stats_setup[6].format = 2;
    if (g_unit_panel_comparison_unit_status.list_index < 0) {
        g_unit_view_right_stats_setup[6].format = 0x0c00;
    }
    unit_view_stats_panel_thread(&g_unit_view_right_stats_rect, UNIT_STATS_PANEL_RIGHT_UNIT,
        g_unit_view_right_stats_setup, &g_unit_panel_comparison_unit_status);
}

/* Preview the pending action, then display its caster panel. */
void unit_view_preview_caster_stats(void) {
    unit_stats_t* attacker = menu_get_selected_unit_stats();
    unit_stats_t* target = unit_get_stats(g_unit_panel_comparison_unit_data.battle_id);
    action_command_t* action;
    ability_data_t* ability_data;
    ability_secondary_pointer_t secondary;
    s16* selected;

    /* One address register serves both reads, as in the target. */
    selected = &g_unit_panel_selected_unit.battle_id;
    action = unit_get_runtime_action_command_by_battle_id(*selected);
    action->actor_id = *selected;
    if (action->ability_id < ABILITY_ID_ITEM_FIRST) {
        ability_get_data_and_type(action->ability_id & ABILITY_ID_MASK, &ability_data, &secondary);
    }
    /* Item IDs leave secondary unset before this range-flags read. */
    if (((secondary.range->flags_3 & ABILITY_SECONDARY_FLAG_3_PERSEVERE) && action->ability_id < ABILITY_ID_ITEM_FIRST)
        || action->skillset_id == SKILLSET_ID_EQUIP_CHANGE) {
        g_unit_view_preview_target_action = 0;
    } else {
        g_thread_call_target = (void (*)(void))action_calculate_projected_effect;
        thread_call_on_main_stack(attacker, target, action);
        g_unit_view_preview_target_action = &target->action;
    }
    thread_yield();
    thread_yield();
    unit_panel_load_selected();
    unit_view_stats_panel_thread(&g_unit_view_attack_caster_stats_rect, UNIT_STATS_PANEL_ATTACK_CASTER,
        g_unit_view_attack_caster_stats_setup, &g_unit_panel_selected_unit_status);
}

/* Load the preview target, restore the active unit, then display its panel. */
void unit_view_preview_target_stats(void) {
    s32 saved;

    if (g_thread_contexts[THREAD_SLOT_RIGHT_UNIT_STATS].task_id != THREAD_TASK_ATTACK_CASTER_STATS) {
        thread_wait_until_inactive(THREAD_SLOT_RIGHT_UNIT_STATS);
    }
    thread_yield();
    thread_yield();
    saved = g_unit_panel_selected_unit.battle_id;
    /* The target loads the preview unit id unsigned (lhu). */
    g_unit_panel_selected_unit.battle_id = g_unit_panel_comparison_unit_data.battle_id;
    unit_panel_load_selected();
    g_unit_panel_selected_unit.battle_id = saved;
    g_unit_panel_comparison_unit_fields.state.generic_monster = g_unit_panel_selected_unit_fields.state.generic_monster;
    g_unit_panel_comparison_unit_fields.state.ability_ids[2] = g_unit_panel_selected_unit_fields.state.ability_ids[2];
    unit_panel_load_selected();
    unit_view_stats_panel_thread(&g_unit_view_attack_target_stats_rect, UNIT_STATS_PANEL_ATTACK_TARGET,
        g_unit_view_attack_target_stats_setup, &g_unit_panel_comparison_unit_status);
}

/* Draw the unit summary and slide it out when the third thread parameter is set. */
void unit_view_summary_thread(void) {
    RECT rects[4];
    unit_view_summary_frame_t frames[2];
    unit_view_summary_frame_t* frame_record;
    unit_stats_t* unit;
    u8* buffer;
    menu_unit_number_images_t* digits;
    glyph_blit_t* origin;
    s32* stride;
    SPRT* sprite;
    s32 speed;
    s32 slide;
    s32 frame;
    s32 i;
    s32 value;
    s16 zodiac;

    /* `slide` is the panel's x shift from its resting place: it starts 0x84 out and eases
     * in, `speed` dropping by 2 per step; a stop request slides it back out. */
    speed = 0x16;
    slide = 0x84;
    digits = &g_unit_view_summary_digit_images;
    unit_panel_load_selected();
    unit = menu_get_selected_unit_stats();
    g_unit_panel_hide_numeric_values = unit->unit_flags & UNIT_FLAG_EGG;

    sprite = frames[0].sprites;
    gfx_set_draw_mode_for_texture_page(&frames[0].draw_mode_0, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&frames[0].draw_mode_1, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    menu_init_sprite_array(sprite, 7, MENU_CLUT_NORMAL);
    menu_init_default_poly_ft4(&frames[0].portrait);
    text_configure_sprite_vram(&rects[0], 0x58, 0x20, &frames[0].sprites[0], 0);
    text_configure_sprite_vram(&rects[1], 0x10, 0xA, &frames[0].sprites[1], 0);
    text_configure_sprite_vram(&rects[2], 0x10, 0xA, &frames[0].sprites[2], 0);
    text_configure_sprite_vram(&rects[3], 0x10, 0xA, &frames[0].sprites[3], 0);
    for (frame = 0; frame < 7; frame++) {
        gfx_set_image_primitive_layout(&frames[0].sprites[frame], &g_menu_texture_origin,
            &g_unit_view_summary_panel_rect, &g_unit_view_summary_sprite_image_params[frame]);
    }
    gfx_set_image_primitive_layout(&frames[0].portrait, &g_menu_texture_origin, &g_unit_view_summary_panel_rect,
        &g_unit_view_summary_portrait_image_params);
    if (unit->initial_team_flags & UNIT_TEAM_MASK) {
        frames[0].portrait.clut = 0x7FFD;
    } else {
        frames[0].portrait.clut = 0x7FBD;
    }
    frames[0].portrait.tpage = GetTPage(0, 1, 0x3C0, 0x100);
    SetSemiTrans(&frames[0].portrait, 1);

    buffer = menu_alloc_buffer(0x580);
    menu_clear_render_buffer(buffer, 0x580);
    stride = &g_menu_window_text_pen.stride;
    origin = &g_menu_window_text_pen;
    *stride = 0x58;
    menu_set_text_origin(0, 0);
    menu_display_text_string(buffer, origin, g_unit_panel_billboard_unit_name);
    menu_set_text_origin(0, 0x10);
    menu_display_text_string(buffer, origin, g_unit_panel_billboard_job_name);
    LoadImage(&rects[0], (u32*)buffer);

    menu_clear_render_buffer(digits, sizeof(*digits));
    *stride = 0x10;
    menu_set_text_origin(0, 0);
    text_draw_small_number_glyphs(g_unit_panel_selected_unit.brave, 0x202, digits->brave, origin);
    LoadImage(&rects[1], (u32*)digits->brave);
    menu_set_text_origin(0, 0);
    text_draw_small_number_glyphs(g_unit_panel_selected_unit.faith, 0x202, digits->faith, origin);
    LoadImage(&rects[2], (u32*)digits->faith);
    menu_set_text_origin(0, 0);
    if (g_debug_display_enabled != 0) {
        text_draw_small_number_glyphs(g_unit_panel_selected_unit.battle_id, 0x4002, digits->roster_slot, origin);
    } else if (g_unit_panel_selected_unit.battle_id >= 0x10) {
        value = unit->formation_index;
        if (value == 0xFF) {
            value = 0;
        }
        text_draw_small_number_glyphs(value + 1, 0x4002, digits->roster_slot, origin);
    }
    LoadImage(&rects[3], (u32*)digits->roster_slot);

    frames[0].sprites[6].clut = MENU_CLUT_NORMAL;
    frames[0].sprites[6].w = 0x18;
    frames[0].sprites[6].h = 0x14;
    /* Zodiac icons are 24 x 20 cells, seven to a row from v = 0x2A. */
    zodiac = g_unit_panel_selected_unit.zodiac;
    frames[0].sprites[6].u0 = zodiac % 7 * 0x18;
    frames[0].sprites[6].v0 = zodiac / 7 * 0x14 + 0x2A;

    mem_copy_bytes_nonempty(&frames[1], &frames[0], sizeof(unit_view_summary_frame_t));
    frames[0].palette = &g_unit_view_summary_palettes[0].primitives;
    frames[1].palette = &g_unit_view_summary_palettes[1].primitives;
    status_screen_init_numbers_frame(&g_unit_view_summary_panel_rect, frames[0].palette);
    status_screen_init_numbers_frame(&g_unit_view_summary_panel_rect, frames[1].palette);

    for (frame = 0;; frame++) {
        thread_yield();
        g_unit_panel_hide_numeric_values = menu_get_selected_unit_stats()->unit_flags & UNIT_FLAG_EGG;
        frame_record = &frames[frame & 1];
        if (frame == 0) {
            /* The name and job upload has had a frame to finish. */
            menu_free_memory(buffer);
        }

        /* y of the frame buffer drawn this frame. */
        value = (g_gfx_screen_polarity & 1) == 0 ? 0xF0 : 0;
        /* One slide step per event-speed step, so fast-forward speeds the slide. */
        for (i = 0; i < g_script_event_speed; i++) {
            if (thread_get_current_parameter_3() != 0) {
                slide += 0x1E;
                if (slide > 0x80) {
                    /* Leaves both the step loop and the frame loop. */
                    goto exit;
                }
            } else {
                if (slide != 0) {
                    slide -= speed;
                    if (slide < 0) {
                        slide = 0;
                    }
                }
                if (speed >= 3) {
                    speed -= 2;
                }
            }
        }

        /* Bounce after the Status screen moves the panel: steps 1-6 at the top (y 0x1A), then
         * 7-0xB at the bottom (y 0xAA) and back to 0, which the screen waits for. */
        if (g_unit_view_summary_panel_rect.y == 0x1A) {
            if (g_unit_view_stats_bounce_step == 0) {
                g_unit_view_stats_bounce_step = 1;
            } else if (g_unit_view_stats_bounce_step < 6) {
                g_unit_view_stats_bounce_step = g_unit_view_stats_bounce_step + 1;
            }
        }
        if (g_unit_view_summary_panel_rect.y == 0xAA) {
            if (g_unit_view_stats_bounce_step == 6) {
                g_unit_view_stats_bounce_step = 7;
            } else if (g_unit_view_stats_bounce_step >= 7 && g_unit_view_stats_bounce_step < 0xB) {
                g_unit_view_stats_bounce_step = g_unit_view_stats_bounce_step + 1;
            } else if (g_unit_view_stats_bounce_step == 0xB) {
                g_unit_view_stats_bounce_step = 0;
            }
        }
        if (g_unit_view_stats_bounce_step != 0) {
            frame_record->offset_a.y = value + g_unit_view_stats_bounce_offsets[g_unit_view_stats_bounce_step];
        } else {
            frame_record->offset_a.y = value;
        }

        frame_record->offset_a.x = slide - 0x80;
        SetDrawOffset(&frame_record->draw_offset_a, (u16*)&frame_record->offset_a);
        frame_record->offset_b.x = -0x80;
        frame_record->offset_b.y = value;
        SetDrawOffset(&frame_record->draw_offset_b, (u16*)&frame_record->offset_b);
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_offset_b);

        /* `value` becomes the dim flag: set while the Status screen's status list runs in the
         * slot before the screen. */

        value = thread_find_running_by_task(THREAD_TASK_STATUS_SCREEN);
        if (value != 0 && thread_is_running_asm(value - 1) == 0) {
            value = 0;
        }
        if (value != 0) {
            setRGB0(&frame_record->portrait, 0x40, 0x40, 0x60);
        } else {
            setRGB0(&frame_record->portrait, 0x80, 0x80, 0x80);
        }
        SetSemiTrans(&frame_record->portrait, 1);
        gfx_draw_or_append_gpu_primitive(&frame_record->portrait);
        for (i = 3; i >= 0; i--) {
            if (value != 0) {
                frame_record->sprites[i].clut = MENU_CLUT_DIMMED;
            } else {
                frame_record->sprites[i].clut = MENU_CLUT_NORMAL;
            }
            gfx_draw_or_append_gpu_primitive(&frame_record->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_mode_1);
        for (i = 4; i < 7; i++) {
            if (value != 0) {
                frame_record->sprites[i].clut = MENU_CLUT_DIMMED;
            } else {
                frame_record->sprites[i].clut = MENU_CLUT_NORMAL;
            }
            gfx_draw_or_append_gpu_primitive(&frame_record->sprites[i]);
        }

        gfx_draw_or_append_gpu_primitive(&frame_record->draw_mode_0);
        if (value != 0) {
            menu_init_primitive_colors_dimmed(frame_record->palette);
        } else {
            menu_init_primitive_colors_normal(frame_record->palette);
        }
        status_screen_submit_numbers_frame(frame_record->palette);
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_offset_a);
    }
exit:
    thread_yield();
    gfx_free_texture_rect(&rects[0]);
    gfx_free_texture_rect(&rects[1]);
    gfx_free_texture_rect(&rects[2]);
    gfx_free_texture_rect(&rects[3]);
    thread_exit_current();
}

/* Fill the texture coordinates, CLUT and size of a 16x16 item icon sprite. Icons sit 15 to a
 * row from v = 0x20 of the item icon page, and the item's palette picks one of 8 CLUTs per
 * CLUT row from the item icon CLUT base. */
void gfx_set_item_icon_sprite(SPRT* sprite, s32 item_id) {
    item_data_t* item = item_get_data(item_id);
    s32 sprite_id;
    s32 palette;
    s32 column;

    sprite_id = item->sprite_id;
    palette = item->palette;
    column = sprite_id % 15;
    sprite->u0 = column << 4;
    sprite->v0 = ((sprite_id / 15) << 4) + 0x20;
    sprite->clut
        = GetClut(g_gfx_item_icon_clut_base_x + ((palette % 8) << 4), g_gfx_item_icon_clut_base_y + palette / 8);
    sprite->w = 16;
    sprite->h = 16;
}

#ifdef FFT_BATTLE
/*
 * Set the texture page, CLUT and UVs of a unit's portrait quad.
 *
 * The unit whose ID equals g_portrait_special_unit_id uses the fixed 0x380/0x100 page; others
 * select a 32-line band of the spritesheet VRAM slot's page and the CLUT of
 * BATTLE_MAP_CLUT_ROW_UNITS_SECOND indexed by the low nibble of the unit runtime record ID.
 */
void portrait_build_unit_poly(POLY_FT4* poly, s32 battle_id) {
    s32 vram_slot;
    s32 runtime_id;
    u8 v;

    if (unit_get_stats(battle_id)->unit_id == g_portrait_special_unit_id) {
        setUV4(poly, 0xe0, 0xcf, 0xff, 0xcf, 0xe0, 0xff, 0xff, 0xff);
        poly->tpage = GetTPage(0, 0, 0x380, 0x100);
        poly->clut = GetClut(0x40, 0x1fe);
    } else {
        vram_slot = sprite_sheet_get_slot_by_battle_id(battle_id);
        runtime_id = unit_get_runtime_id_by_battle_id(battle_id);
        v = vram_slot << 5;
        if (vram_slot >= 8) {
            poly->tpage = GetTPage(0, 0, 0x380, 0x120);
            v = 0;
        } else {
            poly->tpage = GetTPage(0, 0, 0x340, 0x100);
        }
        setUV4(poly, 0x80, v, 0x80, v + 0x20, 0x50, v, 0x50, v + 0x20);
        poly->v1--;
        poly->v3--;
        poly->clut = GetClut((runtime_id & 0xf) * 16, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_UNITS_SECOND);
    }
}
#endif

#ifdef FFT_WORLD
/*
 * Set the texture window, texture page and CLUT of a 32x48 portrait quad.
 *
 * The low byte of `code` is the portrait index: 40 portraits per texture
 * page, laid out 8 across and 5 down, with four 16-color CLUTs per CLUT row.
 * Indices 0x3F-0x52 instead use a 6-row CLUT block at y 0x1FA whose column
 * group comes from bits 10-11 of `code`.
 *
 * `code` is reused for the cell number that selects the CLUT.
 */
void portrait_build_unit_poly(POLY_FT4* poly, s32 code) {
    s32 index = code & 0xFF;
    s32 palette = (code & 0xC00) >> 10;
    s32 page = index / 40;
    s32 cell = index % 40;
    s32 u;
    s32 v;
    s32 clut_x;
    s32 clut_y;
    s32 page_x;

    code = cell;
    u = (cell % 8) << 5;
    v = (cell / 8) * 48;
    clut_x = (code % 4) << 4;
    clut_y = code / 4;
    page_x = page << 6;
    setUV4(poly, u, v, u + 31, v, u, v + 48, u + 31, v + 48);
    poly->tpage = GetTPage(0, 0, page_x + 0x680, 0x100);
    if (index >= 0x3F && index <= 0x52) {
        s32 x;

        index -= 0x3F;
        x = (palette << 4) + 0x680;
        clut_y = index % 6;
        clut_x = ((index / 6) << 6) + x;
        poly->clut = GetClut(clut_x, clut_y + 0x1FA);
    } else {
        s32 x;

        x = clut_x + 0x680;
        poly->clut = GetClut(page_x + x, clut_y + 0x1F0);
    }
}
#endif

/* The Status screen's numeric side panel: draw its 15 numbers into three VRAM images
 * once, then draw the double-buffered frame and 18 sprites every frame, dimmed while a
 * grandchild menu runs, until thread parameter 3 is set. */
void status_screen_numbers_thread(void) {
    status_screen_numbers_record_t* records;
    status_screen_numbers_record_t* record = g_status_screen_numbers_panel_records;
    menu_unit_stat_number_images_t* number_images;
    s32 i;
    s32 frame;

    records = record;
    thread_set_current_task_id(THREAD_TASK_NUMERIC_DISPLAY_PANEL);
    gfx_set_draw_mode_for_texture_page(&record->mode0, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&record->mode1, MENU_TEXTURE_PAGE_TEXT_IMAGES);
    menu_init_scaled_draw_area_packets(&record->draw_areas);
    number_images = menu_alloc_memory(sizeof(*number_images));
    menu_clear_render_buffer(number_images, sizeof(*number_images));
    g_menu_window_text_pen.stride = 0x14;
    menu_set_text_origin(0, 0);
    menu_draw_numeric_display_entries(
        number_images->mobility, g_status_screen_numbers_entries, &g_menu_window_text_pen, 3);
    g_menu_window_text_pen.stride = 0x40;
    menu_set_text_origin(0, 0);
    menu_draw_numeric_display_entries(
        number_images->weapons, &g_status_screen_numbers_entries[3], &g_menu_window_text_pen, 4);
    g_menu_window_text_pen.stride = 0x64;
    menu_set_text_origin(0, 0);
    menu_draw_numeric_display_entries(
        number_images->combat, &g_status_screen_numbers_entries[7], &g_menu_window_text_pen, 8);
    LoadImage(&g_status_screen_numbers_text_rect_a, (u32*)number_images->mobility);
    LoadImage(&g_status_screen_numbers_text_rect_b, (u32*)number_images->weapons);
    LoadImage(&g_status_screen_numbers_text_rect_c, (u32*)number_images->combat);
    thread_wait_frames(1);
    menu_free_memory(number_images);
    for (i = 0; i < 18; i++) {
        menu_init_sprt(&record->sprites[i]);
        gfx_set_image_primitive_layout(&record->sprites[i], &g_menu_texture_origin, &g_status_screen_numbers_frame_rect,
            &g_status_screen_numbers_image_params[i]);
    }
    mem_copy_bytes_nonempty(&record[1], record, sizeof(*record));
    record[0].palette = &g_status_screen_numbers_palettes[0].primitives;
    record[1].palette = &g_status_screen_numbers_palettes[1].primitives;
    status_screen_init_numbers_frame(&g_status_screen_numbers_frame_rect, record[0].palette);
    status_screen_init_numbers_frame(&g_status_screen_numbers_frame_rect, record[1].palette);
    for (frame = 0;; frame++) {
        thread_yield();
        record = &records[frame & 1];
        gfx_build_scaled_draw_area_pair_swapped(
            &record->draw_areas, &g_status_screen_numbers_panel_texture_window, frame, g_gfx_screen_polarity);
        if (thread_get_current_parameter_3() != 0) {
            break;
        }
        if (thread_is_running_asm(THREAD_SLOT_MENU_GRANDCHILD) != 0) {
            for (i = 17; i >= 0; i--) {
                record->sprites[i].clut = MENU_CLUT_DIMMED;
            }
            record->sprites[10].clut = 0x7EBC;
            menu_init_primitive_colors_dimmed(record->palette);
        } else {
            for (i = 17; i >= 0; i--) {
                record->sprites[i].clut = MENU_CLUT_NORMAL;
            }
            record->sprites[10].clut = 0x7D7C;
            record->sprites[14].clut = 0x7FFC;
            record->sprites[15].clut = 0x7FFC;
            record->sprites[16].clut = 0x7FFC;
            menu_init_primitive_colors_normal(record->palette);
        }
        gfx_draw_or_append_gpu_primitive(&record->draw_areas.areas[1]);
        for (i = 0; i < 14; i++) {
            gfx_draw_or_append_gpu_primitive(&record->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&record->sprites[17]);
        gfx_draw_or_append_gpu_primitive(&record->mode0);
        for (; i < 17; i++) {
            gfx_draw_or_append_gpu_primitive(&record->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&record->mode1);
        status_screen_submit_numbers_frame(record->palette);
        gfx_draw_or_append_gpu_primitive(&record->draw_areas.areas[0]);
    }
    thread_yield();
    thread_exit_current();
}

/* The unit status screen's equipment and ability panel (task STATUS_SCREEN). It moves the
 * stats and summary panels to the top, draws the five equipment names with their item icons
 * and the five ability names into VRAM once, starts the numeric side panel, then redraws every
 * frame until Cross or a stop request. Circle opens the unit's status list. On exit the panels
 * return to the bottom with their bounce, the tile info panel restarts and the idle action menu
 * is rebuilt. */
void status_screen_thread(void) {
    /* The frame reserves 0x100 bytes for the first copy's line-box
     * primitives, the stride of the palette records (WORLD 0x80195ed0). */
    menu_palette_slot_t palette;
    status_screen_frame_t* records;
    status_screen_frame_t* frame_record;
    LINE_F2* line;
    u8* buffer;
    /* One variable for the label text id and, in the frame loop, whether the
     * status thread runs: the target keeps both in one callee-saved register. */
    s32 value;
    s32 frame;
    s32 i;

    status_screen_open_mounted_unit();
    thread_set_current_task_id(THREAD_TASK_STATUS_SCREEN);
    menu_enter_status_screen_selection();
    g_menu_overlay_state = 1;
    g_menu_panel_fade_mode = MENU_PANEL_FADE_OUT;
    thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
    command_record_unit_selection();
    menu_set_modal_open();
    /* The hovered-unit stats thread counts its frames; wait until it has drawn two. */
    do {
        thread_yield();
    } while (g_unit_view_hovered_stats_frame_count < 2);
    menu_clear_modal_open();

    frame_record = g_status_screen_frames;
    records = frame_record;
    menu_init_scaled_draw_area_packets(&frame_record->draw_areas);
    /* Move the hovered-unit and summary panels to the top while the editor shows. */
    g_unit_view_hovered_stats_rect.y = 0x1A;
    g_unit_view_summary_panel_rect.y = 0x1A;
    gfx_set_draw_mode_for_texture_page(&frame_record->frame_draw_mode, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&frame_record->text_draw_mode, MENU_TEXTURE_PAGE_TEXT_IMAGES);
    gfx_set_draw_mode_for_texture_page(&frame_record->image_draw_mode, MENU_TEXTURE_PAGE_ITEM_ICONS);

    SetTile(&frame_record->tiles[0]);
    SetSemiTrans(&frame_record->tiles[0], 1);
    setRGB0(&frame_record->tiles[0], 0x30, 0x30, 0x30);
    frame_record->tiles[0].w = 0x10;
    frame_record->tiles[0].h = 0x5A;
    frame_record->tiles[0].x0 = g_status_screen_panel_rect.x;
    frame_record->tiles[0].y0 = g_status_screen_panel_rect.y + 1;
    mem_copy_bytes_nonempty(&frame_record->tiles[1], &frame_record->tiles[0], sizeof(TILE));
    frame_record->tiles[0].x0 += 0x12;
    frame_record->tiles[1].x0 += 0x7C;

    line = frame_record->lines;
    for (i = 0; i < 8 * MENU_STATUS_SCREEN_LINE_SIZE; i += MENU_STATUS_SCREEN_LINE_SIZE) {
        SetLineF2(line);
        SetSemiTrans(line, 1);
        setRGB0(line, g_status_screen_line_colors[i + MENU_STATUS_SCREEN_LINE_R],
            g_status_screen_line_colors[i + MENU_STATUS_SCREEN_LINE_G],
            g_status_screen_line_colors[i + MENU_STATUS_SCREEN_LINE_B]);
        setXY2(line, g_status_screen_line_colors[i + MENU_STATUS_SCREEN_LINE_X0] + g_status_screen_panel_rect.x,
            g_status_screen_line_colors[i + MENU_STATUS_SCREEN_LINE_Y0] + g_status_screen_panel_rect.y,
            g_status_screen_line_colors[i + MENU_STATUS_SCREEN_LINE_X1] + g_status_screen_panel_rect.x,
            g_status_screen_line_colors[i + MENU_STATUS_SCREEN_LINE_Y1] + g_status_screen_panel_rect.y);
        line++;
    }

    menu_init_sprite_array(&frame_record->sprites[0], 0xC, 0x7D7C);
    menu_init_sprite_array(&frame_record->sprites[12], 7, MENU_CLUT_NORMAL);

    /* Each arm copies all four rows itself; the compiler cross-jumps the
     * common tails, leaving only the first call's table loads in the arms. */
    if (g_unit_panel_selected_unit_fields.state.two_hands != 0) {
        mem_copy_bytes_nonempty(
            &g_status_screen_sprite_image_params_a[2], g_status_screen_row_image_params_b, 2 * sizeof(image_layout_t));
        mem_copy_bytes_nonempty(&g_status_screen_sprite_image_params_a[14], &g_status_screen_row_image_params_b[2],
            2 * sizeof(image_layout_t));
        mem_copy_bytes_nonempty(
            &g_status_screen_sprite_image_params_b[2], g_status_screen_row_image_params_b, 2 * sizeof(image_layout_t));
        mem_copy_bytes_nonempty(&g_status_screen_sprite_image_params_b[14], &g_status_screen_row_image_params_b[2],
            2 * sizeof(image_layout_t));
    } else {
        mem_copy_bytes_nonempty(
            &g_status_screen_sprite_image_params_a[2], g_status_screen_row_image_params_a, 2 * sizeof(image_layout_t));
        mem_copy_bytes_nonempty(&g_status_screen_sprite_image_params_a[14], &g_status_screen_row_image_params_a[2],
            2 * sizeof(image_layout_t));
        mem_copy_bytes_nonempty(
            &g_status_screen_sprite_image_params_b[2], g_status_screen_row_image_params_a, 2 * sizeof(image_layout_t));
        mem_copy_bytes_nonempty(&g_status_screen_sprite_image_params_b[14], &g_status_screen_row_image_params_a[2],
            2 * sizeof(image_layout_t));
    }

    /* These sprite and rectangle records share the fields read by the image loader. */
    if (g_unit_panel_selected_unit_fields.state.generic_monster == 0) {
        for (i = 0; i < 19; i++) {
            gfx_set_image_primitive_layout(&frame_record->sprites[i], &g_menu_texture_origin,
                &g_status_screen_panel_rect, &g_status_screen_sprite_image_params_a[i]);
        }
    } else {
        for (i = 0; i < 19; i++) {
            gfx_set_image_primitive_layout(&frame_record->sprites[i], &g_menu_texture_origin,
                &g_status_screen_panel_rect, &g_status_screen_sprite_image_params_b[i]);
        }
    }

    menu_init_sprite_array(&frame_record->sprites[19], 5, 0x7CFC);
    for (i = 0; i < 5; i++) {
        gfx_set_image_primitive_layout(&frame_record->sprites[19 + i], &g_gfx_item_icon_image_location,
            &g_status_screen_panel_rect, &g_status_screen_item_icon_image_params[i]);
        gfx_set_item_icon_sprite(&frame_record->sprites[19 + i], g_unit_panel_selected_unit_fields.state.equipment[i]);
    }

    buffer = menu_alloc_buffer(0xC80);
    menu_clear_render_buffer(buffer, 0xC80);
    g_menu_window_text_pen.stride = 0x50;
    menu_set_text_origin(0, 0);
    for (i = 0; i < 5; i++) {
        value = g_unit_panel_selected_unit_fields.state.equipment[i] + TEXT_ID_ITEM_NAME_BASE;
        if ((u16)g_unit_panel_selected_unit_fields.state.equipment[i] == 0xFFFF) {
            g_menu_window_text_pen.rect.y += 0x10;
        } else {
            menu_display_text_entry(value, buffer, &g_menu_window_text_pen);
            g_menu_window_text_pen.rect.y += 0x10;
        }
    }
    LoadImage(&g_status_screen_text_rect_a, (u32*)buffer);
    /* Give the upload a frame before the buffer is cleared for the ability names. */
    thread_yield();

    menu_clear_render_buffer(buffer, 0xC80);
    g_menu_window_text_pen.stride = 0x50;
    menu_set_text_origin(0, 0);
    for (i = 0; i < 2; i++) {
        value = g_unit_panel_selected_unit_fields.state.ability_ids[i] + TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
        if (g_unit_panel_selected_unit_fields.state.generic_monster != 0) {
            value = g_unit_panel_selected_unit_fields.state.ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE;
        }
        if ((u16)g_unit_panel_selected_unit_fields.state.ability_ids[i] == MENU_STATUS_PANEL_LABEL_NONE) {
            g_menu_window_text_pen.rect.y += 0x10;
        } else {
            menu_display_text_entry(value, buffer, &g_menu_window_text_pen);
            g_menu_window_text_pen.rect.y += 0x10;
        }
    }
    for (i = 2; i < 5; i++) {
        value = g_unit_panel_selected_unit_fields.state.ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE;
        if ((u16)g_unit_panel_selected_unit_fields.state.ability_ids[i] == MENU_STATUS_PANEL_LABEL_NONE) {
            g_menu_window_text_pen.rect.y += 0x10;
        } else {
            menu_display_text_entry(value, buffer, &g_menu_window_text_pen);
            g_menu_window_text_pen.rect.y += 0x10;
        }
    }

    /* A monster's empty ability rows move their sprites off screen. */
    if (g_unit_panel_selected_unit_fields.state.generic_monster != 0) {
        for (i = 0; i < 5; i++) {
            if ((u16)g_unit_panel_selected_unit_fields.state.ability_ids[i] == MENU_STATUS_PANEL_LABEL_NONE) {
                frame_record->sprites[7 + i].x0 -= 0x200;
            }
        }
    }

    LoadImage(&g_status_screen_text_rect_b, (u32*)buffer);
    thread_yield();
    menu_free_memory(buffer);

    mem_copy_bytes_nonempty(&records[1], frame_record, sizeof(status_screen_frame_t));
    records[0].palette = &palette.primitives;
    records[1].palette = &g_status_screen_palette;
    status_screen_init_numbers_frame(&g_status_screen_panel_rect, records[0].palette);
    status_screen_init_numbers_frame(&g_status_screen_panel_rect, records[1].palette);

    thread_start(THREAD_SLOT_TILE_INFO_PANEL, status_screen_numbers_thread);
    thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 0);
    thread_yield();

    frame = 0;
    for (;;) {
        thread_yield();
        frame_record = &records[frame & 1];
        gfx_build_scaled_draw_area_pair_swapped(
            &frame_record->draw_areas, &g_status_screen_cursor_rect, frame, g_gfx_screen_polarity);
        if (g_menu_input_lock_mode == 0 && g_help_open == 0 && thread_get_current_parameter_3() != 0) {
            break;
        }

        /* Dim while the status list started below runs in the slot before this thread. */
        value = thread_is_running_asm(g_thread_current_id - 1);
        for (i = 0; i < 19; i++) {
            frame_record->sprites[i].clut = g_status_screen_sprite_clut_pairs[i][value];
        }
        for (i = 0; i < 5; i++) {
            if (value != 0) {
                SetShadeTex(&frame_record->sprites[19 + i], 0);
                /* Spelled from sprites[i]: the target's store pointer walks
                 * frame_record + i * 0x14 with the item-icon bias in the
                 * displacement; sprites[19 + i] adds the base per store. */
                (&frame_record->sprites[i])[19].r0 = 0x40;
                (&frame_record->sprites[i])[19].g0 = 0x40;
                (&frame_record->sprites[i])[19].b0 = 0x80;
            } else {
                SetShadeTex(&frame_record->sprites[19 + i], 1);
            }
        }
        if (value == 0) {
            menu_init_primitive_colors_normal(frame_record->palette);
        } else {
            menu_init_primitive_colors_dimmed(frame_record->palette);
        }

        if (menu_can_accept_input() != 0) {
            if (g_script_event_input & PSX_PAD_CROSS) {
                thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 1);
                break;
            }
            if ((g_script_event_input & PSX_PAD_CIRCLE) && thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) == 0
                && (unit_has_any_current_status(g_unit_panel_selected_unit.battle_id) != 0
                    || (g_unit_panel_selected_unit_fields.state.generic_monster == 0
                        && g_unit_panel_selected_unit_status.team_kind != 1))) {
                thread_start(g_thread_current_id - 1, status_screen_build_status_list);
                thread_set_parameters(
                    g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_STATUS_LIST], 0, 0);
            }
        }

        gfx_draw_or_append_gpu_primitive(&frame_record->draw_areas.areas[1]);
        for (i = 0; i < 12; i++) {
            gfx_draw_or_append_gpu_primitive(&frame_record->sprites[i]);
        }
        for (i = 14; i < 19; i++) {
            gfx_draw_or_append_gpu_primitive(&frame_record->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&frame_record->frame_draw_mode);
        gfx_draw_or_append_gpu_primitive(&frame_record->sprites[12]);
        gfx_draw_or_append_gpu_primitive(&frame_record->sprites[13]);
        gfx_draw_or_append_gpu_primitive(&frame_record->text_draw_mode);
        for (i = 0; i < 5; i++) {
            if ((u16)g_unit_panel_selected_unit_fields.state.equipment[i] != 0xFFFF) {
                gfx_draw_or_append_gpu_primitive(&frame_record->sprites[19 + i]);
            }
        }
        gfx_draw_or_append_gpu_primitive(&frame_record->image_draw_mode);
        gfx_draw_or_append_gpu_primitive(&frame_record->tiles[0]);
        gfx_draw_or_append_gpu_primitive(&frame_record->tiles[1]);
        for (i = 0; i < 8; i++) {
            gfx_draw_or_append_gpu_primitive(&frame_record->lines[i]);
        }
        frame++;
        status_screen_submit_numbers_frame(frame_record->palette);
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_areas.areas[0]);
    }

    /* Return the panels to the bottom; step 6 starts the summary panel's bounce there,
     * and it resets the step to 0 when the bounce ends. */
    g_unit_view_hovered_stats_rect.y = 0xAA;
    g_unit_view_summary_panel_rect.y = 0xAA;
    g_menu_panel_fade_mode = MENU_PANEL_FADE_IN;
    g_unit_view_stats_bounce_step = 6;
    sound_queue_sfx_cancel();
    do {
        thread_yield();
    } while (g_unit_view_stats_bounce_step != 0);

    thread_start(THREAD_SLOT_TILE_INFO_PANEL, unit_view_tile_info_thread);
    thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 0);
    g_thread_contexts[THREAD_SLOT_TILE_INFO_PANEL].task_id = THREAD_TASK_TILE_INFO_PANEL;
    g_menu_overlay_state = 0;
    menu_leave_status_screen_selection();
    command_open_menu(g_command_current_id);
    thread_exit_current();
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * status_screen_thread and menu_can_accept_input. Nothing on the
 * disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void menu_noop_3(void) {
}

/* Returns 1 when a menu thread may act on input: no previous event thread
 * running, no third thread parameter (a stop request), not in event mode, no
 * help menu open, and g_menu_input_block_frames clear. In WORLD, a help box
 * (box type 0x30) in the text or message slot blocks input first and arms a
 * five-frame block. */
s32 menu_can_accept_input(void) {
#ifdef FFT_WORLD
    if ((thread_is_running_asm(THREAD_SCREEN_SLOT_TEXT)
            && (g_thread_contexts[THREAD_SCREEN_SLOT_TEXT].function_parameter_1 & 0x70) == 0x30)
        || (thread_is_running_asm(THREAD_SCREEN_SLOT_MESSAGE)
            && (g_thread_contexts[THREAD_SCREEN_SLOT_MESSAGE].function_parameter_1 & 0x70) == 0x30)) {
        g_menu_input_block_frames = 5;
        return 0;
    }
#endif
    if (thread_is_previous_running() != 0 || thread_get_current_parameter_3() != 0 || g_menu_input_lock_mode != 0
        || g_help_open != 0 || g_menu_input_block_frames != 0) {
        return 0;
    }
    return 1;
}

/* Act on Circle for menu row row_index: start the row's menu thread one slot below the current
 * one, clear the selection, or for a negative action -n ask n menu threads from the current one up
 * to close. Stores the row as the entry's result and plays the confirm sound unless the action is
 * silent; in tutorial input mode rows disabled by EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS do
 * nothing. */
void menu_handle_action(menu_entry_t* entry, s32 row_index) {
    s32 silent;
    s32 target;
    s32 i;
    s32 j;
    s32 thread_id;
    s32 menu_thread_id;
    s16* row_actions;

    if (g_menu_input_mode == MENU_INPUT_TUTORIAL
        && ((script_get_variable(EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS) >> row_index) & 1)) {
        return;
    }
    silent = 0;
    if ((g_script_event_input & PSX_PAD_CIRCLE) && menu_can_accept_input() != 0) {
        if (g_thread_contexts[g_thread_current_id].task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] == 0) {
            g_thread_contexts[g_thread_current_id].task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] = 2;
            g_thread_contexts[g_thread_current_id].task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] = 0;
        }
        row_actions = entry->row_actions;
        target = ((s32)row_actions >= -0xf && (s32)row_actions <= -1) ? (s32)row_actions : row_actions[row_index];
        if (target >= 0) {
            if (target & MENU_ROW_ACTION_SILENT) {
                silent = 1;
                /* Raises silent's allocation priority; otherwise $s6-$s8 rotate. */
                __asm__("" : : "r"(silent), "r"(silent));
                target -= MENU_ROW_ACTION_SILENT;
            }
        }
        if (target >= 0) {
            if (target < 0x100) {
                thread_start(g_thread_current_id - 1, g_menu_thread_menu_data[target].thread_entry);
                thread_set_parameters(g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[target], 0, 0);
            } else if (target == MENU_ROW_ACTION_CLEAR_SELECTION) {
                row_index = -1;
            }
        } else {
            target = -target;
            for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
                if (g_thread_contexts[i].task_id == THREAD_TASK_RESUME) {
                    g_thread_contexts[i].task_id = THREAD_TASK_STOP_REQUEST;
                }
            }
            /* Remember the rows of the closing battle menus (the menu slot and its
             * children below it) as the pending selection. */
            for (i = 0; i < target; i++) {
                thread_id = i + g_thread_current_id;
                g_thread_contexts[thread_id].function_parameter_3 = 1;
                if (thread_id == THREAD_SLOT_MENU) {
                    for (j = 0; j < target; j++) {
                        menu_thread_id = THREAD_SLOT_MENU - j;
                        if (thread_is_running_asm(menu_thread_id) != 0) {
                            g_command_pending_selection[j]
                                = ((menu_entry_t*)g_thread_contexts[menu_thread_id].function_parameter_1)
                                      ->selected_index;
                        }
                    }
                }
            }
        }
        g_script_event_input = 0;
        *menu_entry_result(entry) = row_index;
        if (g_sound_suppressed == 0 && silent == 0) {
            sound_queue_sfx_confirm();
        }
    }
}

/* While the menu may accept input, move a row cursor with Up and Down, wrapping
 * between row 0 and the entry's max_row_index, and play the cursor sound. */
void menu_step_wrapping_cursor_on_scroll_buttons(menu_entry_t* ctx, s32* sel) {
    s32 selection;
    s32 max;
    s32 cur;

    if (menu_can_accept_input() != 0) {
        if (g_script_event_input & PSX_PAD_UP) {
            selection = *sel;
            if (selection == 0) {
                selection = ctx->max_row_index;
            } else {
                selection = selection - 1;
            }
            *sel = selection;
            sound_queue_sfx_cursor_move();
        }
        if (g_script_event_input & PSX_PAD_DOWN) {
            max = ctx->max_row_index;
            cur = *sel;
            if (cur == max) {
                *sel = 0;
            } else {
                *sel = cur + 1;
            }
            sound_queue_sfx_cursor_move();
        }
    }
}

/* On the cancel button, flag the context's thread group as finished, turn every
 * THREAD_TASK_RESUME thread into a stop request and report -1 to the caller. Closing the
 * battle menu slot also forgets the pending selection. Menu input is then held off for 5
 * frames. */
void menu_handle_cancel_input(menu_entry_t* context) {
    s32 i;
    s32 j;
    s32 thread_id;
    s32 count;

    if (!(g_script_event_input & PSX_PAD_CROSS)) {
        return;
    }
    if (menu_can_accept_input() == 0) {
        return;
    }
    count = context->thread_count;
    if (count != -1) {
        for (i = 0; i < count; i++) {
            thread_id = i + g_thread_current_id;
            g_thread_contexts[thread_id].function_parameter_3 = 1;
            if (thread_id == THREAD_SLOT_MENU) {
                for (j = 3; j >= 0; j--) {
                    g_command_pending_selection[j] = -1;
                }
            }
        }
        for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
            if (g_thread_contexts[i].task_id == THREAD_TASK_RESUME) {
                g_thread_contexts[i].task_id = THREAD_TASK_STOP_REQUEST;
            }
        }
    }
    g_script_event_input = 0;
    *menu_entry_result(context) = -1;
    sound_queue_sfx_cancel();
    g_script_input_suppression_frames = 5;
}

/* Suppress menu input while another thread has a stop request and report
 * completion: 1 once no other running thread has a stop request and this
 * thread's parameter 3 is set. In WORLD, SELECT first opens the entry's
 * auxiliary text thread when its table selector is nonzero. */
s32 menu_should_close_thread(u32* input) {
#ifdef FFT_WORLD
    menu_entry_t* parameter;
#endif
    s32 i;

#ifdef FFT_WORLD
    if (g_thread_contexts[THREAD_SCREEN_SLOT_TEXT].is_running == 0 && menu_can_accept_input() != 0
        && (*input & PSX_PAD_SELECT)) {
        parameter = (menu_entry_t*)thread_get_current_parameter_1();
        if (parameter->select_text_table != 0) {
            if (g_script_input_suppression_frames == 0) {
                thread_start(THREAD_SCREEN_SLOT_TEXT, screen_help_text_thread);
                thread_set_parameters(THREAD_SCREEN_SLOT_TEXT, (s32)parameter, 0, 0);
                g_menu_input_lock_mode = 1;
            }
            *input = 0;
        }
    }
#endif
    if (g_menu_input_lock_mode != 0 || g_help_open != 0) {
        *input = 0;
        return 0;
    }
    for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
        if (i != g_thread_current_id && thread_is_running_asm(i) != 0
            && g_thread_contexts[i].task_id == THREAD_TASK_STOP_REQUEST) {
            *input = 0;
            break;
        }
    }
    if (i == VARIANT_THREAD_TABLE_SLOT_COUNT && thread_get_current_parameter_3() != 0) {
        return 1;
    }
    return 0;
}

/* Publish the icon strip's text-image upload rectangle to the caller, build
 * the four menu sprites of the icon record, and place the icon sprites that
 * the strip's icon kind selects. Only WORLD has the icon kind 0x10 branch. */
void menu_build_window_sprites(RECT* rect, menu_entry_t* param, menu_icon_record_t* record) {
    s32 kind;
    s32 index;
    /* HImode is load-bearing. `cse` keys its constant equivalence classes by
     * mode, so a u16 `size` is not value-equal to the SImode 0x10 of the
     * `kind == 0x10` test below; that restores the target's separate
     * `li v0,0x10` for the compare and keeps `size` live across the arm's
     * call, which is what parks it in callee-saved s1. `s32` here changes the allocation. */
    u16 size;
    /* The target's frame is 0x40 with its register saves at 0x20 and a 20-byte
     * outgoing-argument area, so `vars` is 8 bytes the code never touches.
     * Those bytes sit above the argument area and below the saves, which is
     * where a declared aggregate lands; an unreferenced one reproduces the
     * frame at zero instructions. Worth the whole prologue and epilogue. */
    u8 unused_scratch[8];

    thread_set_current_task_id(THREAD_TASK_MENU_WINDOW_BUILD);
    rect->x = param->vram_origin.x;
    rect->y = param->vram_origin.y;
    rect->w = param->inner_width >> 2;
    rect->h = param->inner_height;
    record->_unknown_74 = 0;
    record->_unknown_78 = 0;
    menu_init_sprite_array(record->base.sprites, 4, 0x7cfc);
    gfx_set_image_primitive_layout(record->base.sprites, &param->vram_origin, &param->window, &param->text_image);
    SetSemiTrans(&record->base.sprites[1], 0);
    SetSemiTrans(&record->base.sprites[2], 1);
    size = 0x10;
    record->base.sprites[1].w = size;
    record->base.sprites[1].h = size;
    record->base.sprites[2].w = size;
    record->base.sprites[2].h = size;
    record->base.sprites[1].u0 = 0xa8;
    record->base.sprites[1].v0 = 0;
    record->base.sprites[2].u0 = 0xb8;
    record->base.sprites[2].v0 = 0;
    kind = param->header_id;
    /* The range test below reads its own 16-bit copy of the kind. */
    index = (s16)kind;
#ifdef FFT_WORLD
    if (kind == 0x10) {
        const image_layout_t* image_params;

        menu_init_sprite_array(record->icons[0], 1, 0x7cfc);
        record->icons[0]->u0 = 0x30;
        record->icons[0]->v0 = 0x10;
        record->icons[0]->w = size;
        record->icons[0]->h = 8;
        image_params = param->context;
        record->icons[0]->x0 = image_params->source.h + param->window.x + 7;
        record->icons[0]->y0 = param->window.y - 1;
        gfx_set_image_primitive_layout(
            &record->base.sprites[3], &g_menu_texture_origin, &param->window, &g_menu_single_icon_image_params[1]);
    } else
#endif
        if (kind < 5) {
        gfx_set_image_primitive_layout(
            &record->base.sprites[3], &g_menu_texture_origin, &param->window, &g_menu_single_icon_image_params[kind]);
    } else {
        if (index >= 5 && index < 0x10) {
            menu_init_sprite_array(record->icons[0], 1, 0x7cfc);
            menu_init_sprite_array(record->icons[1], 1, 0x7cfc);
            menu_init_sprite_array(record->icons[2], 1, 0x7cfc);
            /* `kind` stays unscaled and the x4 is spelled `<< 2` inline. Written
             * `kind * 4 + n`, fold collapses it to one x48 whose 3-op synth is an
             * instruction short; `(kind << 2) + n` selects the target's 4-op form. */
            kind -= 5;
            gfx_set_image_primitive_layout(&record->base.sprites[3], &g_menu_texture_origin, &param->window,
                &g_menu_icon_strip_image_params[(kind << 2)]);
            gfx_set_image_primitive_layout(record->icons[0], &g_menu_texture_origin, &param->window,
                &g_menu_icon_strip_image_params[(kind << 2) + 1]);
            gfx_set_image_primitive_layout(record->icons[1], &g_menu_texture_origin, &param->window,
                &g_menu_icon_strip_image_params[(kind << 2) + 2]);
            gfx_set_image_primitive_layout(record->icons[2], &g_menu_texture_origin, &param->window,
                &g_menu_icon_strip_image_params[(kind << 2) + 3]);
        } else {
            record->icons[0] = 0;
            record->icons[1] = 0;
            record->icons[2] = 0;
        }
    }
    gfx_set_draw_mode_from_origin(&record->base.first_draw_mode, &param->vram_origin);
    SetDrawMode(&record->base.draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);
}

/* Give a menu sprite or quad the dimmed menu CLUT while the previous thread is running or
 * g_menu_input_lock_mode is 1, else the normal one. */
void menu_select_sprite_clut(void* packet) {
    s32 clut;
    texture_primitive_t* primitive = packet;

    if (thread_is_previous_running() != 0 || g_menu_input_lock_mode == 1) {
        clut = MENU_CLUT_DIMMED;
    } else {
        clut = MENU_CLUT_NORMAL;
    }
    primitive->sprt.clut = clut;
}

/* Select a menu sprite's CLUT as menu_select_icon_cluts does for its fourth
 * sprite: the dimmed one while the previous thread is running or g_menu_input_lock_mode is 1. */
void menu_select_icon_sprite_clut(SPRT* sprite) {
    s32 clut;

    if (thread_is_previous_running() != 0 || g_menu_input_lock_mode == 1) {
        clut = 0x7c7c;
    } else {
        clut = 0x7cbc;
    }
    sprite->clut = clut;
}

/* Choose the CLUTs of the four menu-frame sprites: the dimmed set while the
 * previous thread is running or g_menu_input_lock_mode is 1. */
void menu_select_icon_cluts(menu_icon_sprites_t* frame) {
    if (thread_is_previous_running() != 0 || g_menu_input_lock_mode == 1) {
        frame->sprites[0].clut = MENU_CLUT_DIMMED;
        frame->sprites[1].clut = 0x7dfc;
        frame->sprites[2].clut = 0x7e3c;
        frame->sprites[3].clut = 0x7c7c;
    } else {
        frame->sprites[0].clut = MENU_CLUT_NORMAL;
        frame->sprites[1].clut = 0x7d7c;
        frame->sprites[2].clut = 0x7dbc;
        frame->sprites[3].clut = 0x7cbc;
    }
}

/* Return the cursor's vertical animation offset for the current menu frame.
 *
 * The two signed threshold tables produce the idle and active cursor curves. */
s32 menu_get_cursor_bob_offset(s32 mode) {
    menu_cursor_bob_entry_t* entry;
    s32 counter = g_thread_contexts[g_thread_current_id].task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER];
    s32 threshold;

    if (mode == 0) {
        entry = g_menu_cursor_bob_thresholds;
        counter %= entry->threshold;
    } else if (mode == 1) {
        entry = g_menu_cursor_bob_active_thresholds;
    }

    /* Any other mode leaves entry unset. The first threshold is tested here
     * and again by the loop condition. */
    entry++;
    threshold = entry->threshold;
    if (threshold == 0) {
        return 0;
    }
    while (threshold != 0) {
        if (counter < threshold) {
            return entry->offset;
        }
        entry++;
        threshold = entry->threshold;
    }
    return 0;
}

/* Position the icon-list cursor sprites and animate the highlighted icon.
 *
 * The current thread's cursor bob task words hold the timer and the active
 * flag: the flag restarts at 2 while input is blocked and clears once the
 * timer reaches 27. The cursor sprites follow the selected row. */
void menu_update_icon_cursor_sprites(menu_entry_t* param, menu_icon_sprites_t* record, s32 frame, s32 cursor) {
    thread_t* thread = &g_threads[g_thread_current_id];
    s32 x;
    s32 y;

    if (cursor >= 0) {
        if (g_menu_formation_display_mode == 1) {
            thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] = 2;
            thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] = 0;
        }
        if (thread_is_previous_running() == 0 && g_menu_input_lock_mode == 0) {
            if (thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] != 0
                && thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] >= 0x1B) {
                thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] = 0;
                thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] = 0;
            }
        } else if (thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] == 0) {
            thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] = 2;
            thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] = 0;
        }
        if (thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] != 0) {
            x = menu_get_cursor_bob_offset(1);
        } else {
            x = menu_get_cursor_bob_offset(0);
        }
        thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] += g_script_event_speed;
        x -= 12;
        y = cursor * 16 + 10;
        record->sprites[1].x0 = param->window.x + x;
        record->sprites[1].y0 = param->window.y + y;
        record->sprites[2].x0 = param->window.x + x + 2;
        record->sprites[2].y0 = param->window.y + y + 2;
    }
    if (g_script_event_speed == 2) {
        frame *= 2;
    }
    menu_copy_sprite_position_uv_and_size(param, &record->sprites[0], frame);
    menu_zoom_cursor_frame(param, &record->sprites[0], frame);
}

/* Copy a menu entry's window position and text-image UV and size to its sprite. */
static void menu_copy_sprite_position_uv_and_size(const menu_entry_t* entry, SPRT* sprite, s32 unused_frame) {
    sprite->x0 = entry->window.x;
    sprite->y0 = entry->window.y;
    sprite->u0 = (entry->vram_origin.x & 0x3f) * 4;
    sprite->v0 = entry->vram_origin.y;
    sprite->w = entry->text_image.source.w;
    sprite->h = entry->text_image.source.h;
}

/* Scale the sprite about its center by the percentage of zoom step `step` (0-11; later steps
 * leave it alone), keeping position, texture coordinates and size even. */
void menu_zoom_cursor_frame(const void* unused_source, SPRT* sprite, s32 step) {
    s32 scale;
    s32 offset;
    if (step < 12) {
        scale = g_menu_cursor_zoom_percentages[step];
        offset = sprite->w * scale / 200;
        sprite->x0 = (sprite->w / 2) + sprite->x0 - offset;
        sprite->u0 = (sprite->w / 2) + sprite->u0 - offset;
        sprite->x0 &= ~1;
        sprite->u0 &= ~1;
        offset = sprite->h * scale / 200;
        sprite->y0 = sprite->h / 2 + sprite->y0 - offset;
        sprite->w = sprite->w * scale / 100;
        sprite->v0 = sprite->h / 2 + sprite->v0 - offset;
        sprite->y0 &= ~1;
        sprite->v0 &= ~1;
        sprite->h = sprite->h * scale / 100;
        sprite->w &= 0xfe;
        sprite->h &= 0xfe;
    }
}

/*
 * Build a double-buffered draw-area pair whose rectangle is cropped to a
 * percentage of `geometry` about its center. Sources at x < 0x80 are moved
 * to the right-hand page (x + 0x80, y + 0x78); others are moved left.
 */
void gfx_build_scaled_draw_area_pair_swapped(
    scaled_draw_area_pair_t* packets, const RECT* geometry, s32 step, s32 flag) {
    s32 percent;
    s16 width;
    s16 height;
    s32 offset;

    if (g_script_event_speed == 2) {
        step *= 2;
    }
    if (step >= 12) {
        step = 11;
    }
    percent = g_menu_cursor_zoom_percentages[step];
    mem_copy_bytes_nonempty(&packets->rects[0], geometry, sizeof(*geometry));
    if (packets->rects[0].x < 0x80) {
        packets->rects[0].x += 0x80;
        packets->rects[0].y += 0x78;
    } else {
        packets->rects[0].x -= 0x80;
    }
    width = packets->rects[0].w;
    height = packets->rects[0].h;
    offset = width * percent / 200;
    packets->rects[0].x = width / 2 + packets->rects[0].x - offset;
    offset = height * percent / 200;
    packets->rects[0].y = height / 2 + packets->rects[0].y - offset;
    packets->rects[0].w = (packets->rects[0].w * percent) / 100;
    packets->rects[0].h = (packets->rects[0].h * percent) / 100;
    if (flag != 0) {
        packets->rects[1].y = 0;
    } else {
        packets->rects[1].y = 0xF0;
        packets->rects[0].y += 0xF0;
    }
    SetDrawArea(&packets->areas[0], &packets->rects[0]);
    SetDrawArea(&packets->areas[1], &packets->rects[1]);
}
